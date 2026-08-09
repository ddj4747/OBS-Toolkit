#include <PluginDock.h>
#include <PluginSource.h>
#include <obs-helpers.h>
#include <FrontendEvents.h>
#include <PluginFrontend.h>
#include <EventManager.h>
#include <plugin-support.h>
#include <QFrame>
#include <QMainWindow>
#include <QItemSelectionModel>
#include <QStyle>
#include <util/config-file.h>

#include <algorithm>

namespace {
const std::string PLUGIN_DOCK_ID = std::string(PLUGIN_NAME) + "_mainDock";
const QString SETTINGS_ICON_PATH = QStringLiteral("settings/general.svg");
const QString ARROW_UP_ICON_PATH = QStringLiteral("up.svg");
const QString ARROW_DOWN_ICON_PATH = QStringLiteral("down.svg");

void restoreSavedDockLayout() {
	config_t *userConfig = obs_frontend_get_user_config();
	if (!userConfig) {
		return;
	}

	const char *dockStateStr = config_get_string(userConfig, "BasicWindow", "DockState");
	if (!dockStateStr) {
		return;
	}

	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow) {
		return;
	}

	const QByteArray dockState = QByteArray::fromBase64(QByteArray(dockStateStr));
	mainWindow->restoreState(dockState);
}
} // namespace

PluginDock::PluginDock(QWidget *parent)
	: QWidget(parent),
	  m_layout(new QVBoxLayout(this)),
	  m_sourcesListWidget(new QListWidget(this)),
	  m_toolbar(new QToolBar(this)) {

	m_toolbar->addSeparator();

	m_settingsAction = m_toolbar->addAction(obs_helpers::getIconFromPath(SETTINGS_ICON_PATH), "Settings", this,
						&PluginDock::onSettingsClicked);

	m_toolbar->addSeparator();

	m_moveUpSourceAction = m_toolbar->addAction(obs_helpers::getIconFromPath(ARROW_UP_ICON_PATH),
						    "Move Source Upward", this, &PluginDock::onMoveUpSourceClicked);

	m_moveDownSourceAction = m_toolbar->addAction(obs_helpers::getIconFromPath(ARROW_DOWN_ICON_PATH),
						      "Move Source Downward", this,
						      &PluginDock::onMoveDownSourceClicked);

	m_moveDownSourceAction->setEnabled(false);
	m_moveUpSourceAction->setEnabled(false);

	m_sourcesListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_sourcesListWidget->setIconSize(QSize(18, 18));
	m_sourcesListWidget->setFrameShape(QFrame::NoFrame);
	m_sourcesListWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	m_layout->setContentsMargins(1, 0, 1, 1);
	m_layout->setSpacing(0);
	m_layout->addWidget(m_sourcesListWidget, 1);
	m_layout->addWidget(m_toolbar, 0);

	m_toolbar->setObjectName("sourcesToolbar");
	m_toolbar->setIconSize(QSize(16, 16));
	m_toolbar->setFloatable(false);
	m_toolbar->setMovable(false);
	m_toolbar->setContentsMargins(0, 0, 0, 0);
	m_toolbar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	m_toolbar->setFixedHeight(30);

	setObjectName(PLUGIN_DOCK_ID.c_str());

	setAttribute(Qt::WA_StyledBackground, true);
	setStyleSheet(QString("#%1 {"
			      "  background-color: palette(base);"
			      "  border: 1px solid palette(mid);"
			      "  border-radius: 4px;"
			      "}"
			      "#%1 QListWidget { background: transparent; }"
			      "#sourcesToolbar { background: transparent; border: none; }")
			      .arg(PLUGIN_DOCK_ID.c_str()));

	if (m_obsDockRegistered = obs_frontend_add_dock_by_id(PLUGIN_DOCK_ID.c_str(), PLUGIN_NAME, this);
	    !m_obsDockRegistered) {
		obs_log(LOG_WARNING, "failed to register dock '%s'", PLUGIN_DOCK_ID.c_str());
		return;
	}

	restoreSavedDockLayout();

	connect(m_sourcesListWidget, &QListWidget::itemSelectionChanged, this,
		&PluginDock::onSourcesListSelectionChanged);
	loadSourcesList();

	EventManager::get()->addFrontendEventListener(this);

	obs_frontend_add_event_callback(onFrontendEvent, static_cast<void *>(this));

	m_sourceModificationSignalKey = obs_helpers::connectSourceEditSignals([this](const calldata_t *cd) {
		syncTrackedSourceNames(cd);
		updateSourcesList();
	});
}

PluginDock::~PluginDock() {
	detach();
}

void PluginDock::detach() {
	if (m_detached)
		return;
	m_detached = true;
	obs_helpers::disconnectSourceEditSignals(m_sourceModificationSignalKey);
	m_sourceModificationSignalKey = 0;
	EventManager::get()->removeFrontendEventListener(this);
	obs_frontend_remove_event_callback(onFrontendEvent, static_cast<void *>(this));
}

void PluginDock::prepareForShutdown() {
	detach();
	saveSourcesList();
}

const QList<QString> &PluginDock::getSourcesList() const {
	return m_sourcesList;
}

bool PluginDock::event(QEvent *event) {
	return QWidget::event(event);
}

void PluginDock::syncTrackedSourceNames(const calldata_t *cd) {
	if (!cd) {
		return;
	}

	const char *prevName = calldata_string(cd, "prev_name");
	const char *newName = calldata_string(cd, "new_name");
	if (prevName && newName) {
		const qsizetype i = m_sourcesList.indexOf(QString::fromUtf8(prevName));
		if (i >= 0) {
			m_sourcesList[i] = QString::fromUtf8(newName);
		}
	}
}

void PluginDock::updateSourcesList() {
	if (m_detached) {
		return;
	}

	m_sourcesListWidget->clear();

	obs_enum_sources(
		// ReSharper disable once CppParameterMayBeConstPtrOrRef
		[](void *data, obs_source_t *source) -> bool {
			PluginDock *dock = static_cast<PluginDock *>(data);
			if (obs_source_removed(source)) {
				return true;
			}

			const char *id = obs_source_get_id(source);
			if (!id || strcmp(id, PluginSource::id()) != 0) {
				return true;
			}

			const QString name = obs_source_get_name(source);
			if (dock->m_sourcesList.contains(name)) {
				return true;
			}

			dock->m_sourcesList.append(name);
			return true;
		},
		this);

	for (qsizetype i = 0; i < m_sourcesList.size(); i++) {
		const QString &source = m_sourcesList.at(i);
		obs_source_t *sourcePtr = obs_get_source_by_name(source.toUtf8());

		if (!sourcePtr) {
			m_sourcesList.removeAt(i);
			i--;
			continue;
		}

		const QIcon icon = obs_helpers::getIconFromSource(sourcePtr);
		new QListWidgetItem(icon, source, m_sourcesListWidget);

		obs_source_release(sourcePtr);
	}

	updateMinimumDockWidth();
	saveSourcesList();
}

void PluginDock::updateMinimumDockWidth() {
	int contentWidth = m_toolbar->sizeHint().width();

	if (m_sourcesListWidget->count() > 0) {
		contentWidth = std::max(contentWidth, m_sourcesListWidget->sizeHintForColumn(0));
	}

	const QMargins margins = m_layout->contentsMargins();
	contentWidth += margins.left() + margins.right() + 2;

	setMinimumWidth(contentWidth);
}

void PluginDock::updateActionIcons() const {
	m_settingsAction->setIcon(obs_helpers::getIconFromPath(SETTINGS_ICON_PATH));
	m_moveUpSourceAction->setIcon(obs_helpers::getIconFromPath(ARROW_UP_ICON_PATH));
	m_moveDownSourceAction->setIcon(obs_helpers::getIconFromPath(ARROW_DOWN_ICON_PATH));
}

void PluginDock::saveSourcesList() {
	obs_log(LOG_INFO, "Saving sources list");

	obs_data_t *settings = PluginFrontend::get()->getSettingsObject();
	obs_data_array_t *obsArray = obs_data_array_create();

	for (const auto &source : m_sourcesList) {
		obs_data_t *item = obs_data_create();
		obs_data_set_string(item, "value", source.toUtf8().constData());
		obs_data_array_push_back(obsArray, item);
		obs_data_release(item);
	}

	obs_data_set_array(settings, "sources", obsArray);
	obs_data_array_release(obsArray);

	PluginFrontend::get()->saveSettingsObject();
}

void PluginDock::loadSourcesList() {
	obs_log(LOG_INFO, "Loading sources list");
	m_sourcesList.clear();

	obs_data_t *settings = PluginFrontend::get()->getSettingsObject();
	obs_data_array_t *obsArray = obs_data_get_array(settings, "sources");
	if (!obsArray) {
		return;
	}

	const size_t count = obs_data_array_count(obsArray);
	m_sourcesList.resize(static_cast<qsizetype>(count));

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(obsArray, i);
		const char *str_val = obs_data_get_string(item, "value");
		if (str_val) {
			m_sourcesList[static_cast<qsizetype>(i)] = QString::fromUtf8(str_val);
		}

		obs_data_release(item);
	}

	obs_data_array_release(obsArray);
	updateSourcesList();
}

void PluginDock::onSettingsClicked() {
	if (!PluginFrontend::isRunning()) {
		return;
	}

	PluginFrontend::get()->showSettingsWindow();
}

void PluginDock::onSourcesListSelectionChanged() const {
	const QList<QListWidgetItem *> selected = m_sourcesListWidget->selectedItems();
	m_moveDownSourceAction->setEnabled(!selected.empty());
	m_moveUpSourceAction->setEnabled(!selected.empty());
}

void PluginDock::onMoveDownSourceClicked() {
	const QList<QListWidgetItem *> selected = m_sourcesListWidget->selectedItems();
	if (selected.isEmpty()) {
		return;
	}

	QList<QString> selectedNames;
	selectedNames.reserve(selected.size());
	for (const QListWidgetItem *item : selected) {
		selectedNames.append(item->text());
	}

	for (const QString &name : selectedNames) {
		const qsizetype index = m_sourcesList.indexOf(name);
		if (index < 0 || index >= m_sourcesList.size() - 1) {
			continue;
		}

		m_sourcesList.swapItemsAt(index, index + 1);
		break;
	}

	updateSourcesList();

	for (const QString &name : selectedNames) {
		const qsizetype index = m_sourcesList.indexOf(name);
		if (index >= 0) {
			m_sourcesListWidget->setCurrentRow(static_cast<int>(index), QItemSelectionModel::Select);
		}
	}
}

void PluginDock::onFrontendEvent(const obs_frontend_event event, void *data) {
	if (event == OBS_FRONTEND_EVENT_THEME_CHANGED) {
		PluginDock *dock = static_cast<PluginDock *>(data);

		dock->updateSourcesList();
		dock->updateActionIcons();
	}
}

void PluginDock::onMoveUpSourceClicked() {
	const QList<QListWidgetItem *> selected = m_sourcesListWidget->selectedItems();
	if (selected.isEmpty()) {
		return;
	}

	QList<QString> selectedNames;
	selectedNames.reserve(selected.size());
	for (const QListWidgetItem *item : selected) {
		selectedNames.append(item->text());
	}

	for (const QString &name : selectedNames) {
		const qsizetype index = m_sourcesList.indexOf(name);
		if (index <= 0) {
			continue;
		}

		m_sourcesList.swapItemsAt(index, index - 1);
		break;
	}

	updateSourcesList();

	for (const QString &name : selectedNames) {
		const qsizetype index = m_sourcesList.indexOf(name);
		if (index >= 0) {
			m_sourcesListWidget->setCurrentRow(static_cast<int>(index), QItemSelectionModel::Select);
		}
	}
}
