#include <PluginFrontend.h>
#include <PluginDock.h>

PluginFrontend::PluginFrontend(QMainWindow *window) : m_window(window) {
	m_pluginDock = new PluginDock(m_window);
}

PluginFrontend::~PluginFrontend() = default;
