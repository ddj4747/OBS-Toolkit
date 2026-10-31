#pragma once

#include <QString>
#include <cstdint>

namespace MediaMtxConfig {
inline constexpr uint16_t apiPort = 9997;

bool write(const QString &path, QString &error);
} // namespace MediaMtxConfig
