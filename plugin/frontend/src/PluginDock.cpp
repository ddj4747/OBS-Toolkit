#include <PluginDock.h>
#include <obs-helpers.h>
#include <FrontendEvents.h>
#include <PluginFrontend.h>
#include <EventManager.h>
#include <plugin-support.h>
#include <QFrame>

namespace {
const std::string PLUGIN_DOCK_ID = std::string(PLUGIN_NAME) + "_mainDock";
} // namespace

PluginDock::PluginDock(QWidget *parent)
	: QWidget(parent),
	  m_layout(new QVBoxLayout(this)),
	  m_sourcesListWidget(new QListWidget(this)),
	  m_toolbar(new QToolBar(this)) {

	m_addSourceAction = m_toolbar->addAction(obs_helpers::getIconFromPath("plus.svg"), "AddSource", this,
						 &PluginDock::onAddSourceClicked);
	m_removeSourceAction = m_toolbar->addAction(obs_helpers::getIconFromPath("trash.svg"), "RemoveSource", this,
						    &PluginDock::onRemoveSourceClicked);

	m_toolbar->addSeparator();

	m_settingsAction = m_toolbar->addAction(obs_helpers::getIconFromPath("settings/general.svg"), "Settings", this,
						&PluginDock::onSettingsClicked);

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

	EventManager::get()->addFrontendEventListener(this);
	m_sourceModificationSignalKey = obs_helpers::connectSourceEditSignals([this](calldata_t *cd) {
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
		const int i = m_sourcesList.indexOf(QString::fromUtf8(prevName));
		if (i >= 0) {
			m_sourcesList[i] = QString::fromUtf8(newName);
		}
		return;
	}

	const obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!source || !obs_source_removed(source)) {
		return;
	}

	m_sourcesList.removeOne(QString::fromUtf8(obs_source_get_name(source)));
}

void PluginDock::updateSourcesList() {
	m_sourcesListWidget->clear();

	for (const auto &source : m_sourcesList) {
		obs_source_t *sourcePtr = obs_get_source_by_name(source.toUtf8());
		const QIcon icon = obs_helpers::getIconFromSource(sourcePtr);

		obs_source_release(sourcePtr);
		new QListWidgetItem(icon, source, m_sourcesListWidget);
	}
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
