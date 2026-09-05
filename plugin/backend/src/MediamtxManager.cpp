#include <MediamtxManager.h>

#include <obs-module.h>
#include <plugin-support.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QVariant>
#include <QtNetwork/QNetworkReply>

#include <filesystem>
#include <util/bmem.h>

static constexpr int c_stopTimeoutMs = 5000;

MediamtxManager::MediamtxManager() {
	m_networkAccessManager = new QNetworkAccessManager(this);
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

	if (m_process != nullptr) {
		terminateProcess();
	}

	const std::filesystem::path path(serverPath);
	const QString pathStr(path.string().data());

	const std::filesystem::path config = path.parent_path() / "mediamtx.yml";
	const QString configStr(config.string().data());

	bfree(serverPath);

	m_stopRequested = false;
	m_process = new QProcess(this);
	connect(m_process, &QProcess::started, this, &MediamtxManager::onProcessStarted);
	connect(m_process, &QProcess::errorOccurred, this, &MediamtxManager::onProcessErrorOccurred);
	connect(m_process, &QProcess::finished, this, &MediamtxManager::onProcessFinished);
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

void MediamtxManager::onProcessStarted() {
	emit serverStarted();
}

void MediamtxManager::onProcessErrorOccurred(const QProcess::ProcessError error) {
	if (error != QProcess::FailedToStart) {
		return;
	}

	obs_log(LOG_ERROR, "failed to start mediamtx: %s", qUtf8Printable(m_process->errorString()));
	cleanupProcess();
	emit serverError(ServerError::FailedToStart);
}

void MediamtxManager::onProcessFinished(const int /*exitCode*/, const QProcess::ExitStatus /*exitStatus*/) {
	const bool requested = m_stopRequested;
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
}

void MediamtxManager::addInput(const std::string &streamId, const Protocol protocol, const std::string &ip) {
	const QString qname = QString::fromStdString(streamId);
	if (!running()) {
		emit inputError(qname, QStringLiteral("MediaMTX server is not running"));
		return;
	}

	QJsonObject body;
	body["source"] = "publisher";

	QString publishUrl;
	const QString encodedName = QString::fromLatin1(QUrl::toPercentEncoding(qname));
	const QString ipStr = QString::fromStdString(ip);

	switch (protocol) {
	case Protocol::RTSP:
		publishUrl = QString("rtsp://%1:8554/%2").arg(ipStr).arg(encodedName);
		break;
	case Protocol::RTMP:
		publishUrl = QString("rtmp://%1:1935/%2").arg(ipStr).arg(encodedName);
		break;
	case Protocol::SRT:
		publishUrl = QString("srt://%1:8890?streamid=publish:%2").arg(ipStr).arg(encodedName);
		break;
	case Protocol::WebRTC:
		publishUrl = QString("http://%1:8889/%2/whip").arg(ipStr).arg(encodedName);
		break;
	default:
		emit inputError(qname, QStringLiteral("Unsupported protocol"));
		return;
	}

	const QUrl requestUrl = QString("http://127.0.0.1:9997/v3/config/paths/add/%1").arg(encodedName);

	QNetworkRequest request(requestUrl);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	const QJsonDocument doc(body);
	QNetworkReply *reply = m_networkAccessManager->sendCustomRequest(request, "POST", doc.toJson());
	connect(reply, &QNetworkReply::finished, this, [this, reply, qname, publishUrl]() {
		const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
		if (reply->error() != QNetworkReply::NoError || !status.isValid() || status.toInt() / 100 != 2) {
			const QString error = reply->error() != QNetworkReply::NoError
						      ? reply->errorString()
						      : QStringLiteral("HTTP %1").arg(status.toInt());
			obs_log(LOG_ERROR, "failed to add MediaMTX input '%s': %s", qUtf8Printable(qname),
				qUtf8Printable(error));
			emit inputError(qname, error);
		} else {
			emit inputAdded(qname, publishUrl);
		}
		reply->deleteLater();
	});
}

void MediamtxManager::removeInput(const std::string &name) {
	const QString qname = QString::fromStdString(name);
	if (!running()) {
		emit inputError(qname, QStringLiteral("MediaMTX server is not running"));
		return;
	}

	const QString encodedName = QString::fromLatin1(QUrl::toPercentEncoding(qname));
	const QUrl requestUrl = QString("http://127.0.0.1:9997/v3/config/paths/delete/%1").arg(encodedName);

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
