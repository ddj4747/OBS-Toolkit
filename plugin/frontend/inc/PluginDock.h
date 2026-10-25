#pragma once

#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QIcon>
#include <QListWidgetItem>
#include <QString>
#include <QTimer>
#include <obs-frontend-api.h>

#include <string>

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

	void updateSourcesList();

protected:
	bool event(QEvent *event) override;

private slots:
	static void onSettingsClicked();
	void onWebPanelClicked();
	void onSourcesListSelectionChanged() const;
	void onMoveUpSourceClicked();
	void onMoveDownSourceClicked();

private:
	static void onFrontendEvent(obs_frontend_event event, void *data);

	void syncTrackedSourceNames(const calldata_t *cd);
	void updateMinimumDockWidth();
	void updateStatusIcons() const;
	void updateActionIcons() const;
	void saveSourcesList();
	void loadSourcesList();

	static void restoreSavedDockLayout();
	static QIcon streamStatusIcon(bool active);

	static const std::string PLUGIN_DOCK_ID;
	static const QString SETTINGS_ICON_PATH;
	static const QString WEB_PANEL_ICON_PATH;
	static const QString ARROW_UP_ICON_PATH;
	static const QString ARROW_DOWN_ICON_PATH;

	static constexpr int TYPE_ICON_ROLE = Qt::UserRole;
	static constexpr int STATUS_ICON_ROLE = Qt::UserRole + 1;
	static constexpr int STATUS_ACTIVE_ROLE = Qt::UserRole + 2;
	static constexpr int STATUS_REFRESH_INTERVAL_MS = 500;

	static constexpr int ROW_MARGIN = 6;
	static constexpr int ROW_ICON_SIZE = 16;
	static constexpr int ROW_SPACING = 8;
	static constexpr int STATUS_SECTION_PADDING = 8;
	static constexpr int STATUS_SECTION_WIDTH = ROW_ICON_SIZE + 2 * STATUS_SECTION_PADDING;

	class SourceRowDelegate;

	QList<QString> m_sourcesList;
	uint64_t m_sourceModificationSignalKey{0};
	bool m_obsDockRegistered{false};
	bool m_detached{false};

	QVBoxLayout *m_layout;
	QListWidget *m_sourcesListWidget;
	QToolBar *m_toolbar;
	QTimer *m_statusRefreshTimer;

	QAction *m_settingsAction{nullptr};
	QAction *m_sourceSettingsAction{nullptr};
	QAction *m_moveUpSourceAction{nullptr};
	QAction *m_moveDownSourceAction{nullptr};
	QAction *m_webPanelSettingsAction{nullptr};
};
