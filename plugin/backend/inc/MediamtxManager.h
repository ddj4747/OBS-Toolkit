#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <cstdint>
#include <string>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class MediamtxManager final : public QObject {
	Q_OBJECT

public:
	enum class Protocol : uint8_t { MoQ, SRT, WebRTC, RTSP, RTMP, HLS, MPEGTS, RTP };
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
	void addInput(const std::string &streamId, Protocol protocol, const std::string &ip);
	void removeInput(const std::string &streamId);

signals:
	void serverStarted();
	void serverStopped();
	void serverError(ServerError error);
	void inputAdded(const QString &name, const QString &publishUrl);
	void inputRemoved(const QString &name);
	void inputError(const QString &name, const QString &error);

private:
	void onProcessStarted();
	void onProcessErrorOccurred(QProcess::ProcessError error);
	void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
	void terminateProcess();
	void cleanupProcess();

	QProcess *m_process{nullptr};
	QNetworkAccessManager *m_networkAccessManager{nullptr};
	bool m_stopRequested{false};
};
