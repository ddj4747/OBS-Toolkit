#include <PluginFrontend.h>
#include <PluginDock.h>
#include <plugin-support.h>

PluginFrontend *PluginFrontend::s_instance{nullptr};

void PluginFrontend::start() {
	if (s_instance) {
		return;
	}

	void *mainWindow = obs_frontend_get_main_window();
	if (!mainWindow) {
		obs_log(LOG_WARNING, "failed to start frontend: main window unavailable");
		return;
	}

	s_instance = new PluginFrontend(static_cast<QMainWindow *>(mainWindow));
	obs_log(LOG_INFO, "frontend initialized");
}

void PluginFrontend::stop() {
	if (!s_instance) {
		return;
	}

	delete s_instance;
	s_instance = nullptr;
}

PluginFrontend *PluginFrontend::get() {
	assert(s_instance);
	return s_instance;
}

bool PluginFrontend::isRunning() {
	return s_instance;
}

PluginFrontend::PluginFrontend(QMainWindow *window) : m_window(window) {
	m_pluginDock = new PluginDock(m_window);
	m_sourceSelectorWindow = new SourceSelectorWindow(m_window);

	showSourceSelectorWindow();
}

PluginFrontend::~PluginFrontend() = default;

void PluginFrontend::hideSourceSelectorWindow() const {
	if (!m_sourceSelectorWindow->isVisible()) {
		return;
	}

	m_sourceSelectorWindow->hide();
}

void PluginFrontend::showSourceSelectorWindow() const {
	if (m_sourceSelectorWindow->isVisible()) {
		return;
	}

	m_sourceSelectorWindow->refreshSourceList();
	m_sourceSelectorWindow->show();
}

void PluginFrontend::getAddedSources(QList<QString> &sources) const {
	m_pluginDock->getSourcesList(sources);
}
