#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <obs-frontend-api.h>
#include <SourceSelectorWindow.h>

class PluginDock final : public QWidget {
	Q_OBJECT

public:
	PluginDock() = delete;

	explicit PluginDock(QWidget *parent);

	~PluginDock() override;

	PluginDock(const PluginDock &) = delete;

	void getSourcesList(QList<QString> &sources);
};
