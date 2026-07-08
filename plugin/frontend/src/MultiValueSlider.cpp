#include <MultiValueSlider.h>
#include <QStylePainter>
#include <QToolTip>
#include <qevent.h>

MultiValueSlider::MultiValueSlider(QWidget *parent) : QSlider(Qt::Horizontal, parent) {
	setRange(0, 100);
}

MultiValueSlider::MultiValueSlider(QWidget *parent, const Qt::Orientation orientation, const int minValue,
				   const int maxValue)
	: QSlider(orientation, parent) {
	setRange(minValue, maxValue);
}

void MultiValueSlider::addValue(const int value) {
	for (qsizetype i = 0; i < m_values.count(); i++) {
		if (m_values.at(i) >= value) {
			m_values.insert(i, value);
			return;
		}
	}

	m_values.append(value);
}

void MultiValueSlider::paintEvent(QPaintEvent *) {
	QStylePainter painter(this);

	initStyleOption(&m_sliderDesign);
	m_sliderDesign.subControls = QStyle::SC_SliderGroove;
	painter.drawComplexControl(QStyle::CC_Slider, m_sliderDesign);

	m_sliderDesign.subControls = QStyle::SC_SliderHandle;
	for (const auto value : m_values) {
		m_sliderDesign.sliderPosition = value;
		m_sliderDesign.sliderValue = value;
		painter.drawComplexControl(QStyle::CC_Slider, m_sliderDesign);
	}
}

void MultiValueSlider::mousePressEvent(QMouseEvent *event) {
	QStyleOptionSlider opt;
	initStyleOption(&opt);
	opt.subControls = QStyle::SC_SliderHandle;

	const QPointF clickPos = event->position();

	m_activeValueIndex = -1;
	for (int i = 0; i < m_values.count(); i++) {
		opt.sliderPosition = m_values.at(i);
		opt.sliderValue = m_values.at(i);

		const QRect rect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

		if (rect.contains(clickPos.toPoint())) {
			m_activeValueIndex = i;
			break;
		}
	}

	if (m_activeValueIndex < 0) {
		event->ignore();
		return;
	}

	m_isMousePressed = true;
	event->accept();
}

void MultiValueSlider::mouseReleaseEvent(QMouseEvent *event) {
	m_isMousePressed = false;
	event->accept();
}

void MultiValueSlider::mouseMoveEvent(QMouseEvent *event) {
	if (!m_isMousePressed || m_activeValueIndex < 0 || m_activeValueIndex >= m_values.count()) {
		return;
	}

	const QPointF mousePosition = event->position();
	const int pixel = orientation() == Qt::Horizontal ? static_cast<int>(mousePosition.x())
							  : static_cast<int>(mousePosition.y());

	int newValue = valueFromPixel(pixel);

	if (m_activeValueIndex > 0) {
		const int lastValue = m_values.at(m_activeValueIndex - 1);
		if (newValue < lastValue) {
			newValue = lastValue;
		}
	}

	if (m_activeValueIndex < m_values.count() - 1) {
		const int nextValue = m_values.at(m_activeValueIndex + 1);
		if (newValue > nextValue) {
			newValue = nextValue;
		}
	}

	m_values[m_activeValueIndex] = newValue;

	update();
	event->accept();
}

int MultiValueSlider::valueFromPixel(const int pixelPos) const {
	QStyleOptionSlider opt;
	initStyleOption(&opt);

	const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
	const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

	int spanMin;
	int span;
	int pos;

	if (orientation() == Qt::Horizontal) {
		const int handleLength = handle.width();
		spanMin = groove.x() + handleLength / 2;
		span = groove.width() - handleLength;
		pos = pixelPos - spanMin;
	} else {
		const int handleLength = handle.height();
		spanMin = groove.y() + handleLength / 2;
		span = groove.height() - handleLength;
		pos = pixelPos - spanMin;
	}

	return QStyle::sliderValueFromPosition(minimum(), maximum(), pos, span, opt.upsideDown);
}

void MultiValueSlider::removeValue(const int index) {
	if (m_values.count() <= index) {
		return;
	}

	m_values.removeAt(index);
}

void MultiValueSlider::setMaxValue(const int maxValue) {
	setMaximum(maxValue);
}

int MultiValueSlider::getMaxValue() const {
	return maximum();
}

void MultiValueSlider::setMinValue(const int minValue) {
	setMinimum(minValue);
}

int MultiValueSlider::getMinValue() const {
	return minimum();
}

QList<int> MultiValueSlider::values() const {
	return m_values;
}
