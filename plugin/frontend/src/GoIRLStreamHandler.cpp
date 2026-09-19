#include <GoIRLStreamHandler.h>

#include <obs-module.h>
#include <plugin-support.h>

GoIRLStreamHandler::GoIRLStreamHandler(QObject *parent) : StreamHandler(parent) {}

GoIRLStreamHandler::~GoIRLStreamHandler() {
	delete m_process;
}

void GoIRLStreamHandler::start(const std::string &streamId, const StreamProtocol, const std::string &publicAddress,
			       const uint16_t port) {
	m_streamId = streamId;
	m_publicAddress = publicAddress;

	if (!m_process) {
		m_process = new GoIRL_Process(port);
		QObject::connect(m_process, &GoIRL_Process::serverStarted, this, [this]() {
			const QString streamId = QString::fromStdString(m_streamId);
			const QString publishUrl = QString::fromStdString(m_process->getStreamUrl(m_publicAddress));
			obs_log(LOG_INFO, "go-irl SRTLA server started");
			emit streamReady(streamId, publishUrl);
			emit streamAvailable(streamId);
		});
		QObject::connect(m_process, &GoIRL_Process::serverStopped, this, [this]() {
			obs_log(LOG_INFO, "go-irl SRTLA server stopped");
			emit streamStopped(QString::fromStdString(m_streamId));
		});
		QObject::connect(m_process, &GoIRL_Process::serverError, this,
				 [this](const GoIRL_Process::ServerError error) {
					 const QString message =
						 QStringLiteral("go-irl server error: %1").arg(static_cast<int>(error));
					 obs_log(LOG_ERROR, "%s", qUtf8Printable(message));
					 emit streamError(QString::fromStdString(m_streamId), message);
				 });
	}

	m_process->startServer(streamId);
}

void GoIRLStreamHandler::stop(const std::string &) {
	if (m_process) {
		m_process->stopServer();
	}
}
