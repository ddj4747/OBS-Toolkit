#include <PartitionSettingsWidget.h>
#include <SourcePartitionSelector.h>

#include <QPainter>
#include <QSignalBlocker>

PartitionSettingsWidget::PartitionSettingsWidget(QWidget *parent, SourcePartitionSelector *selector, const int index,
						 const int min, const int max)
	: QGroupBox(QString("Partition %1").arg(index + 1), parent),
	  m_selector(selector),
	  m_minValueLabel(new QLabel("From:", this)),
	  m_maxValueLabel(new QLabel("To:", this)),
	  m_minValueLayout(new QHBoxLayout()),
	  m_maxValueLayout(new QHBoxLayout()),
	  m_mainLayout(new QVBoxLayout(this)),
	  m_minValue(new QSpinBox(this)),
	  m_maxValue(new QSpinBox(this)),
	  m_index(index) {

	setFlat(false);

	const int labelWidth = std::max(m_minValueLabel->sizeHint().width(), m_maxValueLabel->sizeHint().width());
	m_minValueLabel->setFixedWidth(labelWidth);
	m_maxValueLabel->setFixedWidth(labelWidth);
	m_minValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_maxValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

	m_minValue->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_maxValue->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	m_minValueLayout->addWidget(m_minValueLabel);
	m_minValueLayout->addWidget(m_minValue, 1);

	m_maxValueLayout->addWidget(m_maxValueLabel);
	m_maxValueLayout->addWidget(m_maxValue, 1);

	m_mainLayout->addLayout(m_minValueLayout);
	m_mainLayout->addLayout(m_maxValueLayout);
	m_mainLayout->setContentsMargins(8, 8, 8, 8);
	m_mainLayout->setSpacing(6);

	m_minValue->setValue(min);
	m_maxValue->setValue(max);

	m_minValue->setMinimum(min);
	m_maxValue->setMinimum(max);

	connect(m_minValue, &QSpinBox::valueChanged, this, &PartitionSettingsWidget::onValueChanged);
	connect(m_maxValue, &QSpinBox::valueChanged, this, &PartitionSettingsWidget::onValueChanged);

	if (index == 0) {
		m_minValue->setDisabled(true);
	}

	if (index == m_selector->getHandleValues().size()) {
		m_maxValue->setDisabled(true);
	}
}

void PartitionSettingsWidget::updateValue(const int max) const {
	const QList<int> values = m_selector->getHandleValues();
	const qsizetype count = values.size();

	const int from = (m_index == 0) ? 0 : values.at(m_index - 1);
	const int to = (m_index == count) ? max : values.at(m_index);

	const int fromLower = (m_index >= 2) ? values.at(m_index - 2) : 0;
	const int fromUpper = to;

	const int toLower = from;
	const int toUpper = (m_index + 1 < count) ? values.at(m_index + 1) : max;

	const QSignalBlocker minBlocker(m_minValue);
	const QSignalBlocker maxBlocker(m_maxValue);

	m_minValue->setRange(fromLower, fromUpper);
	m_maxValue->setRange(toLower, toUpper);

	m_minValue->setValue(from);
	m_maxValue->setValue(to);

	m_minValue->setDisabled(m_index == 0);
	m_maxValue->setDisabled(m_index == count);
}

void PartitionSettingsWidget::setHighlighted(const bool highlighted) {
	if (m_highlighted == highlighted) {
		return;
	}

	m_highlighted = highlighted;
	update();
}

void PartitionSettingsWidget::enterEvent(QEnterEvent *event) {
	QGroupBox::enterEvent(event);
	emit onPartitionFocused(m_index);
}

void PartitionSettingsWidget::paintEvent(QPaintEvent *event) {
	QGroupBox::paintEvent(event);

	if (!m_highlighted) {
		return;
	}

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);
	painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
	painter.setBrush(Qt::NoBrush);
	painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
}

void PartitionSettingsWidget::onValueChanged() const {
	m_selector->setHandleValue(m_index - 1, m_minValue->value());
	m_selector->setHandleValue(m_index, m_maxValue->value());
}
