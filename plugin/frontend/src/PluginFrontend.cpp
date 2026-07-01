#include <PluginFrontend.h>
#include <PluginDock.h>

PluginFrontend::PluginFrontend(QMainWindow *window) : m_window(window) {
	m_pluginDock = new PluginDock(m_window);
	m_sourceSelectorWindow = std::make_unique<SourceSelectorWindow>(m_window);

	showSourceSelectorWindow();
}

PluginFrontend::~PluginFrontend() = default;

void PluginFrontend::showSourceSelectorWindow() {
	if (m_sourceSelectorWindow->isVisible()) {
		return;
	}

	m_sourceSelectorWindow->refreshSourceList();
	m_sourceSelectorWindow->show();
}
