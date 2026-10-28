#pragma once

#include <RelayConfig.h>
#include <QString>

namespace MediaMtxConfig {
inline constexpr uint16_t apiPort = 9997;

bool write(const QString &path, const RelayConfig &config, QString &error);
} // namespace MediaMtxConfig
