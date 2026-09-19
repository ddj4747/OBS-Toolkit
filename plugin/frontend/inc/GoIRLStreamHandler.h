#pragma once

#include <StreamHandler.h>

#include <GoIRL_Process.h>

class GoIRLStreamHandler final : public StreamHandler {
	Q_OBJECT

public:
	explicit GoIRLStreamHandler(QObject *parent = nullptr);
	~GoIRLStreamHandler() override;

	void start(const std::string &streamId, StreamProtocol protocol, const std::string &publicAddress,
		   uint16_t port) override;
	void stop(const std::string &streamId) override;

private:
	GoIRL_Process *m_process{nullptr};
	std::string m_streamId;
	std::string m_publicAddress;
};
