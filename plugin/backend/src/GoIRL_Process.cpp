#include <GoIRL_Process.h>

#include <obs-module.h>
#include <plugin-support.h>

#include <filesystem>
#include <util/bmem.h>

static constexpr int c_stopTimeoutMs = 5000;
static constexpr std::size_t c_minPassphraseLength = 10;

GoIRL_Process::GoIRL_Process(const uint16_t srtPort) : m_srtPort(srtPort) {}

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
	if (streamKey.size() < c_minPassphraseLength) {
		obs_log(LOG_ERROR, "go-irl passphrase must be at least %zu characters", c_minPassphraseLength);
		emit serverError(ServerError::IncorrectInput);
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
	const QString pathStr(path.u16string());
	bfree(serverPath);

	m_stopRequested = false;
	m_process = new QProcess(this);
	connect(m_process, &QProcess::started, this, &GoIRL_Process::onProcessStarted);
	connect(m_process, &QProcess::errorOccurred, this, &GoIRL_Process::onProcessErrorOccurred);
	connect(m_process, &QProcess::finished, this, &GoIRL_Process::onProcessFinished);

	m_process->start(pathStr,
			 {QStringLiteral("-mode=server"), QStringLiteral("-srt-port=") + QString::number(m_srtPort),
			  QStringLiteral("-passphrase=") + QString::fromStdString(streamKey)});
}

void GoIRL_Process::stopServer() {
	if (m_process == nullptr) {
		return;
	}

	m_stopRequested = true;
	m_process->kill();
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

void GoIRL_Process::onProcessFinished(const int /*exitCode*/, const QProcess::ExitStatus /*exitStatus*/) {
	const bool requested = m_stopRequested;
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
