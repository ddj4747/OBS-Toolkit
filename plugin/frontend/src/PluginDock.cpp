#include <PluginDock.h>
#include <obs-helpers.h>
#include <FrontendEvents.h>
#include <PluginFrontend.h>
#include <EventManager.h>
#include <plugin-support.h>
#include <QFrame>
#include <QMainWindow>
#include <QItemSelectionModel>
#include <util/config-file.h>

namespace {
const std::string PLUGIN_DOCK_ID = std::string(PLUGIN_NAME) + "_mainDock";

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

	m_addSourceAction = m_toolbar->addAction(obs_helpers::getIconFromPath("plus.svg"), "Add New Sources", this,
						 &PluginDock::onAddSourceClicked);
	m_removeSourceAction = m_toolbar->addAction(obs_helpers::getIconFromPath("trash.svg"),
						    "Remove Selected Sources", this,
						    &PluginDock::onRemoveSourceClicked);

	m_toolbar->addSeparator();

	m_settingsAction = m_toolbar->addAction(obs_helpers::getIconFromPath("settings/general.svg"), "Settings", this,
						&PluginDock::onSettingsClicked);

	m_toolbar->addSeparator();

	m_moveUpSourceAction = m_toolbar->addAction(obs_helpers::getIconFromPath("up.svg"), "Move Source Upward", this,
						    &PluginDock::onMoveUpSourceClicked);

	m_moveDownSourceAction = m_toolbar->addAction(obs_helpers::getIconFromPath("down.svg"), "Move Source Downward",
						      this, &PluginDock::onMoveDownSourceClicked);

	m_removeSourceAction->setEnabled(false);
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
}

void PluginDock::prepareForShutdown() {
	detach();
	saveSourcesList();
}

const QList<QString> &PluginDock::getSourcesList() const {
	return m_sourcesList;
}

bool PluginDock::event(QEvent *event) {
	if (event->type() == SourceAddedEvent::type) {
		const SourceAddedEvent *sourceAddedEvent = dynamic_cast<const SourceAddedEvent *>(event);
		if (!sourceAddedEvent) {
			return false;
		}

		for (const auto &source : sourceAddedEvent->names()) {
			if (m_sourcesList.contains(source)) {
				continue;
			}

			m_sourcesList.push_back(source);
		}

		updateSourcesList();
	}

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

	saveSourcesList();
	updateAddSourceButtonState();
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

void PluginDock::updateAddSourceButtonState() const {
	const size_t dockSourcesCount = m_sourcesList.size();
	size_t sourceCount = 0;

	obs_enum_sources(
		[](void *param, obs_source_t *) -> bool {
			size_t *counter = static_cast<size_t *>(param);
			(*counter)++;

			return true;
		},
		&sourceCount);

	m_addSourceAction->setEnabled(dockSourcesCount != sourceCount);
}

void PluginDock::onSettingsClicked() {}

void PluginDock::onAddSourceClicked() {
	if (!PluginFrontend::isRunning()) {
		return;
	}

	PluginFrontend::get()->showSourceSelectorWindow();
}

void PluginDock::onRemoveSourceClicked() {
	const QList<QListWidgetItem *> selected = m_sourcesListWidget->selectedItems();
	QList<QString> names(selected.size());

	for (qsizetype i = 0; i < names.size(); ++i) {
		const auto *elem = selected[i];
		names[i] = elem->text();
		m_sourcesList.removeOne(elem->text());
	}

	QEvent *event = new SourceRemovedEvent(names);
	EventManager::get()->sendFrontendEvent(event);

	updateSourcesList();
}

void PluginDock::onSourcesListSelectionChanged() const {
	const QList<QListWidgetItem *> selected = m_sourcesListWidget->selectedItems();
	m_removeSourceAction->setEnabled(!selected.empty());
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
