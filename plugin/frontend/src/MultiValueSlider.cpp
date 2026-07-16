#include <MultiValueSlider.h>

#include <QMessageBox>
#include <QStyle>
#include <QStylePainter>
#include <qevent.h>

MultiValueSlider::MultiValueSlider(QWidget *parent) : QSlider(Qt::Horizontal, parent) {
	setRange(0, 100);
	setMouseTracking(true);
}

MultiValueSlider::MultiValueSlider(QWidget *parent, const Qt::Orientation orientation, const int minValue,
				   const int maxValue)
	: QSlider(orientation, parent) {
	setRange(minValue, maxValue);
	setMouseTracking(true);
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

void MultiValueSlider::setValue(const int index, int value) {
	if (index < 0 || index >= m_values.count()) {
		return;
	}

	if (index > 0 && m_values.at(index - 1) > value) {
		value = m_values.at(index - 1);
	}

	if (index < m_values.count() - 1 && m_values.at(index + 1) < value) {
		value = m_values.at(index + 1);
	}

	if (m_values.at(index) == value) {
		return;
	}

	m_values[index] = value;
	update();
	emit valuesChanged();
}

void MultiValueSlider::paintEvent(QPaintEvent *) {
	QStylePainter painter(this);

	initStyleOption(&m_sliderDesign);
	m_sliderDesign.subControls = QStyle::SC_SliderGroove;
	painter.drawComplexControl(QStyle::CC_Slider, m_sliderDesign);

	m_sliderDesign.subControls = QStyle::SC_SliderHandle;
	for (int i = 0; i < m_values.count(); i++) {
		const int value = m_values.at(i);

		m_sliderDesign.sliderPosition = value;
		m_sliderDesign.sliderValue = value;
		m_sliderDesign.state &= ~(QStyle::State_MouseOver | QStyle::State_Sunken);
		m_sliderDesign.activeSubControls = QStyle::SC_None;

		if (i == m_hoveredValueIndex) {
			m_sliderDesign.state |= QStyle::State_MouseOver;
			m_sliderDesign.activeSubControls = QStyle::SC_SliderHandle;
		}
		if (i == m_activeValueIndex && m_isMousePressed) {
			m_sliderDesign.state |= QStyle::State_Sunken;
			m_sliderDesign.activeSubControls = QStyle::SC_SliderHandle;
		}

		painter.drawComplexControl(QStyle::CC_Slider, m_sliderDesign);
	}
}

void MultiValueSlider::mousePressEvent(QMouseEvent *event) {
	const QPointF clickPos = event->position();
	const int clickedIndex = handleAt(clickPos);

	if (event->button() == Qt::RightButton) {
		if (clickedIndex < 0) {
			event->ignore();
			return;
		}

		emit onHandleRightClicked(event, clickedIndex);
		event->accept();
		return;
	}

	if (event->button() != Qt::LeftButton) {
		event->ignore();
		return;
	}

	m_activeValueIndex = clickedIndex;

	if (m_activeValueIndex < 0) {
		const int pixel = orientation() == Qt::Horizontal ? static_cast<int>(clickPos.x())
								  : static_cast<int>(clickPos.y());
		const int value = valueFromPixel(pixel);

		emit trackPressed(value);
		event->ignore();
		return;
	}

	emit onHandlerPressed(m_activeValueIndex);
	m_isMousePressed = true;
	update();
	event->accept();
}

void MultiValueSlider::mouseReleaseEvent(QMouseEvent *event) {
	m_isMousePressed = false;
	m_activeValueIndex = -1;
	updateHoveredHandle(event->position());
	event->accept();
}

void MultiValueSlider::mouseMoveEvent(QMouseEvent *event) {
	const QPointF mousePosition = event->position();

	if (!m_isMousePressed || m_activeValueIndex < 0 || m_activeValueIndex >= m_values.count()) {
		updateHoveredHandle(mousePosition);
		event->accept();
		return;
	}
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
	emit valuesChanged();
	event->accept();
}

void MultiValueSlider::enterEvent(QEnterEvent *event) {
	QSlider::enterEvent(event);
	emit onMouseEnter(event);
}

void MultiValueSlider::leaveEvent(QEvent *event) {
	if (m_hoveredValueIndex >= 0) {
		m_hoveredValueIndex = -1;
		emit onHoveredValueChanged(m_hoveredValueIndex);
		update();
	}

	QSlider::leaveEvent(event);
	emit onMouseLeave(event);
}

int MultiValueSlider::handleAt(const QPointF &pos) const {
	QStyleOptionSlider opt;
	initStyleOption(&opt);
	opt.subControls = QStyle::SC_SliderHandle;

	for (int i = 0; i < m_values.count(); i++) {
		opt.sliderPosition = m_values.at(i);
		opt.sliderValue = m_values.at(i);

		const QRect rect = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
		if (rect.contains(pos.toPoint())) {
			return i;
		}
	}

	return -1;
}

void MultiValueSlider::updateHoveredHandle(const QPointF &pos) {
	const int hovered = handleAt(pos);
	if (hovered == m_hoveredValueIndex) {
		return;
	}

	m_hoveredValueIndex = hovered;
	emit onHoveredValueChanged(m_hoveredValueIndex);
	update();
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

int MultiValueSlider::pixelFromValue(const int value) const {
	QStyleOptionSlider opt;
	initStyleOption(&opt);

	const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
	const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

	if (orientation() == Qt::Horizontal) {
		const int handleLength = handle.width();
		const int span = groove.width() - handleLength;
		const int pos = QStyle::sliderPositionFromValue(minimum(), maximum(), value, span, opt.upsideDown);
		return groove.x() + handleLength / 2 + pos;
	}

	const int handleLength = handle.height();
	const int span = groove.height() - handleLength;
	const int pos = QStyle::sliderPositionFromValue(minimum(), maximum(), value, span, opt.upsideDown);
	return groove.y() + handleLength / 2 + pos;
}

void MultiValueSlider::removeValue(const int index) {
	if (index < 0 || m_values.count() <= index) {
		return;
	}

	m_values.removeAt(index);
	update();
	emit valuesChanged();
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
