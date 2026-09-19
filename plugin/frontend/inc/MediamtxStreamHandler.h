#pragma once

#include <StreamHandler.h>

class MediamtxStreamHandler final : public StreamHandler {
	Q_OBJECT

public:
	explicit MediamtxStreamHandler(QObject *parent = nullptr);

	void start(const std::string &streamId, StreamProtocol protocol, const std::string &publicAddress,
		   uint16_t port) override;
	void stop(const std::string &streamId) override;

	static void prepareForShutdown();

private:
	static MediamtxManager *sharedManager();

	MediamtxManager *m_manager{nullptr};
	std::string m_streamId;
	StreamProtocol m_protocol{};
};
