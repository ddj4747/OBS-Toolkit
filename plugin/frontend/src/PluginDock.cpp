#include <PluginDock.h>
#include <SourceSelectorWindow.h>
#include <plugin-support.h>

namespace {
const std::string PLUGIN_DOCK_ID = std::string(PLUGIN_NAME) + "_mainDock";
} // namespace

PluginDock::PluginDock(QWidget *parent) : QWidget(parent) {
	setObjectName(PLUGIN_DOCK_ID.c_str());
	if (!obs_frontend_add_dock_by_id(PLUGIN_DOCK_ID.c_str(), PLUGIN_NAME, this)) {
		obs_log(LOG_WARNING, "failed to register dock '%s'", PLUGIN_DOCK_ID.c_str());
	}
}

PluginDock::~PluginDock() = default;

void PluginDock::getSourcesList(QList<QString> &sources) {
	sources.clear();
}
