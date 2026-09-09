#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <cstdint>
#include <deque>
#include <map>
#include <string>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

enum class Protocol : uint8_t { MoQ, SRT, WebRTC, RTSP, RTMP, HLS, MPEGTS, RTP, SRTLA };

class MediamtxManager final : public QObject {
	Q_OBJECT

public:
	enum class ServerError { FailedToStart, IncorrectInput, Crashed };

	MediamtxManager();
	MediamtxManager(const MediamtxManager &) = delete;
	MediamtxManager &operator=(const MediamtxManager &) = delete;
	MediamtxManager(MediamtxManager &&) = delete;
	MediamtxManager &operator=(MediamtxManager &&) = delete;
	~MediamtxManager() override;

	void startServer();
	void stopServer();
	NO_DISCARD bool running() const;
	NO_DISCARD bool ready() const;
	void addInput(const std::string &streamId, Protocol protocol, const std::string &ip);
	void removeInput(const std::string &streamId, Protocol protocol);

signals:
	void serverStarted();
	void serverStopped();
	void serverError(ServerError error);
	void inputAdded(const QString &name, const QString &publishUrl);
	void inputAvailable(const QString &name);
	void inputUnavailable(const QString &name);
	void inputRemoved(const QString &name);
	void inputError(const QString &name, const QString &error);

private:
	struct PendingInput {
		std::string streamId;
		Protocol protocol;
		std::string ip;
	};

	void onProcessStarted();
	void onProcessErrorOccurred(QProcess::ProcessError error);
	void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void addInputWhenReady(const std::string &streamId, Protocol protocol, const std::string &ip);
	void flushPendingInputs();
	void terminateProcess();
	void cleanupProcess();
	static QString pathName(const std::string &streamId, Protocol protocol);
	void pollInputAvailability();

	QProcess *m_process{nullptr};
	QNetworkAccessManager *m_networkAccessManager{nullptr};
	std::deque<PendingInput> m_pendingInputs;
	std::map<std::string, std::pair<Protocol, bool>> m_inputs;
	QTimer *m_inputStatusTimer{nullptr};
	bool m_stopRequested{false};
	bool m_apiReady{false};
};
