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

	void showSourceSelectorWindow();

private:
	QMainWindow *m_window;
	PluginDock *m_pluginDock;
	std::unique_ptr<SourceSelectorWindow> m_sourceSelectorWindow;
};
