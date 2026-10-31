#pragma once

#include <MediamtxManager.h>
#include <GoIRL_Process.h>
#include <QObject>
#include <QPointer>
#include <QString>
#include <string>

class FeedContext final : public QObject {
	Q_OBJECT

public:
	enum class State { Stopped, Starting, Running, Stopping, Failed };

	FeedContext(std::string streamKey, StreamProtocol protocol, std::string address, MediamtxManager *mediaManager,
		    GoIRL_Process *goIrl, QObject *parent = nullptr);

	const std::string &streamKey() const;
	StreamProtocol protocol() const;
	State state() const;
	QString link() const;

	void start();
	void stop();

signals:
	void started(const QString &link);
	void stopped();
	void error(const QString &message);
	void available();
	void unavailable();

private:
	void fail(const QString &message);
	bool matches(const QString &name) const;

	const std::string m_streamKey;
	const StreamProtocol m_protocol;
	const std::string m_address;
	QPointer<MediamtxManager> m_mediaManager;
	QPointer<GoIRL_Process> m_goIrl;
	State m_state{State::Stopped};
	QString m_link;
};
