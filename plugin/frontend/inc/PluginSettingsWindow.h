#pragma once

#include <QVBoxLayout>
#include <QScrollArea>
#include <obs-frontend-api.h>
#include <SourcePartitionSelector.h>
#include <PartitionSettingsWidget.h>

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

private slots:
	void onPartitionSelectorValueChanged();
	void onPartitionClicked(int partition);
	void onPartitionFocused(int partition);

private:
	void setFocusedPartition(int partition);

	SourcePartitionSelector *m_sourcePartitionSelector;
	QScrollArea *m_partitionScrollArea;
	QWidget *m_partitionScrollContent;
	QVBoxLayout *m_partitionSettingsLayout;
	QList<PartitionSettingsWidget *> m_partitionSettings;
	QVBoxLayout *m_mainLayout;
	int m_focusedPartitionSettings = -1;
	bool m_ignoreHoverFocus = false;
};
