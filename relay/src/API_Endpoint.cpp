#include <API_Endpoint.h>
#include <CertificateManager.h>

#include <spdlog/spdlog.h>
#include <openssl/crypto.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QUrl>
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

bool API_Endpoint::postToEventLoop(std::function<void()> operation) {
	QCoreApplication *const application = QCoreApplication::instance();

	if (!application || !operation) {
		return false;
	}

	return QMetaObject::invokeMethod(application, std::move(operation), Qt::QueuedConnection);
}

API_Endpoint::~API_Endpoint() {
	stop();
}

bool API_Endpoint::start(const std::filesystem::path &configDirectory, const uint16_t port) {
	if (m_server) {
		return false;
	}

	const QString configuredHost = qEnvironmentVariable("RELAY_PUBLIC_HOST").trimmed();
	m_publicHost.clear();

	if (!configuredHost.isEmpty()) {
		QHostAddress ipAddress;

		if (ipAddress.setAddress(configuredHost)) {
			m_publicHost = ipAddress.toString().toStdString();
		} else {
			const QUrl publicUrl(QStringLiteral("https://") + configuredHost);

			if (!publicUrl.isValid() || publicUrl.host().isEmpty() || !publicUrl.userInfo().isEmpty() ||
			    publicUrl.port() != -1 || !publicUrl.path().isEmpty() || publicUrl.hasQuery() ||
			    publicUrl.hasFragment()) {
				spdlog::error("RELAY_PUBLIC_HOST must be an IP address or hostname without a port");
				return false;
			}

			m_publicHost = publicUrl.host().toStdString();
		}
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

	const QDir executableDirectory(QCoreApplication::applicationDirPath());
#ifdef _WIN32
	const QString executableSuffix = QStringLiteral(".exe");
#else
	const QString executableSuffix;
#endif
	const QString mediaPath = executableDirectory.filePath(QStringLiteral("mediamtx") + executableSuffix);
	m_goIrlPath = executableDirectory.filePath(QStringLiteral("go-irl") + executableSuffix);
	const QString mediaConfig = QDir(QFileInfo(m_keyPath).absolutePath()).filePath(QStringLiteral("mediamtx.yml"));
	m_mediaManager = std::make_unique<MediamtxManager>(mediaPath, mediaConfig);

	if (m_publicHost.empty()) {
		m_publicIpProcess = std::make_unique<QProcess>();
		m_publicIpTimer = std::make_unique<QTimer>();
		m_publicIpTimer->setInterval(5 * 60 * 1000);
		QObject::connect(m_publicIpTimer.get(), &QTimer::timeout, m_publicIpProcess.get(),
				 [this] { refreshPublicIp(); });
		QObject::connect(
			m_publicIpProcess.get(), &QProcess::finished, m_publicIpProcess.get(),
			[this](const int exitCode, const QProcess::ExitStatus exitStatus) {
				const QByteArray contents = m_publicIpProcess->readAllStandardOutput().trimmed();
				QHostAddress address;

				if (exitStatus == QProcess::NormalExit && exitCode == 0 &&
				    address.setAddress(QString::fromLatin1(contents)) &&
				    address.protocol() == QAbstractSocket::IPv4Protocol) {
					const std::string publicHost = address.toString().toStdString();
					const std::lock_guard<std::mutex> lock(m_feedsMutex);
					m_publicHost = publicHost;

					for (auto &[streamKey, feed] : m_feeds) {
						feed.link = MediamtxManager::publishLink(
								    streamKey, feedProtocol(feed.protocol), publicHost)
								    .toStdString();
					}
				} else {
					spdlog::warn(
						"Could not discover public IPv4 address; using the last available host");
				}
			});
		QObject::connect(
			m_publicIpProcess.get(), &QProcess::errorOccurred, m_publicIpProcess.get(),
			[](const QProcess::ProcessError error) {
				if (error == QProcess::FailedToStart) {
					spdlog::warn(
						"Could not start curl for public IP discovery; using the last available host");
				}
			});
		m_publicIpTimer->start();
		refreshPublicIp();
	}

	m_httpThread = std::thread([this] { m_server->listen_after_bind(); });

	return true;
}

void API_Endpoint::refreshPublicIp() {
	if (!m_publicIpProcess || m_publicIpProcess->state() != QProcess::NotRunning) {
		return;
	}

	m_publicIpProcess->start(QStringLiteral("curl"),
				 {QStringLiteral("--silent"), QStringLiteral("--fail"), QStringLiteral("--max-time"),
				  QStringLiteral("5"), QStringLiteral("https://api.ipify.org")});
}

bool API_Endpoint::parseFeed(const httplib::Request &req, std::string &protocol, std::string &streamKey) {
	QJsonParseError error;
	const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(req.body), &error);

	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return false;
	}

	const QJsonObject object = document.object();
	const QJsonValue protocolValue = object.value(QStringLiteral("protocol"));
	const QJsonValue keyValue = object.value(QStringLiteral("streamKey"));

	if (!protocolValue.isString() || !keyValue.isString() || keyValue.toString().trimmed().isEmpty()) {
		return false;
	}

	const QString normalizedProtocol = protocolValue.toString().trimmed().toUpper();
	static const QStringList supportedProtocols{QStringLiteral("MOQ"),    QStringLiteral("SRT"),
						    QStringLiteral("WEBRTC"), QStringLiteral("RTSP"),
						    QStringLiteral("RTMP"),   QStringLiteral("SRTLA")};

	if (!supportedProtocols.contains(normalizedProtocol)) {
		return false;
	}

	protocol = normalizedProtocol.toStdString();
	streamKey = keyValue.toString().toStdString();

	return true;
}

StreamProtocol API_Endpoint::feedProtocol(const std::string &protocol) {
	if (protocol == "SRTLA") {
		return StreamProtocol::SRTLA;
	} else if (protocol == "RTSP") {
		return StreamProtocol::RTSP;
	} else if (protocol == "RTMP") {
		return StreamProtocol::RTMP;
	} else if (protocol == "WEBRTC") {
		return StreamProtocol::WebRTC;
	} else if (protocol == "MOQ") {
		return StreamProtocol::MoQ;
	}

	return StreamProtocol::SRT;
}

void API_Endpoint::startFeed(const std::string &protocol, const std::string &streamKey, const std::string &host) {
	const StreamProtocol selectedProtocol = feedProtocol(protocol);
	GoIRL_Process *const goIrl =
		selectedProtocol == StreamProtocol::SRTLA ? new GoIRL_Process(RELAY_SRTLA_PORT, m_goIrlPath) : nullptr;
	FeedContext *const context = new FeedContext(streamKey, selectedProtocol, host, m_mediaManager.get(), goIrl);

	if (goIrl) {
		goIrl->setParent(context);
	}

	m_contexts.emplace(streamKey, context);

	QObject::connect(context, &FeedContext::started, context, [this, streamKey](const QString &) {
		const std::lock_guard<std::mutex> lock(m_feedsMutex);
		const std::map<std::string, FeedInfo>::iterator feed = m_feeds.find(streamKey);

		if (feed != m_feeds.end() && feed->second.status == "starting") {
			feed->second.status = "running";
		}
	});
	QObject::connect(context, &FeedContext::error, context, [this, streamKey](const QString &message) {
		const std::lock_guard<std::mutex> lock(m_feedsMutex);
		const std::map<std::string, FeedInfo>::iterator feed = m_feeds.find(streamKey);

		if (feed != m_feeds.end()) {
			feed->second.status = "failed";
			feed->second.error = message.toStdString();
		}
	});
	QObject::connect(context, &FeedContext::stopped, context, [this, streamKey, context] {
		{
			const std::lock_guard<std::mutex> lock(m_feedsMutex);
			m_feeds.erase(streamKey);
		}

		m_contexts.erase(streamKey);
		context->deleteLater();
	});

	context->start();
}

void API_Endpoint::stopFeed(const std::string &streamKey) {
	const std::map<std::string, FeedContext *>::iterator context = m_contexts.find(streamKey);

	if (context != m_contexts.end()) {
		context->second->stop();
	}
}

void API_Endpoint::registerEndpoints() {
	m_server->Post("/feeds", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}

		std::string protocol;
		std::string streamKey;

		if (!parseFeed(req, protocol, streamKey)) {
			res.status = 400;
			return;
		}

		const QUrl requestHost(QStringLiteral("https://") +
				       QString::fromStdString(req.get_header_value("Host")));
		std::string link;

		{
			const std::lock_guard<std::mutex> lock(m_feedsMutex);
			std::string host = m_publicHost;

			if (host.empty()) {
				if (!requestHost.isValid() || requestHost.host().isEmpty() ||
				    !requestHost.userInfo().isEmpty() || !requestHost.path().isEmpty() ||
				    requestHost.hasQuery() || requestHost.hasFragment()) {
					res.status = 400;
					return;
				}

				host = requestHost.host().toStdString();
			}

			link = MediamtxManager::publishLink(streamKey, feedProtocol(protocol), host).toStdString();

			if (m_feeds.contains(streamKey)) {
				res.status = 409;
				return;
			}

			m_feeds.emplace(
				streamKey,
				FeedInfo{.protocol = protocol, .status = "starting", .error = {}, .link = link});

			if (!postToEventLoop(
				    [this, protocol, streamKey, host] { startFeed(protocol, streamKey, host); })) {
				m_feeds.erase(streamKey);
				res.status = 503;
				return;
			}
		}

		QJsonObject response;
		response.insert(QStringLiteral("link"), QString::fromStdString(link));
		res.set_content(QJsonDocument(response).toJson(QJsonDocument::Compact).toStdString(),
				"application/json");
		res.status = 202;
	});

	m_server->Get("/status", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}

		QJsonObject response;
		response.insert(QStringLiteral("status"), QStringLiteral("ok"));
		res.set_content(QJsonDocument(response).toJson(QJsonDocument::Compact).toStdString(),
				"application/json");
		res.status = 200;
	});

	m_server->Delete("/feeds", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}

		std::string protocol;
		std::string streamKey;

		if (!parseFeed(req, protocol, streamKey)) {
			res.status = 400;
			return;
		}

		const std::lock_guard<std::mutex> lock(m_feedsMutex);

		const std::map<std::string, FeedInfo>::iterator feed = m_feeds.find(streamKey);

		if (feed == m_feeds.end() || feed->second.protocol != protocol) {
			res.status = 404;
			return;
		}

		if (feed->second.status == "stopping") {
			res.status = 202;
			return;
		}

		const std::string previousStatus = feed->second.status;
		feed->second.status = "stopping";

		if (!postToEventLoop([this, streamKey] { stopFeed(streamKey); })) {
			feed->second.status = previousStatus;
			res.status = 503;
			return;
		}

		res.status = 202;
	});

	m_server->Get("/feeds", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}

		QJsonArray feeds;
		{
			const std::lock_guard<std::mutex> lock(m_feedsMutex);

			for (const auto &[streamKey, feed] : m_feeds) {
				QJsonObject entry;
				entry.insert(QStringLiteral("protocol"), QString::fromStdString(feed.protocol));
				entry.insert(QStringLiteral("streamKey"), QString::fromStdString(streamKey));
				entry.insert(QStringLiteral("status"), QString::fromStdString(feed.status));
				entry.insert(QStringLiteral("link"), QString::fromStdString(feed.link));

				if (!feed.error.empty()) {
					entry.insert(QStringLiteral("error"), QString::fromStdString(feed.error));
				}

				feeds.append(entry);
			}
		}

		res.set_content(QJsonDocument(feeds).toJson(QJsonDocument::Compact).toStdString(), "application/json");
		res.status = 200;
	});
}

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

	for (const std::pair<const std::string, FeedContext *> &entry : m_contexts) {
		delete entry.second;
	}

	m_contexts.clear();
	m_mediaManager.reset();
	m_publicIpTimer.reset();
	m_publicIpProcess.reset();

	const std::lock_guard<std::mutex> lock(m_feedsMutex);
	m_feeds.clear();
}
