#pragma once

#include <QMainWindow>
#include <PluginDock.h>
#include <SourceSelectorWindow.h>

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

	void hideSourceSelectorWindow() const;

	void showSourceSelectorWindow() const;

	NO_DISCARD const QList<QString> &getAddedSources() const;

private:
	static PluginFrontend *s_instance;

	PluginDock *m_pluginDock;
	SourceSelectorWindow *m_sourceSelectorWindow;
};
