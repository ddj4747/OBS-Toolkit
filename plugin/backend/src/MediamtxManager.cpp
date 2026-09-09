#include <MediamtxManager.h>

#include <obs-module.h>
#include <plugin-support.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QVariant>
#include <QByteArray>
#include <QtNetwork/QNetworkReply>

#include <algorithm>
#include <filesystem>
#include <util/bmem.h>

static constexpr int c_stopTimeoutMs = 5000;

MediamtxManager::MediamtxManager() {
	m_networkAccessManager = new QNetworkAccessManager(this);
	m_inputStatusTimer = new QTimer(this);
	m_inputStatusTimer->setInterval(1000);
	connect(m_inputStatusTimer, &QTimer::timeout, this, &MediamtxManager::pollInputAvailability);
	m_inputStatusTimer->start();
}

MediamtxManager::~MediamtxManager() {
	if (m_process == nullptr) {
		return;
	}

	(void)m_process->disconnect(this);
	m_process->kill();
	m_process->waitForFinished(c_stopTimeoutMs);
	m_process = nullptr;
}

void MediamtxManager::startServer() {
	if (m_process != nullptr) {
		return;
	}

#ifdef WIN32
	char *serverPath = obs_module_file("mediamtx.exe");
#else
	char *serverPath = obs_module_file("mediamtx");
#endif

	if (serverPath == nullptr) {
		obs_log(LOG_ERROR, "mediamtx binary was not found in the plugin data directory");
		emit serverError(ServerError::FailedToStart);
		return;
	}

	const std::filesystem::path path(serverPath);
	const QString pathStr(path.string().data());

	const std::filesystem::path config = path.parent_path() / "mediamtx.yml";
	const QString configStr(config.string().data());

	bfree(serverPath);

	m_stopRequested = false;
	m_apiReady = false;
	m_process = new QProcess(this);
	connect(m_process, &QProcess::started, this, &MediamtxManager::onProcessStarted);
	connect(m_process, &QProcess::errorOccurred, this, &MediamtxManager::onProcessErrorOccurred);
	connect(m_process, &QProcess::finished, this, &MediamtxManager::onProcessFinished);
	connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
		const QByteArray output = m_process->readAllStandardOutput().trimmed();
		if (!output.isEmpty()) {
			obs_log(LOG_INFO, "mediamtx: %s", output.constData());
			if (!m_apiReady && output.contains("[API] started with listener")) {
				m_apiReady = true;
				emit serverStarted();
				flushPendingInputs();
			}
		}
	});
	connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
		const QByteArray output = m_process->readAllStandardError().trimmed();
		if (!output.isEmpty())
			obs_log(LOG_WARNING, "mediamtx: %s", output.constData());
	});
	m_process->start(pathStr, {configStr});
}

void MediamtxManager::stopServer() {
	if (m_process == nullptr) {
		return;
	}

	m_stopRequested = true;
	m_process->kill();
}

bool MediamtxManager::running() const {
	return m_process != nullptr && m_process->state() == QProcess::Running;
}

bool MediamtxManager::ready() const {
	return running() && m_apiReady;
}

void MediamtxManager::onProcessStarted() {
}

void MediamtxManager::onProcessErrorOccurred(const QProcess::ProcessError error) {
	if (error != QProcess::FailedToStart) {
		return;
	}

	obs_log(LOG_ERROR, "failed to start mediamtx: %s", qUtf8Printable(m_process->errorString()));
	cleanupProcess();
	emit serverError(ServerError::FailedToStart);
}

void MediamtxManager::onProcessFinished(const int exitCode, const QProcess::ExitStatus exitStatus) {
	const bool requested = m_stopRequested;
	obs_log(requested ? LOG_INFO : LOG_ERROR, "mediamtx exited with code %d (%s)", exitCode,
		exitStatus == QProcess::NormalExit ? "normal exit" : "crashed");
	cleanupProcess();

	if (requested) {
		emit serverStopped();
	} else {
		emit serverError(ServerError::Crashed);
	}
}

void MediamtxManager::terminateProcess() {
	(void)m_process->disconnect(this);
	m_process->kill();
	m_process->waitForFinished(c_stopTimeoutMs);
	m_process->deleteLater();
	m_process = nullptr;
	m_stopRequested = false;
}

void MediamtxManager::cleanupProcess() {
	if (m_process == nullptr) {
		return;
	}

	(void)m_process->disconnect(this);
	m_process->deleteLater();
	m_process = nullptr;
	m_stopRequested = false;
	m_apiReady = false;
	m_pendingInputs.clear();
	m_inputs.clear();
}

void MediamtxManager::addInput(const std::string &streamId, const Protocol protocol, const std::string &ip) {
	const QString qname = QString::fromStdString(streamId);
	if (!running()) {
		m_pendingInputs.push_back({streamId, protocol, ip});
		startServer();
		return;
	}

	if (!ready()) {
		m_pendingInputs.push_back({streamId, protocol, ip});
		return;
	}

	addInputWhenReady(streamId, protocol, ip);
}

void MediamtxManager::flushPendingInputs() {
	while (!m_pendingInputs.empty()) {
		const PendingInput input = std::move(m_pendingInputs.front());
		m_pendingInputs.pop_front();
		addInputWhenReady(input.streamId, input.protocol, input.ip);
	}
}

QString MediamtxManager::pathName(const std::string &streamId, const Protocol protocol) {
	const QString name = QString::fromStdString(streamId);
	return protocol == Protocol::RTMP ? QStringLiteral("app/%1").arg(name) : name;
}

void MediamtxManager::addInputWhenReady(const std::string &streamId, const Protocol protocol, const std::string &ip) {
	const QString qname = QString::fromStdString(streamId);
	QJsonObject body;
	body["source"] = "publisher";

	QString publishUrl;
	const QString mediaPath = pathName(streamId, protocol);
	const QString encodedPathName = QString::fromLatin1(QUrl::toPercentEncoding(mediaPath));
	const QString ipStr = QString::fromStdString(ip);

	switch (protocol) {
	case Protocol::RTSP:
		publishUrl = QString("rtsp://%1:8554/%2").arg(ipStr).arg(mediaPath);
		break;
	case Protocol::RTMP:
		publishUrl = QString("rtmp://%1:1935/%2").arg(ipStr).arg(mediaPath);
		break;
	case Protocol::SRT:
		publishUrl = QString("srt://%1:8890?streamid=publish:%2").arg(ipStr).arg(encodedPathName);
		break;
	case Protocol::WebRTC:
		publishUrl = QString("http://%1:8889/%2/whip").arg(ipStr).arg(mediaPath);
		break;
	default:
		emit inputError(qname, QStringLiteral("Unsupported protocol"));
		return;
	}

	const QUrl requestUrl = QString("http://127.0.0.1:9997/v3/config/paths/add/%1").arg(encodedPathName);

	QNetworkRequest request(requestUrl);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	const QJsonDocument doc(body);
	QNetworkReply *reply = m_networkAccessManager->sendCustomRequest(request, "POST", doc.toJson());
	connect(reply, &QNetworkReply::finished, this, [this, reply, qname, publishUrl, streamId, protocol]() {
		const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
		if (reply->error() != QNetworkReply::NoError || !status.isValid() || status.toInt() / 100 != 2) {
			const QString error = reply->error() != QNetworkReply::NoError
						      ? reply->errorString()
						      : QStringLiteral("HTTP %1").arg(status.toInt());
			obs_log(LOG_ERROR, "failed to add MediaMTX input '%s': %s", qUtf8Printable(qname),
				qUtf8Printable(error));
			emit inputError(qname, error);
		} else {
			m_inputs.insert_or_assign(streamId, std::make_pair(protocol, false));
			emit inputAdded(qname, publishUrl);
		}
		reply->deleteLater();
	});
}

void MediamtxManager::removeInput(const std::string &name, const Protocol protocol) {
	const QString qname = QString::fromStdString(name);
	std::erase_if(m_pendingInputs, [&name](const PendingInput &input) { return input.streamId == name; });
	m_inputs.erase(name);

	if (!running()) {
		return;
	}
	if (!ready()) {
		return;
	}

	const QString encodedPathName = QString::fromLatin1(QUrl::toPercentEncoding(pathName(name, protocol)));
	const QUrl requestUrl = QString("http://127.0.0.1:9997/v3/config/paths/delete/%1").arg(encodedPathName);

	QNetworkRequest request(requestUrl);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QNetworkReply *reply = m_networkAccessManager->sendCustomRequest(request, "DELETE");
	connect(reply, &QNetworkReply::finished, this, [this, reply, qname]() {
		const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
		if (reply->error() != QNetworkReply::NoError || !status.isValid() || status.toInt() / 100 != 2) {
			const QString error = reply->error() != QNetworkReply::NoError
						      ? reply->errorString()
						      : QStringLiteral("HTTP %1").arg(status.toInt());
			obs_log(LOG_ERROR, "failed to remove MediaMTX input '%s': %s", qUtf8Printable(qname),
				qUtf8Printable(error));
			emit inputError(qname, error);
		} else {
			emit inputRemoved(qname);
		}
		reply->deleteLater();
	});
}

void MediamtxManager::pollInputAvailability() {
	if (!ready()) {
		return;
	}

	for (const auto &[streamId, input] : m_inputs) {
		const QString path = QString::fromLatin1(QUrl::toPercentEncoding(pathName(streamId, input.first)));
		QNetworkReply *reply = m_networkAccessManager->get(
			QNetworkRequest(QString("http://127.0.0.1:9997/v3/paths/get/%1").arg(path)));
		connect(reply, &QNetworkReply::finished, this, [this, reply, streamId]() {
			const auto input = m_inputs.find(streamId);
			if (input == m_inputs.end()) {
				reply->deleteLater();
				return;
			}

			const QJsonObject status = QJsonDocument::fromJson(reply->readAll()).object();
			const bool available = reply->error() == QNetworkReply::NoError && status["available"].toBool();
			if (available != input->second.second) {
				input->second.second = available;
				emit available ? inputAvailable(QString::fromStdString(streamId))
					       : inputUnavailable(QString::fromStdString(streamId));
			}
			reply->deleteLater();
		});
	}
}
