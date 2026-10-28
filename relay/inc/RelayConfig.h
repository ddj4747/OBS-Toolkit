#pragma once

#include <QString>
#include <array>
#include <cstdint>

struct RelayConfig {
	uint16_t apiPort = 8080;
	uint16_t srtlaPort = 5000;
	uint16_t srtPort = 8890;
	uint16_t rtspPort = 8554;
	uint16_t rtpPort = 8000;
	uint16_t rtcpPort = 8001;
	uint16_t rtmpPort = 1935;
	uint16_t webRtcPort = 8889;
	uint16_t moqPort = 8892;
	uint16_t moqQuicPort = 8893;

	static bool load(const QString &path, RelayConfig &config, QString &error);

private:
	struct PortSetting {
		const char *key;
		uint16_t RelayConfig::*member;
	};

	static const std::array<PortSetting, 10> &portSettings();
};
