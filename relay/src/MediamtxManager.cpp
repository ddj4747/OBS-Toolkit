#include <MediamtxManager.h>
#include <MediaMtxConfig.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QProcessEnvironment>
#include <QVariant>
#include <QByteArray>
#include <QtNetwork/QNetworkReply>

#include <algorithm>
#include <spdlog/spdlog.h>
#include <filesystem>

static constexpr int c_stopTimeoutMs = 5000;

MediamtxManager::MediamtxManager(const QString &executablePath, const QString &configPath, const RelayConfig &config)
	: m_config(config),
	  m_executablePath(executablePath),
	  m_configPath(configPath) {
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

	QString error;
	if (!MediaMtxConfig::write(m_configPath, m_config, error)) {
		spdlog::error("failed to write MediaMTX configuration: {}", error.toStdString());
		emit serverError(ServerError::FailedToStart);
		return;
	}
	m_stopRequested = false;
	m_apiReady = false;
	m_process = new QProcess(this);
	QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
	for (const QString &name : environment.keys()) {
		if (name.startsWith(QStringLiteral("MTX_"), Qt::CaseInsensitive))
			environment.remove(name);
	}
	m_process->setProcessEnvironment(environment);
	connect(m_process, &QProcess::started, this, &MediamtxManager::onProcessStarted);
	connect(m_process, &QProcess::errorOccurred, this, &MediamtxManager::onProcessErrorOccurred);
	connect(m_process, &QProcess::finished, this, &MediamtxManager::onProcessFinished);
	connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
		const QByteArray output = m_process->readAllStandardOutput().trimmed();
		if (!output.isEmpty()) {
			spdlog::info("mediamtx: {}", output.constData());
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
			spdlog::warn("mediamtx: {}", output.constData());
	});
	m_process->start(m_executablePath, {m_configPath});
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

void MediamtxManager::onProcessStarted() {}

void MediamtxManager::onProcessErrorOccurred(const QProcess::ProcessError error) {
	if (error != QProcess::FailedToStart) {
		return;
	}

	spdlog::error("failed to start mediamtx: {}", qUtf8Printable(m_process->errorString()));
	cleanupProcess();
	emit serverError(ServerError::FailedToStart);
}

void MediamtxManager::onProcessFinished(const int exitCode, const QProcess::ExitStatus exitStatus) {
	const bool requested = m_stopRequested;
	spdlog::log(requested ? spdlog::level::info : spdlog::level::err, "mediamtx exited with code {} ({})", exitCode,
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

void MediamtxManager::addInput(const std::string &streamId, const StreamProtocol protocol, const std::string &ip) {
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

QString MediamtxManager::pathName(const std::string &streamId, const StreamProtocol protocol) {
	const QString name = QString::fromStdString(streamId);
	return protocol == StreamProtocol::RTMP ? QStringLiteral("app/%1").arg(name) : name;
}

void MediamtxManager::addInputWhenReady(const std::string &streamId, const StreamProtocol protocol,
					const std::string &ip) {
	const QString qname = QString::fromStdString(streamId);
	QJsonObject body;
	body["source"] = "publisher";

	QString publishUrl;
	const QString mediaPath = pathName(streamId, protocol);
	const QString encodedPathName = QString::fromLatin1(QUrl::toPercentEncoding(mediaPath));
	const QString ipStr = QString::fromStdString(ip);

	switch (protocol) {
	case StreamProtocol::RTSP:
		publishUrl = QString("rtsp://%1:%2/%3").arg(ipStr).arg(m_config.rtspPort).arg(mediaPath);
		break;
	case StreamProtocol::RTMP:
		publishUrl = QString("rtmp://%1:%2/%3").arg(ipStr).arg(m_config.rtmpPort).arg(mediaPath);
		break;
	case StreamProtocol::SRT:
		publishUrl =
			QString("srt://%1:%2?streamid=publish:%3").arg(ipStr).arg(m_config.srtPort).arg(encodedPathName);
		break;
	case StreamProtocol::WebRTC:
		publishUrl = QString("http://%1:%2/%3/whip").arg(ipStr).arg(m_config.webRtcPort).arg(mediaPath);
		break;
	case StreamProtocol::MoQ:
		publishUrl = QString("https://%1:%2/%3/publish").arg(ipStr).arg(m_config.moqPort).arg(mediaPath);
		break;
	default:
		emit inputError(qname, QStringLiteral("Unsupported protocol"));
		return;
	}

	const QUrl requestUrl =
		QString("http://127.0.0.1:%1/v3/config/paths/add/%2").arg(MediaMtxConfig::apiPort).arg(encodedPathName);

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
			spdlog::error("failed to add MediaMTX input '{}': {}", qUtf8Printable(qname),
				      qUtf8Printable(error));
			emit inputError(qname, error);
		} else {
			m_inputs.insert_or_assign(streamId, std::make_pair(protocol, false));
			emit inputAdded(qname, publishUrl);
		}
		reply->deleteLater();
	});
}

void MediamtxManager::removeInput(const std::string &name, const StreamProtocol protocol) {
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
	const QUrl requestUrl = QString("http://127.0.0.1:%1/v3/config/paths/delete/%2")
					.arg(MediaMtxConfig::apiPort)
					.arg(encodedPathName);

	QNetworkRequest request(requestUrl);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QNetworkReply *reply = m_networkAccessManager->sendCustomRequest(request, "DELETE");
	connect(reply, &QNetworkReply::finished, this, [this, reply, qname]() {
		const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
		if (reply->error() != QNetworkReply::NoError || !status.isValid() || status.toInt() / 100 != 2) {
			const QString error = reply->error() != QNetworkReply::NoError
						      ? reply->errorString()
						      : QStringLiteral("HTTP %1").arg(status.toInt());
			spdlog::error("failed to remove MediaMTX input '{}': {}", qUtf8Printable(qname),
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

	for (const std::pair<const std::string, std::pair<StreamProtocol, bool>> &entry : m_inputs) {
		const std::string &streamId = entry.first;
		const QString path =
			QString::fromLatin1(QUrl::toPercentEncoding(pathName(streamId, entry.second.first)));
		QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(
			QString("http://127.0.0.1:%1/v3/paths/get/%2").arg(MediaMtxConfig::apiPort).arg(path)));
		connect(reply, &QNetworkReply::finished, this, [this, reply, streamId]() {
			const std::map<std::string, std::pair<StreamProtocol, bool>>::iterator input =
				m_inputs.find(streamId);
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
