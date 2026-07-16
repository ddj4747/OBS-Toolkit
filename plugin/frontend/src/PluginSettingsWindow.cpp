#include <PluginSettingsWindow.h>
#include <plugin-support.h>
#include <QFrame>

PluginSettingsWindow::PluginSettingsWindow(QWidget *parent)
	: QWidget(parent, Qt::Window),
	  m_sourcePartitionSelector(new SourcePartitionSelector(this, 0, 10000)),
	  m_partitionScrollContent(new QWidget(this)),
	  m_partitionSettingsLayout(new QVBoxLayout(m_partitionScrollContent)),
	  m_partitionScrollArea(new QScrollArea(this)),
	  m_mainLayout(new QVBoxLayout(this)) {

	setWindowTitle(QStringLiteral("%1 Settings").arg(PLUGIN_NAME));

	m_mainLayout->setContentsMargins(12, 12, 12, 12);
	m_mainLayout->setSpacing(12);
	m_mainLayout->setAlignment(Qt::AlignTop);

	m_partitionSettingsLayout->setContentsMargins(0, 0, 0, 0);
	m_partitionSettingsLayout->setSpacing(12);
	m_partitionSettingsLayout->setAlignment(Qt::AlignTop);

	m_partitionScrollArea->setWidget(m_partitionScrollContent);
	m_partitionScrollArea->setWidgetResizable(true);
	m_partitionScrollArea->setFrameShape(QFrame::NoFrame);
	m_partitionScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	m_mainLayout->addWidget(m_sourcePartitionSelector);
	m_mainLayout->addWidget(m_partitionScrollArea, 1);

	setMinimumWidth(700);
	setMinimumHeight(600);

	connect(m_sourcePartitionSelector, &SourcePartitionSelector::onValuesChanged, this,
		&PluginSettingsWindow::onPartitionSelectorValueChanged);

	onPartitionSelectorValueChanged();

	m_sourcePartitionSelector->setMinimumWidth(500);
	setLayout(m_mainLayout);
}

PluginSettingsWindow::~PluginSettingsWindow() = default;

void PluginSettingsWindow::onPartitionSelectorValueChanged() {
	const auto values = m_sourcePartitionSelector->getHandleValues();
	const qsizetype targetCount = values.count() + 1;

	while (m_partitionSettings.count() > targetCount) {
		PartitionSettingsWidget *widget = m_partitionSettings.back();
		m_partitionSettingsLayout->removeWidget(widget);
		m_partitionSettings.pop_back();
		widget->deleteLater();
	}

	while (m_partitionSettings.count() < targetCount) {
		const int index = static_cast<int>(m_partitionSettings.count());
		const int min = index == 0 ? 0 : values.at(index - 1);
		const int max = index == values.count() ? 10000 : values.at(index);

		m_partitionSettings.append(new PartitionSettingsWidget(m_partitionScrollContent,
								       m_sourcePartitionSelector, index, min, max));
		m_partitionSettingsLayout->addWidget(m_partitionSettings.back());
	}

	for (const auto &partition : m_partitionSettings) {
		partition->updateValue(10000);
	}
}
