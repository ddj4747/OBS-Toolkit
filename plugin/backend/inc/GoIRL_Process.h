#pragma once

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

	explicit GoIRL_Process(uint16_t srtPort);
	~GoIRL_Process() override;

	void startServer(const std::string &streamKey);
	void stopServer();
	NO_DISCARD bool running() const;

signals:
	void serverStarted();
	void serverStopped();
	void serverError(ServerError error);

private:
	void onProcessStarted();
	void onProcessErrorOccurred(QProcess::ProcessError error);
	void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void terminateProcess();
	void cleanupProcess();

	QProcess *m_process{nullptr};
	uint16_t m_srtPort{};
	bool m_stopRequested{false};
};
