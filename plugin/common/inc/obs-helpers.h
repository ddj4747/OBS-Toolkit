#pragma once

#include <QIcon>
#include <callback/calldata.h>
#include <functional>

struct obs_source;

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

namespace obs_helpers {
QIcon getIconFromSource(const obs_source *source);
QIcon getIconFromPath(const QString &path);
NO_DISCARD uint64_t connectSourceEditSignals(std::function<void(calldata_t *cd)> callback);
void disconnectSourceEditSignals(uint64_t id);
} // namespace obs_helpers
