#include <SourceSelectorWindow.h>
#include <plugin-support.h>

SourceSelectorWindow::SourceSelectorWindow(QWidget *parent)
	: QWidget(parent, Qt::Window),
	  m_layout(std::make_unique<QVBoxLayout>()),
	  m_buttonLayout(std::make_unique<QHBoxLayout>()),
	  m_listWidget(std::make_unique<QListWidget>(this)),
	  m_addButton(std::make_unique<QPushButton>("Add")),
	  m_cancelButton(std::make_unique<QPushButton>("Cancel")) {

	setWindowTitle(QString(PLUGIN_NAME) + " - Source Selector");
	m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

	m_layout->addWidget(m_listWidget.get());
	m_layout->addLayout(m_buttonLayout.get());

	m_buttonLayout->addWidget(m_addButton.get());
	m_buttonLayout->addWidget(m_cancelButton.get());

	setLayout(m_layout.get());
}

void SourceSelectorWindow::refreshSourceList(const QList<QString> &excludedSources) {
	m_listWidget->clear();

	// ReSharper disable once CppLocalVariableMayBeConst
	uint64_t ptrTable[2]{reinterpret_cast<uint64_t>(this), reinterpret_cast<uint64_t>(&excludedSources)};

	obs_enum_sources(
		// ReSharper disable once CppParameterMayBeConstPtrOrRef
		[](void *data, obs_source_t *source) -> bool {
			void **table = static_cast<void **>(data);

			return static_cast<SourceSelectorWindow *>(table[0])->processSourceCallback(
				source, static_cast<const QList<QString> *>(table[1]));
		},
		reinterpret_cast<void *>(ptrTable));

	if (m_listWidget->count() > 0) {
		m_listWidget->setCurrentRow(0);
	}
}

bool SourceSelectorWindow::processSourceCallback(const obs_source_t *source, const QList<QString> *excludedSources) {
	const QString name = obs_source_get_name(source);
	if (excludedSources->contains(name)) {
		return true;
	}

	m_listWidget->addItem(name);
	return true;
}
