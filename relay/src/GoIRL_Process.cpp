#include <GoIRL_Process.h>


#include <QByteArray>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <QThread>
#include <QUdpSocket>

static constexpr int c_stopTimeoutMs = 5000;

GoIRL_Process::GoIRL_Process(const uint16_t port, const QString &executablePath)
	: m_executablePath(executablePath), m_port(port) {
	QUdpSocket socket;
	if (socket.bind(QHostAddress::AnyIPv4, 0))
		m_localSrtPort = socket.localPort();
}

GoIRL_Process::~GoIRL_Process() {
	if (m_process != nullptr) {
		(void)m_process->disconnect(this);
		m_process->kill();
		m_process->waitForFinished(c_stopTimeoutMs);
		m_process = nullptr;
	}

	m_localSrtPort = 0;
}

void GoIRL_Process::startServer(const std::string &streamKey) {
	if (QThread::currentThread() != thread()) {
		QMetaObject::invokeMethod(this, [this, streamKey] { startServer(streamKey); }, Qt::QueuedConnection);
		return;
	}


	if (m_localSrtPort == 0) {
		spdlog::error("failed to allocate a local SRT port");
		emit serverError(ServerError::FailedToStart);
		return;
	}

	if (m_process != nullptr) {
		terminateProcess();
	}

	m_listening = false;
	m_stderrBuffer.clear();


	m_stopRequested = false;
	m_process = new QProcess(this);
	connect(m_process, &QProcess::errorOccurred, this, &GoIRL_Process::onProcessErrorOccurred);
	connect(m_process, &QProcess::finished, this, &GoIRL_Process::onProcessFinished);
	connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
		const QByteArray output = m_process->readAllStandardOutput().trimmed();
		if (!output.isEmpty())
			spdlog::info("go-irl: {}", output.constData());
	});
	connect(m_process, &QProcess::readyReadStandardError, this, &GoIRL_Process::readProcessError);

	const QString streamIdArgument = QString::fromUtf8(streamKey.data(), static_cast<qsizetype>(streamKey.size()));
	spdlog::info("starting go-irl SRTLA on {}, local SRT on {}", m_port, m_localSrtPort);
	m_process->start(m_executablePath, {QStringLiteral("-mode=server"), QStringLiteral("-cli"), QStringLiteral("-insecure"),
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

	spdlog::error("failed to start go-irl: {}", qUtf8Printable(m_process->errorString()));
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

		spdlog::warn("go-irl: {}", line.constData());
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
		spdlog::warn("go-irl: {}", trailing.constData());
	}

	const bool requested = m_stopRequested;
	const bool listening = m_listening;
	m_listening = false;
	spdlog::log(requested ? spdlog::level::info : spdlog::level::err, "go-irl exited with code {} ({})", exitCode,
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
