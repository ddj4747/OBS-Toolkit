#include <PluginDock.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>

namespace {
const std::string PLUGIN_DOCK_ID = std::string(PLUGIN_NAME) + "_mainDock";
}

PluginDock::PluginDock(QWidget *parent)
	: QWidget(parent)
{
	setObjectName(PLUGIN_DOCK_ID.c_str());
	const bool result = obs_frontend_add_dock_by_id(PLUGIN_DOCK_ID.c_str(), PLUGIN_NAME, this);
	assert(!result);
	(void)result;
}

PluginDock::~PluginDock() = default;