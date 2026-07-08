#pragma once

#include <QMainWindow>
#include <PluginDock.h>
#include <SourceSelectorWindow.h>
#include <PluginSettingsWindow.h>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class PluginFrontend final {
public:
	PluginFrontend() = delete;

	explicit PluginFrontend(QMainWindow *window);

	~PluginFrontend();

	void operator=(const PluginFrontend &) = delete;

	void operator=(const PluginFrontend &&) = delete;

	PluginFrontend(const PluginFrontend &) = delete;

	PluginFrontend(PluginFrontend &&) = delete;

	static PluginFrontend *get();

	static void start();

	static void stop();

	static bool isRunning();

	void prepareForShutdown();

	void hideSourceSelectorWindow() const;

	void hideSettingsWindow() const;

	void showSourceSelectorWindow() const;

	void showSettingsWindow() const;

	obs_data_t *getSettingsObject();

	void saveSettingsObject() const;

	NO_DISCARD const QList<QString> &getAddedSources() const;

private:
	static PluginFrontend *s_instance;

	PluginDock *m_pluginDock;
	obs_data *m_settings{nullptr};
	SourceSelectorWindow *m_sourceSelectorWindow;
	PluginSettingsWindow *m_pluginSettingsWindow;

	bool m_shutdownPrepared{false};
};
