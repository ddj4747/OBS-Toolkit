#include <SourcePartitionSelector.h>
#include <QPainter>
#include <QMessageBox>
#include <QApplication>
#include <qevent.h>

SourcePartitionSelector::SourcePartitionSelector(QWidget *parent, const int minValue, const int maxValue)
	: QWidget(parent),
	  m_layout(new QHBoxLayout(this)),
	  m_slider(new MultiValueSlider(this, Qt::Horizontal, minValue, maxValue)) {

	m_layout->setContentsMargins(0, 0, 0, 0);
	m_layout->setSpacing(0);
	setMinimumHeight(80);

	m_layout->addWidget(m_slider, 1, Qt::AlignBottom);
	connect(m_slider, &MultiValueSlider::valuesChanged, this, &SourcePartitionSelector::updatePartitions);
	connect(m_slider, &MultiValueSlider::trackPressed, this, &SourcePartitionSelector::onSliderTrackPressed);
	connect(m_slider, &MultiValueSlider::onMouseEnter, this, &SourcePartitionSelector::onSliderMouseEnter);
	connect(m_slider, &MultiValueSlider::onMouseLeave, this,
		&SourcePartitionSelector::onSliderMouseLeave); // Ignore this clangd error, its bullshit
}

void SourcePartitionSelector::paintEvent(QPaintEvent *) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	QPen pen;
	pen.setColor(Qt::darkCyan);
	pen.setWidth(2);
	pen.setStyle(Qt::SolidLine);
	painter.setPen(pen);

	QBrush brush;
	brush.setColor(Qt::cyan);
	brush.setStyle(Qt::SolidPattern);
	painter.setBrush(brush);

	int prevX = 0;
	const int partitionHeight = m_slider->geometry().center().y();

	const auto values = m_slider->values();
	for (int i = 0; i < values.count(); i++) {
		const int x = m_slider->mapTo(this, QPoint(m_slider->pixelFromValue(values.at(i)), 0)).x();
		painter.drawRect(prevX, 0, x - prevX, partitionHeight);
		prevX = x;
	}

	painter.drawRect(prevX, 0, width() - prevX, partitionHeight);
}

void SourcePartitionSelector::mousePressEvent(QMouseEvent *event) {
	const QPointF clickPosition = event->position();
	if (clickPosition.y() >= m_slider->pos().y()) {
		event->ignore();
		return;
	}

	QMessageBox::warning(this, "Warning", "Click me", QMessageBox::Ok);

	event->accept();
}

void SourcePartitionSelector::onSliderTrackPressed(const int value) {
	m_slider->addValue(value);
	updatePartitions();
}

void SourcePartitionSelector::updatePartitions() {
	update();
}

void SourcePartitionSelector::onSliderMouseEnter(QEnterEvent *) {
	QApplication::setOverrideCursor(QCursor(Qt::DragCopyCursor));
}

void SourcePartitionSelector::onSliderMouseLeave(QEvent *) {
	QApplication::restoreOverrideCursor();
}
