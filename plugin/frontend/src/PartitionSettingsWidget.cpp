#include <PartitionSettingsWidget.h>
#include <plugin-support.h>
#include <SourcePartitionSelector.h>
#include <obs-frontend-api.h>
#include <obs.h>
#include <QPainter>
#include <QSignalBlocker>

PartitionSettingsWidget::PartitionSettingsWidget(QWidget *parent, SourcePartitionSelector *selector, const int index,
						 const int min, const int max)
	: QGroupBox(QString("Partition %1").arg(index + 1), parent),
	  m_selector(selector),
	  m_minValueLabel(new QLabel("From:", this)),
	  m_maxValueLabel(new QLabel("To:", this)),
	  m_sceneBoxLabel(new QLabel("Scene:", this)),
	  m_minValueLayout(new QHBoxLayout()),
	  m_maxValueLayout(new QHBoxLayout()),
	  m_sceneBoxLayout(new QHBoxLayout()),
	  m_mainLayout(new QVBoxLayout(this)),
	  m_minValue(new QSpinBox(this)),
	  m_maxValue(new QSpinBox(this)),
	  m_sceneBox(new QComboBox(this)),
	  m_index(index) {
	setFlat(false);

	const int labelWidth = std::max({m_minValueLabel->sizeHint().width(), m_maxValueLabel->sizeHint().width(),
					 m_sceneBoxLabel->sizeHint().width()});
	m_minValueLabel->setFixedWidth(labelWidth);
	m_maxValueLabel->setFixedWidth(labelWidth);
	m_sceneBoxLabel->setFixedWidth(labelWidth);

	m_minValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_maxValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	m_sceneBoxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

	m_minValue->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_maxValue->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_sceneBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	m_minValueLayout->addWidget(m_minValueLabel);
	m_minValueLayout->addWidget(m_minValue, 1);

	m_maxValueLayout->addWidget(m_maxValueLabel);
	m_maxValueLayout->addWidget(m_maxValue, 1);

	m_sceneBoxLayout->addWidget(m_sceneBoxLabel);
	m_sceneBoxLayout->addWidget(m_sceneBox, 1);

	m_mainLayout->addLayout(m_minValueLayout);
	m_mainLayout->addLayout(m_maxValueLayout);
	m_mainLayout->addLayout(m_sceneBoxLayout);

	m_mainLayout->setContentsMargins(8, 8, 8, 8);
	m_mainLayout->setSpacing(6);

	m_minValue->setValue(min);
	m_maxValue->setValue(max);

	m_minValue->setMinimum(min);
	m_maxValue->setMinimum(max);

	connect(m_minValue, &QSpinBox::valueChanged, this, &PartitionSettingsWidget::onValueChanged);
	connect(m_maxValue, &QSpinBox::valueChanged, this, &PartitionSettingsWidget::onValueChanged);

	refreshSceneBox();

	if (index == 0) {
		m_minValue->setDisabled(true);
	}

	if (index == m_selector->getHandleValues().size()) {
		m_maxValue->setDisabled(true);
	}

	obs_frontend_add_event_callback(onFrontendEvent, this);
}

PartitionSettingsWidget::~PartitionSettingsWidget() {
	obs_frontend_remove_event_callback(onFrontendEvent, this);
}

void PartitionSettingsWidget::onFrontendEvent(const obs_frontend_event event, void *data) {
	if (event != OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED && event != OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		return;
	}

	const auto *widget = static_cast<PartitionSettingsWidget *>(data);
	widget->refreshSceneBox();
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

void PartitionSettingsWidget::refreshSceneBox() const {
	const QString currentScene = m_sceneBox->currentText();

	obs_frontend_source_list sceneList = {};
	obs_frontend_get_scenes(&sceneList);

	const QSignalBlocker blocker(m_sceneBox);
	m_sceneBox->clear();

	for (size_t i = 0; i < sceneList.sources.num; i++) {
		const obs_source_t *source = sceneList.sources.array[i];
		if (source) {
			const char *name = obs_source_get_name(source);
			m_sceneBox->addItem(name);
		}
	}

	obs_frontend_source_list_free(&sceneList);

	const int currentIndex = m_sceneBox->findText(currentScene);
	if (currentIndex >= 0) {
		m_sceneBox->setCurrentIndex(currentIndex);
	}
}
