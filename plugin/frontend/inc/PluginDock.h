#pragma once

#include <QWidget>
#include <QUuid>

class PluginDock final : public QWidget {
	Q_OBJECT

public:
	PluginDock() = delete;

	explicit PluginDock(QWidget *parent);

	~PluginDock() override;
};