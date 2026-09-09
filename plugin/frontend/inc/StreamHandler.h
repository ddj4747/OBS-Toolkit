#pragma once

#include <MediamtxManager.h>

#include <QObject>
#include <QString>
#include <cstdint>
#include <string>

class StreamHandler : public QObject {
	Q_OBJECT

public:
	explicit StreamHandler(QObject *parent = nullptr) : QObject(parent) {}
	~StreamHandler() override = default;

	StreamHandler(const StreamHandler &) = delete;
	StreamHandler &operator=(const StreamHandler &) = delete;

	virtual void start(const std::string &streamId, Protocol protocol, const std::string &publicAddress,
			   uint16_t port) = 0;
	virtual void stop(const std::string &streamId) = 0;

signals:
	void streamReady(const QString &streamId, const QString &publishUrl);
	void streamAvailable(const QString &streamId);
	void streamUnavailable(const QString &streamId);
	void streamStopped(const QString &streamId);
	void streamError(const QString &streamId, const QString &error);
};
