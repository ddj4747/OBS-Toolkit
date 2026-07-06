#include <SourceSelectorWindow.h>

#include <PluginFrontend.h>
#include <obs-helpers.h>
#include <plugin-support.h>
#include <FrontendEvents.h>
#include <EventManager.h>

SourceSelectorWindow::SourceSelectorWindow(QWidget *parent)
	: QWidget(parent, Qt::Window),
	  m_layout(new QVBoxLayout()),
	  m_buttonLayout(new QHBoxLayout()),
	  m_listWidget(new QListWidget(this)),
	  m_addButton(new QPushButton("Add", this)),
	  m_cancelButton(new QPushButton("Cancel", this)) {

	setWindowTitle(QString(PLUGIN_NAME) + " - Source Selector");
	m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_listWidget->setIconSize(QSize(18, 18));

	m_layout->addWidget(m_listWidget);
	m_layout->addLayout(m_buttonLayout);

	m_buttonLayout->addWidget(m_addButton);
	m_buttonLayout->addWidget(m_cancelButton);

	connect(m_addButton, &QPushButton::clicked, this, &SourceSelectorWindow::onAddButtonClicked);
	connect(m_cancelButton, &QPushButton::clicked, this, &SourceSelectorWindow::onCancelButtonClicked);

	setLayout(m_layout);

	EventManager::get()->addFrontendEventListener(this);
}

SourceSelectorWindow::~SourceSelectorWindow() {
	detach();
}

void SourceSelectorWindow::detach() {
	if (m_detached)
		return;
	m_detached = true;
	obs_helpers::disconnectSourceEditSignals(m_sourceModificationSignalKey);
	m_sourceModificationSignalKey = 0;
	EventManager::get()->removeFrontendEventListener(this);
}

void SourceSelectorWindow::refreshSourceList(const QList<QString> &excludedSources) {
	m_listWidget->clear();

	// ReSharper disable once CppLocalVariableMayBeConst
	size_t ptrTable[2]{reinterpret_cast<size_t>(this), reinterpret_cast<size_t>(&excludedSources)};

	obs_enum_sources(
		// ReSharper disable once CppParameterMayBeConstPtrOrRef
		[](void *data, obs_source_t *source) -> bool {
			void **table = static_cast<void **>(data);

			static_cast<SourceSelectorWindow *>(table[0])->processSourceCallback(
				source, static_cast<const QList<QString> *>(table[1]));

			return true;
		},
		reinterpret_cast<void *>(ptrTable));

	obs_log(LOG_INFO, "source selector refreshed with %d sources", m_listWidget->count());

	if (m_listWidget->count() > 0) {
		m_listWidget->setCurrentRow(0);
	}
}

bool SourceSelectorWindow::event(QEvent *event) {
	if (event->type() == SourceRemovedEvent::type) {
		const QList<QString> sources = PluginFrontend::isRunning() ? PluginFrontend::get()->getAddedSources()
									   : QList<QString>{};
		refreshSourceList(sources);
	}

	return QWidget::event(event);
}

void SourceSelectorWindow::processSourceCallback(const obs_source_t *source,
						 const QList<QString> *excludedSources) const {
	if (obs_source_removed(source)) {
		return;
	}

	const QString name = obs_source_get_name(source);
	if (excludedSources->contains(name)) {
		return;
	}

	const QIcon icon = obs_helpers::getIconFromSource(source);
	new QListWidgetItem(icon, name, m_listWidget);
}

void SourceSelectorWindow::showEvent(QShowEvent *event) {
	const QList<QString> excludedSources = PluginFrontend::isRunning() ? PluginFrontend::get()->getAddedSources()
									   : QList<QString>{};
	refreshSourceList(excludedSources);
	obs_log(LOG_INFO, "source selector opened with %d sources", m_listWidget->count());

	m_sourceModificationSignalKey = obs_helpers::connectSourceEditSignals([this](calldata_t *) {
		const QList<QString> sources = PluginFrontend::isRunning() ? PluginFrontend::get()->getAddedSources()
									   : QList<QString>{};
		refreshSourceList(sources);
	});

	QWidget::showEvent(event);
}

void SourceSelectorWindow::hideEvent(QHideEvent *event) {
	QWidget::hideEvent(event);
	obs_helpers::disconnectSourceEditSignals(m_sourceModificationSignalKey);
}

void SourceSelectorWindow::onAddButtonClicked() {
	const QList<QListWidgetItem *> selected = m_listWidget->selectedItems();
	QList<QString> names(selected.size());

	for (qsizetype i = 0; i < selected.size(); ++i) {
		names[i] = selected[i]->text();
	}

	QEvent *event = new SourceAddedEvent(names);
	EventManager::get()->sendFrontendEvent(event);

	hide();
}

void SourceSelectorWindow::onCancelButtonClicked() {
	hide();
}
