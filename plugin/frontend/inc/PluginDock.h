#pragma once

#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
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

	void detach();

	void prepareForShutdown();

protected:
	bool event(QEvent *event) override;

private slots:
	static void onSettingsClicked();
	static void onAddSourceClicked();
	void onRemoveSourceClicked();
	void onSourcesListSelectionChanged() const;
	void onMoveUpSourceClicked();
	void onMoveDownSourceClicked();

private:
	static void onFrontendEvent(obs_frontend_event event, void *data);

	void syncTrackedSourceNames(const calldata_t *cd);
	void updateSourcesList();
	void updateMinimumDockWidth();
	void updateActionIcons() const;
	void updateAddSourceButtonState() const;

	void saveSourcesList();
	void loadSourcesList();

	QList<QString> m_sourcesList;
	uint64_t m_sourceModificationSignalKey{0};
	bool m_obsDockRegistered{false};
	bool m_detached{false};

	QVBoxLayout *m_layout;
	QListWidget *m_sourcesListWidget;
	QToolBar *m_toolbar;

	QAction *m_settingsAction{nullptr};
	QAction *m_addSourceAction{nullptr};
	QAction *m_removeSourceAction{nullptr};
	QAction *m_moveUpSourceAction{nullptr};
	QAction *m_moveDownSourceAction{nullptr};
};
