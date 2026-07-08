#include <PluginSettingsWindow.h>

PluginSettingsWindow::PluginSettingsWindow(QWidget *parent)
	: QWidget(parent, Qt::Window),
	  m_sourcePartitionSelector(new MultiValueSlider(this, Qt::Horizontal, 0, 10000)) {
	auto *layout = new QVBoxLayout(this);
	layout->addWidget(m_sourcePartitionSelector);
	setLayout(layout);

	m_sourcePartitionSelector->setMinimumHeight(50);
	m_sourcePartitionSelector->setMinimumWidth(500);
	m_sourcePartitionSelector->addValue(1000);
	m_sourcePartitionSelector->addValue(8700);
	m_sourcePartitionSelector->addValue(5000);
}

PluginSettingsWindow::~PluginSettingsWindow() {}
