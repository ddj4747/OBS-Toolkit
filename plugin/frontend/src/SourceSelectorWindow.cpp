#include <SourceSelectorWindow.h>
#include <plugin-support.h>

SourceSelectorWindow::SourceSelectorWindow(QWidget *parent)
	: QWidget(parent, Qt::Window),
	  m_layout(new QVBoxLayout()),
	  m_buttonLayout(new QHBoxLayout()),
	  m_listWidget(new QListWidget(this)),
	  m_addButton(new QPushButton("Add")),
	  m_cancelButton(new QPushButton("Cancel")) {

	setWindowTitle(QString(PLUGIN_NAME) + " - Source Selector");
	m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_listWidget->setIconSize(QSize(18, 18));

	m_layout->addWidget(m_listWidget);
	m_layout->addLayout(m_buttonLayout);

	m_buttonLayout->addWidget(m_addButton);
	m_buttonLayout->addWidget(m_cancelButton);

	setLayout(m_layout);
}

SourceSelectorWindow::~SourceSelectorWindow() {
	if (!isHidden()) {
		signal_handler_t *handler = obs_get_signal_handler();
		if (handler) {
			signal_handler_disconnect(handler, "source_create", onSourceListChange, this);
			signal_handler_disconnect(handler, "source_destroy", onSourceListChange, this);
		}
	}
}

void SourceSelectorWindow::refreshSourceList(const QList<QString> &excludedSources) {
	m_listWidget->clear();

	// ReSharper disable once CppLocalVariableMayBeConst
	uint64_t ptrTable[2]{reinterpret_cast<uint64_t>(this), reinterpret_cast<uint64_t>(&excludedSources)};

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

QIcon SourceSelectorWindow::getIconForSource(const obs_source_t *source) {
	const char *id = obs_source_get_id(source);
	const obs_icon_type type = obs_source_get_icon_type(id);
	const QWidget *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());

	if (!mainWindow) {
		static bool logged = false;
		if (!logged) {
			obs_log(LOG_WARNING, "main window unavailable, source icons will not be shown");
			logged = true;
		}
		return {};
	}

	switch (type) {
	case OBS_ICON_TYPE_IMAGE:
		return mainWindow->property("imageIcon").value<QIcon>();
	case OBS_ICON_TYPE_COLOR:
		return mainWindow->property("colorIcon").value<QIcon>();
	case OBS_ICON_TYPE_SLIDESHOW:
		return mainWindow->property("slideshowIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_INPUT:
		return mainWindow->property("audioInputIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return mainWindow->property("audioOutputIcon").value<QIcon>();
	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		return mainWindow->property("audioProcessOutputIcon").value<QIcon>();
	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return mainWindow->property("desktopCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return mainWindow->property("windowCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_GAME_CAPTURE:
		return mainWindow->property("gameCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_CAMERA:
		return mainWindow->property("cameraIcon").value<QIcon>();
	case OBS_ICON_TYPE_TEXT:
		return mainWindow->property("textIcon").value<QIcon>();
	case OBS_ICON_TYPE_MEDIA:
		return mainWindow->property("mediaIcon").value<QIcon>();
	case OBS_ICON_TYPE_BROWSER:
		return mainWindow->property("browserIcon").value<QIcon>();
	case OBS_ICON_TYPE_UNKNOWN:
	default:
		return mainWindow->property("defaultIcon").value<QIcon>();
	}
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

	const QIcon icon = getIconForSource(source);
	new QListWidgetItem(icon, name, m_listWidget);
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void SourceSelectorWindow::onSourceListChange(void *data, calldata_t *cd) {
	SourceSelectorWindow *self = static_cast<SourceSelectorWindow *>(data);
	const obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));

	if (!self || !source) {
		obs_log(LOG_WARNING, "source list change ignored: invalid callback data");
		return;
	}

	if (obs_source_removed(source)) {
		obs_log(LOG_INFO, "source removed: '%s' (%s), refreshing list", obs_source_get_name(source),
			obs_source_get_id(source));
	} else {
		obs_log(LOG_INFO, "source created: '%s' (%s), refreshing list", obs_source_get_name(source),
			obs_source_get_id(source));
	}

	self->refreshSourceList();
}

void SourceSelectorWindow::showEvent(QShowEvent *event) {
	refreshSourceList();
	obs_log(LOG_INFO, "source selector opened with %d sources", m_listWidget->count());

	signal_handler_t *handler = obs_get_signal_handler();
	if (handler) {
		signal_handler_connect(handler, "source_create", onSourceListChange, this);
		signal_handler_connect(handler, "source_destroy", onSourceListChange, this);
	}

	QWidget::showEvent(event);
}

void SourceSelectorWindow::hideEvent(QHideEvent *event) {
	QWidget::hideEvent(event);

	signal_handler_t *handler = obs_get_signal_handler();
	if (handler) {
		signal_handler_disconnect(handler, "source_create", onSourceListChange, this);
		signal_handler_disconnect(handler, "source_destroy", onSourceListChange, this);
	}
}