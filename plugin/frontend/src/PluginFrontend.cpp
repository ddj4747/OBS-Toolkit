#include <PluginFrontend.h>
#include <PluginDock.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>

PluginFrontend *PluginFrontend::s_instance{nullptr};

namespace {
void ensureConfigDirectoryExists() {
	char *configDir = obs_module_config_path("");
	if (!configDir) {
		return;
	}

	os_mkdirs(configDir);
	bfree(configDir);
}
} // namespace

void PluginFrontend::start() {
	if (s_instance) {
		return;
	}

	void *mainWindow = obs_frontend_get_main_window();
	if (!mainWindow) {
		obs_log(LOG_WARNING, "failed to start frontend: main window unavailable");
		return;
	}

	new PluginFrontend(static_cast<QMainWindow *>(mainWindow));
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
	s_instance = this;
	m_pluginDock = new PluginDock(window);
	m_sourceSelectorWindow = new SourceSelectorWindow(window);
}

PluginFrontend::~PluginFrontend() {
	if (m_sourceSelectorWindow) {
		m_sourceSelectorWindow->hide();
		m_sourceSelectorWindow->detach();
		m_sourceSelectorWindow = nullptr;
	}
	if (m_pluginDock) {
		m_pluginDock->detach();
		m_pluginDock = nullptr;
	}
	if (m_settings) {
		obs_data_release(m_settings);
		m_settings = nullptr;
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

obs_data_t *PluginFrontend::getSettingsObject() {
	if (m_settings) {
		return m_settings;
	}

	ensureConfigDirectoryExists();

	char *settingsPath = obs_module_config_path(QString("%1_config.json").arg(PLUGIN_NAME).toUtf8());
	assert(settingsPath != nullptr);

	m_settings = obs_data_create_from_json_file(settingsPath);
	if (!m_settings) {
		m_settings = obs_data_create();
	}

	bfree(settingsPath);
	return m_settings;
}

void PluginFrontend::saveSettingsObject() const {
	if (!m_settings) {
		return;
	}

	ensureConfigDirectoryExists();

	char *settingsPath = obs_module_config_path(QString("%1_config.json").arg(PLUGIN_NAME).toUtf8());
	assert(settingsPath != nullptr);

	if (!obs_data_save_json_safe(m_settings, settingsPath, "tmp", "bak")) {
		obs_log(LOG_ERROR, "Failed to save settings to %s", settingsPath);
	}

	bfree(settingsPath);
}

const QList<QString> &PluginFrontend::getAddedSources() const {
	return m_pluginDock->getSourcesList();
}
