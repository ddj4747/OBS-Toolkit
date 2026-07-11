#pragma once

#include <QVBoxLayout>
#include <QPushButton>
#include <QListWidgetItem>
#include <obs-frontend-api.h>
#include <QCloseEvent>
#include <SourcePartitionSelector.h>

class PluginSettingsWindow : public QWidget {
	Q_OBJECT
public:
	PluginSettingsWindow() = delete;

	explicit PluginSettingsWindow(QWidget *parent);

	~PluginSettingsWindow() override;

	PluginSettingsWindow(const PluginSettingsWindow &) = delete;

	PluginSettingsWindow(PluginSettingsWindow &&) = delete;

	PluginSettingsWindow &operator=(const PluginSettingsWindow &) = delete;

	PluginSettingsWindow &operator=(const PluginSettingsWindow &&) = delete;

private:
	SourcePartitionSelector *m_sourcePartitionSelector;
};
