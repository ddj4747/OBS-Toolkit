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

PluginFrontend::PluginFrontend(QMainWindow *window) {
	m_pluginDock = new PluginDock(window);
	m_sourceSelectorWindow = new SourceSelectorWindow(window);
}

PluginFrontend::~PluginFrontend() {
	if (m_sourceSelectorWindow) {
		m_sourceSelectorWindow->hide();
		delete m_sourceSelectorWindow;
		m_sourceSelectorWindow = nullptr;
	}
	if (m_pluginDock) {
		delete m_pluginDock;
		m_pluginDock = nullptr;
	}
}

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

	m_sourceSelectorWindow->show();
}

const QList<QString> &PluginFrontend::getAddedSources() const {
	return m_pluginDock->getSourcesList();
}
