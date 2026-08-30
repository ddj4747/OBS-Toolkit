#pragma once

#include <cstdint>
#include <string>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class QProcess;

class GoIRL_Process final {
public:
	GoIRL_Process() = delete;
	~GoIRL_Process();

	GoIRL_Process(const GoIRL_Process &) = delete;
	void operator=(const GoIRL_Process &) = delete;
	GoIRL_Process(GoIRL_Process &&) = delete;
	void operator=(GoIRL_Process &&) = delete;

	explicit GoIRL_Process(uint16_t srtPort);

	bool startServer(const std::string &streamKey);
	bool stopServer();
	NO_DISCARD bool running() const;

private:
	QProcess *m_process{nullptr};
	uint16_t m_srtPort{};
	std::string m_streamKey{};
};
