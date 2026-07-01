#pragma once

#include <QMainWindow>
#include <PluginDock.h>

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

	void hideSourceSelectorWindow() const;

	void showSourceSelectorWindow() const;

	void getAddedSources(QList<QString> &sources) const;

private:
	static PluginFrontend *s_instance;

	QMainWindow *m_window;
	PluginDock *m_pluginDock;
	SourceSelectorWindow *m_sourceSelectorWindow;
};
