#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <cstdint>
#include <string>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class GoIRL_Process final : public QObject {
	Q_OBJECT

public:
	enum class ServerError { FailedToStart, IncorrectInput, Crashed };
	GoIRL_Process() = delete;
	GoIRL_Process(const GoIRL_Process &) = delete;
	GoIRL_Process &operator=(const GoIRL_Process &) = delete;

	explicit GoIRL_Process(uint16_t port, const QString &executablePath);
	~GoIRL_Process() override;

	void startServer(const std::string &streamKey);
	void stopServer();
	std::string getStreamUrl(const std::string &publicAddress);

	NO_DISCARD uint16_t localSrtPort() const;
	NO_DISCARD bool running() const;

signals:
	void serverStarted();
	void serverStopped();
	void serverError(ServerError error);

private:
	void onProcessErrorOccurred(QProcess::ProcessError error);
	void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void readProcessError();
	void terminateProcess();
	void cleanupProcess();

	QString m_executablePath;
	QProcess *m_process{nullptr};
	QByteArray m_stderrBuffer;
	uint16_t m_port{0};
	uint16_t m_localSrtPort{0};
	bool m_stopRequested{false};
	bool m_listening{false};
};
