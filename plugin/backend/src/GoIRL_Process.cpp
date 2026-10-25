#include <GoIRL_Process.h>
#include <KillOnCloseJob.h>
#include <PortForwardUtils.h>

#include <obs-module.h>
#include <plugin-support.h>

#include <QByteArray>
#include <filesystem>
#include <util/bmem.h>
#include <QThread>

static constexpr int c_stopTimeoutMs = 5000;

GoIRL_Process::GoIRL_Process(const uint16_t port) : m_port(port) {
	m_localSrtPort = PortForwarder::reserveUdpPort();
}

GoIRL_Process::~GoIRL_Process() {
	if (m_process != nullptr) {
		(void)m_process->disconnect(this);
		m_process->kill();
		m_process->waitForFinished(c_stopTimeoutMs);
		m_process = nullptr;
	}

	PortForwarder::releaseUdpPort(m_localSrtPort);
	m_localSrtPort = 0;
}

void GoIRL_Process::startServer(const std::string &streamKey) {
	if (QThread::currentThread() != thread()) {
		QMetaObject::invokeMethod(this, [this, streamKey] { startServer(streamKey); }, Qt::QueuedConnection);
		return;
	}

#ifdef WIN32
	char *serverPath = obs_module_file("go-irl.exe");
#else
	char *serverPath = obs_module_file("go-irl");
#endif

	if (serverPath == nullptr) {
		obs_log(LOG_ERROR, "go-irl binary was not found in the plugin data directory");
		emit serverError(ServerError::FailedToStart);
		return;
	}

	if (m_localSrtPort == 0) {
		obs_log(LOG_ERROR, "failed to allocate a local SRT port");
		emit serverError(ServerError::FailedToStart);
		return;
	}

	if (m_process != nullptr) {
		terminateProcess();
	}

	m_listening = false;
	m_stderrBuffer.clear();

	const std::filesystem::path path(serverPath);
	const QString pathStr(path.string().data());
	bfree(serverPath);

	m_stopRequested = false;
	m_process = new QProcess(this);
	attachToKillOnCloseJob(m_process);
	connect(m_process, &QProcess::errorOccurred, this, &GoIRL_Process::onProcessErrorOccurred);
	connect(m_process, &QProcess::finished, this, &GoIRL_Process::onProcessFinished);
	connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
		const QByteArray output = m_process->readAllStandardOutput().trimmed();
		if (!output.isEmpty())
			obs_log(LOG_INFO, "go-irl: %s", output.constData());
	});
	connect(m_process, &QProcess::readyReadStandardError, this, &GoIRL_Process::readProcessError);

	const QString streamIdArgument = QString::fromUtf8(streamKey.data(), static_cast<qsizetype>(streamKey.size()));
	obs_log(LOG_INFO, "starting go-irl SRTLA on %u, local SRT on %u, stream %s", m_port, m_localSrtPort,
		streamKey.c_str());
	m_process->start(pathStr, {QStringLiteral("-mode=server"), QStringLiteral("-cli"), QStringLiteral("-insecure"),
				   QStringLiteral("-verbose"), QStringLiteral("-srtla-port=") + QString::number(m_port),
				   QStringLiteral("-srt-port=") + QString::number(m_localSrtPort),
				   QStringLiteral("-streamId=") + streamIdArgument});
}

uint16_t GoIRL_Process::localSrtPort() const {
	return m_localSrtPort;
}

void GoIRL_Process::stopServer() {
	if (QThread::currentThread() != thread()) {
		QMetaObject::invokeMethod(this, [this] { stopServer(); }, Qt::QueuedConnection);
		return;
	}

	if (m_process == nullptr) {
		return;
	}

	m_stopRequested = true;
	m_process->kill();
}

std::string GoIRL_Process::getStreamUrl(const std::string &publicAddress) {
	return std::format("srtla://{}:{}", publicAddress, m_port);
}

bool GoIRL_Process::running() const {
	return m_process != nullptr && m_process->state() == QProcess::Running;
}

void GoIRL_Process::onProcessErrorOccurred(const QProcess::ProcessError error) {
	if (error != QProcess::FailedToStart) {
		return;
	}

	obs_log(LOG_ERROR, "failed to start go-irl: %s", qUtf8Printable(m_process->errorString()));
	cleanupProcess();
	emit serverError(ServerError::FailedToStart);
}

void GoIRL_Process::readProcessError() {
	if (m_process == nullptr) {
		return;
	}

	m_stderrBuffer.append(m_process->readAllStandardError());
	qsizetype newline = 0;
	while ((newline = m_stderrBuffer.indexOf('\n')) >= 0) {
		const QByteArray line = m_stderrBuffer.left(newline).trimmed();
		m_stderrBuffer.remove(0, newline + 1);
		if (line.isEmpty()) {
			continue;
		}

		obs_log(LOG_WARNING, "go-irl: %s", line.constData());
		if (!m_listening && line.contains("Listening on")) {
			m_listening = true;
			emit serverStarted();
		}
	}
}

void GoIRL_Process::onProcessFinished(const int exitCode, const QProcess::ExitStatus exitStatus) {
	readProcessError();
	const QByteArray trailing = m_stderrBuffer.trimmed();
	m_stderrBuffer.clear();
	if (!trailing.isEmpty()) {
		obs_log(LOG_WARNING, "go-irl: %s", trailing.constData());
	}

	const bool requested = m_stopRequested;
	const bool listening = m_listening;
	m_listening = false;
	obs_log(requested ? LOG_INFO : LOG_ERROR, "go-irl exited with code %d (%s)", exitCode,
		exitStatus == QProcess::NormalExit ? "normal exit" : "crashed");
	cleanupProcess();

	if (!listening && !requested) {
		emit serverError(ServerError::FailedToStart);
		return;
	}

	if (requested) {
		emit serverStopped();
	} else {
		emit serverError(ServerError::Crashed);
	}
}

void GoIRL_Process::terminateProcess() {
	(void)m_process->disconnect(this);
	m_process->kill();
	m_process->waitForFinished(c_stopTimeoutMs);
	m_process->deleteLater();
	m_process = nullptr;
	m_stopRequested = false;
}

void GoIRL_Process::cleanupProcess() {
	if (m_process == nullptr) {
		return;
	}

	(void)m_process->disconnect(this);
	m_process->deleteLater();
	m_process = nullptr;
	m_stopRequested = false;
}
