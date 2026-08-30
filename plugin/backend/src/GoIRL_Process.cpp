#include <GoIRL_Process.h>

#include <obs-module.h>
#include <plugin-support.h>

#include <QProcess>
#include <filesystem>
#include <util/bmem.h>

static constexpr int c_startTimeoutMs = 5000;
static constexpr int c_stopTimeoutMs = 5000;
static constexpr std::size_t c_minPassphraseLength = 10;

GoIRL_Process::~GoIRL_Process() {
	(void)stopServer();
}

GoIRL_Process::GoIRL_Process(const uint16_t srtPort) : m_srtPort(srtPort) {}

bool GoIRL_Process::startServer(const std::string &streamKey) {
	if (streamKey.size() < c_minPassphraseLength) {
		obs_log(LOG_ERROR, "go-irl passphrase must be at least %zu characters", c_minPassphraseLength);
		return false;
	}

	(void)stopServer();

#ifdef WIN32
	char *serverPath = obs_module_file("go-irl.exe");
#else
	char *serverPath = obs_module_file("go-irl");
#endif

	if (serverPath == nullptr) {
		obs_log(LOG_ERROR, "go-irl binary was not found in the plugin data directory");
		return false;
	}

	const std::filesystem::path path(serverPath);
	const QString pathStr(path.u16string());
	bfree(serverPath);

	m_process = new QProcess();
	m_process->start(pathStr,
			 {QStringLiteral("-mode=server"), QStringLiteral("-srt-port=") + QString::number(m_srtPort),
			  QStringLiteral("-passphrase=") + QString::fromStdString(streamKey)});

	if (!m_process->waitForStarted(c_startTimeoutMs)) {
		obs_log(LOG_ERROR, "failed to start go-irl: %s", qUtf8Printable(m_process->errorString()));
		delete m_process;
		m_process = nullptr;
		return false;
	}

	m_streamKey = streamKey;
	return true;
}

bool GoIRL_Process::stopServer() {
	if (m_process == nullptr) {
		return false;
	}

	m_process->kill();
	if (!m_process->waitForFinished(c_stopTimeoutMs)) {
		obs_log(LOG_WARNING, "go-irl did not exit after kill");
	}

	delete m_process;
	m_process = nullptr;
	m_streamKey.clear();
	return true;
}

bool GoIRL_Process::running() const {
	return m_process != nullptr && m_process->state() == QProcess::Running;
}
