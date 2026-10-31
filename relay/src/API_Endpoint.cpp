#include <API_Endpoint.h>
#include <CertificateManager.h>

#include <spdlog/spdlog.h>
#include <openssl/crypto.h>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <array>

bool API_Endpoint::readAccessKey(const QString &path, QString &key, QString &error) {
	QFile file(path);

	if (!file.open(QIODevice::ReadOnly)) {
		error = file.errorString();
		return false;
	}

	const QByteArray contents = file.readAll();

	if (file.error() != QFileDevice::NoError) {
		error = file.errorString();
		return false;
	}

	static const QRegularExpression format(QStringLiteral("^[0-9a-f]{64}$"));
	key = QString::fromLatin1(contents).trimmed();

	if (!format.match(key).hasMatch()) {
		error = QStringLiteral("Invalid API key file");
		return false;
	}

	return true;
}

bool API_Endpoint::prepareAccessKey(const QString &configDirectory, const bool regenerate, QString &key, QString &error,
				    bool &generated) {
	generated = false;
	const QString path = QDir(configDirectory).filePath(QStringLiteral("api-key"));

	if (!regenerate && QFile::exists(path)) {
		return readAccessKey(path, key, error);
	}

	if (!QDir().mkpath(configDirectory)) {
		error = QStringLiteral("Could not create relay configuration directory");
		return false;
	}

	std::array<quint32, 8> randomWords;
	QRandomGenerator::system()->fillRange(randomWords.data(), randomWords.size());

	const QByteArray bytes(reinterpret_cast<const char *>(randomWords.data()), sizeof(randomWords));
	const QByteArray encoded = bytes.toHex();
	QSaveFile file(path);

	if (!file.open(QIODevice::WriteOnly) ||
	    !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
	    file.write(encoded) != encoded.size() || !file.commit()) {
		error = file.errorString();
		return false;
	}

	key = QString::fromLatin1(encoded);
	generated = true;

	return true;
}

API_Endpoint &API_Endpoint::getInstance() {
	static API_Endpoint instance;

	return instance;
}

API_Endpoint::~API_Endpoint() {
	stop();
}

bool API_Endpoint::start(const std::filesystem::path &configDirectory, const uint16_t port) {
	if (m_server) {
		return false;
	}

#ifdef _WIN32
	m_keyPath = QDir(QString::fromStdWString(configDirectory.wstring())).filePath(QStringLiteral("api-key"));
#else
	m_keyPath = QDir(QString::fromStdString(configDirectory.string())).filePath(QStringLiteral("api-key"));
#endif

	const std::filesystem::path keyPath = configDirectory / "key.pem";
	const std::filesystem::path certPath = configDirectory / "cert.pem";

	if ((!CertificateManager::isCertificateValid(certPath) || !CertificateManager::isPrivateKeyValid(keyPath)) &&
	    !CertificateManager::generateCertificate(keyPath, certPath)) {
		spdlog::error("Failed to generate relay API certificate");
		return false;
	}

	m_server = std::make_unique<httplib::SSLServer>(certPath.string().c_str(), keyPath.string().c_str());

	if (!m_server->is_valid()) {
		spdlog::error("Failed to initialize relay API server");
		m_server.reset();
		return false;
	}

	registerEndpoints();

	if (!m_server->bind_to_port("0.0.0.0", port)) {
		spdlog::error("Failed to initialize or bind relay API server on port {}", port);
		m_server.reset();
		return false;
	}

	m_httpThread = std::thread([this] { m_server->listen_after_bind(); });

	return true;
}

void API_Endpoint::registerEndpoints() const {}

bool API_Endpoint::authorize(const httplib::Request &req, httplib::Response &res) const {
	QString key;
	QString error;

	if (!readAccessKey(m_keyPath, key, error)) {
		spdlog::error("Failed to read relay API key: {}", error.toStdString());
		res.status = 503;
		return false;
	}

	const QByteArray expected = key.toLatin1();
	const std::string supplied = req.get_header_value("Authorization");

	if (supplied.size() != static_cast<size_t>(expected.size()) ||
	    CRYPTO_memcmp(supplied.data(), expected.constData(), supplied.size()) != 0) {
		res.status = 401;
		return false;
	}

	return true;
}

void API_Endpoint::stop() {
	if (m_server) {
		m_server->stop();
	}

	if (m_httpThread.joinable()) {
		m_httpThread.join();
	}

	m_server.reset();
}
