#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QListWidget>
#include <QListWidgetItem>
#include <obs-frontend-api.h>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class PluginDock final : public QWidget {
	Q_OBJECT

public:
	PluginDock() = delete;

	explicit PluginDock(QWidget *parent);

	~PluginDock() override;

	PluginDock(const PluginDock &) = delete;

	NO_DISCARD const QList<QString> &getSourcesList() const;

protected:
	bool event(QEvent *event) override;

private slots:
	void onSettingsClicked();
	static void onAddSourceClicked();
	void onRemoveSourceClicked();

private:
	void syncTrackedSourceNames(const calldata_t *cd);
	void updateSourcesList();

	QList<QString> m_sourcesList;
	uint64_t m_sourceModificationSignalKey{0};
	bool m_obsDockRegistered{false};

	QVBoxLayout *m_layout;
	QListWidget *m_sourcesListWidget;
	QToolBar *m_toolbar;
	QAction *m_settingsAction{nullptr};
	QAction *m_addSourceAction{nullptr};
	QAction *m_removeSourceAction{nullptr};
};
