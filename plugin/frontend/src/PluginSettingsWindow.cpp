#include <PluginSettingsWindow.h>

PluginSettingsWindow::PluginSettingsWindow(QWidget *parent)
	: QWidget(parent, Qt::Window),
	  m_sourcePartitionSelector(new SourcePartitionSelector(this, 0, 10000)) {
	auto *layout = new QVBoxLayout(this);

	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(m_sourcePartitionSelector);

	setMinimumWidth(700);
	setMinimumHeight(600);

	m_sourcePartitionSelector->setMinimumWidth(500);

	layout->addWidget(new QPushButton("click me", this));

	setLayout(layout);
}

PluginSettingsWindow::~PluginSettingsWindow() {}
