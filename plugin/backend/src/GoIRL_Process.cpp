#include <GoIRL_Process.h>

#include <obs-module.h>
#include <plugin-support.h>

#include <QByteArray>
#include <filesystem>
#include <util/bmem.h>
#include <QThread>

static constexpr int c_stopTimeoutMs = 5000;
static constexpr std::size_t c_minPassphraseLength = 10;

GoIRL_Process::GoIRL_Process(const uint16_t port) : m_port(port) {}

GoIRL_Process::~GoIRL_Process() {
	if (m_process == nullptr) {
		return;
	}

	(void)m_process->disconnect(this);
	m_process->kill();
	m_process->waitForFinished(c_stopTimeoutMs);
	m_process = nullptr;
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

	if (m_process != nullptr) {
		terminateProcess();
	}

	const std::filesystem::path path(serverPath);
	const QString pathStr(path.string().data());
	bfree(serverPath);

	m_stopRequested = false;
	m_process = new QProcess(this);
	connect(m_process, &QProcess::started, this, &GoIRL_Process::onProcessStarted);
	connect(m_process, &QProcess::errorOccurred, this, &GoIRL_Process::onProcessErrorOccurred);
	connect(m_process, &QProcess::finished, this, &GoIRL_Process::onProcessFinished);
	connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
		const QByteArray output = m_process->readAllStandardOutput().trimmed();
		if (!output.isEmpty())
			obs_log(LOG_INFO, "go-irl: %s", output.constData());
	});
	connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
		const QByteArray output = m_process->readAllStandardError().trimmed();
		if (!output.isEmpty())
			obs_log(LOG_WARNING, "go-irl: %s", output.constData());
	});

	m_process->start(pathStr,
			 {QStringLiteral("-mode=server"), QStringLiteral("-srtla-port=") + QString::number(m_port),
			  QStringLiteral("-srt-port=") + QString::number(8890),
			  QStringLiteral("-streamId=") + QString::fromStdString(streamKey)});
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

void GoIRL_Process::onProcessStarted() {
	emit serverStarted();
}

void GoIRL_Process::onProcessErrorOccurred(const QProcess::ProcessError error) {
	if (error != QProcess::FailedToStart) {
		return;
	}

	obs_log(LOG_ERROR, "failed to start go-irl: %s", qUtf8Printable(m_process->errorString()));
	cleanupProcess();
	emit serverError(ServerError::FailedToStart);
}

void GoIRL_Process::onProcessFinished(const int exitCode, const QProcess::ExitStatus exitStatus) {
	const bool requested = m_stopRequested;
	obs_log(requested ? LOG_INFO : LOG_ERROR, "go-irl exited with code %d (%s)", exitCode,
		exitStatus == QProcess::NormalExit ? "normal exit" : "crashed");
	cleanupProcess();

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
