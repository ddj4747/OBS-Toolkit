#include <MediamtxStreamHandler.h>

#include <obs-module.h>
#include <plugin-support.h>

#include <QThread>

namespace {
MediamtxManager *s_manager = nullptr;
}

MediamtxStreamHandler::MediamtxStreamHandler(QObject *parent) : StreamHandler(parent), m_manager(sharedManager()) {
	QObject::connect(m_manager, &MediamtxManager::serverStarted, this,
			 []() { obs_log(LOG_INFO, "mediamtx server is ready"); });
	QObject::connect(m_manager, &MediamtxManager::serverStopped, this,
			 []() { obs_log(LOG_INFO, "mediamtx server stopped"); });
	QObject::connect(m_manager, &MediamtxManager::serverError, this,
			 [this](const MediamtxManager::ServerError error) {
				 const QString message =
					 QStringLiteral("MediaMTX server error: %1").arg(static_cast<int>(error));
				 obs_log(LOG_ERROR, "%s", qUtf8Printable(message));
				 emit streamError(QString::fromStdString(m_streamId), message);
			 });
	QObject::connect(m_manager, &MediamtxManager::inputAdded, this,
			 [this](const QString &streamId, const QString &publishUrl) {
				 if (streamId != QString::fromStdString(m_streamId)) {
					 return;
				 }
				 obs_log(LOG_INFO, "mediamtx input '%s' added: %s", qUtf8Printable(streamId),
					 qUtf8Printable(publishUrl));
				 emit streamReady(streamId, publishUrl);
			 });
	QObject::connect(m_manager, &MediamtxManager::inputAvailable, this, [this](const QString &streamId) {
		if (streamId == QString::fromStdString(m_streamId)) {
			emit streamAvailable(streamId);
		}
	});
	QObject::connect(m_manager, &MediamtxManager::inputUnavailable, this, [this](const QString &streamId) {
		if (streamId == QString::fromStdString(m_streamId)) {
			emit streamUnavailable(streamId);
		}
	});
	QObject::connect(m_manager, &MediamtxManager::inputRemoved, this, [this](const QString &streamId) {
		if (streamId == QString::fromStdString(m_streamId)) {
			emit streamStopped(streamId);
		}
	});
	QObject::connect(m_manager, &MediamtxManager::inputError, this,
			 [this](const QString &streamId, const QString &error) {
				 if (streamId == QString::fromStdString(m_streamId)) {
					 obs_log(LOG_ERROR, "mediamtx input '%s' error: %s", qUtf8Printable(streamId),
						 qUtf8Printable(error));
					 emit streamError(streamId, error);
				 }
			 });
}

void MediamtxStreamHandler::start(const std::string &streamId, const Protocol protocol,
				  const std::string &publicAddress, const uint16_t) {
	m_streamId = streamId;
	m_protocol = protocol;
	m_manager->startServer();
	m_manager->addInput(streamId, protocol, publicAddress);
}

void MediamtxStreamHandler::stop(const std::string &streamId) {
	m_manager->removeInput(streamId, m_protocol);
}

void MediamtxStreamHandler::prepareForShutdown() {
	MediamtxManager *manager = s_manager;
	s_manager = nullptr;
	if (!manager) {
		return;
	}

	if (manager->thread() == QThread::currentThread()) {
		delete manager;
		return;
	}

	QMetaObject::invokeMethod(manager, [manager]() { delete manager; }, Qt::BlockingQueuedConnection);
}

MediamtxManager *MediamtxStreamHandler::sharedManager() {
	if (!s_manager) {
		s_manager = new MediamtxManager();
	}
	return s_manager;
}
