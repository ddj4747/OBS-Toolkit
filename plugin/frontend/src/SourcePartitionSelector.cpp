#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QPalette>
#include <QColor>
#include <SourcePartitionSelector.h>
#include <QPainter>
#include <QPixmap>
#include <QApplication>
#include <qevent.h>
#include <EventManager.h>
#include <FrontendEvents.h>

namespace {
QBrush makeDiagHatchBrush(const QColor &color, const int lineWidth = 2, const int spacing = 8) {
	const int size = spacing;
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);

	QPainter p(&pixmap);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setPen(QPen(color, lineWidth, Qt::SolidLine, Qt::FlatCap));

	p.drawLine(0, size, size, 0);
	p.drawLine(-size, size, size, -size);
	p.drawLine(0, size * 2, size * 2, 0);
	p.end();

	return QBrush(pixmap);
}
} // namespace

SourcePartitionSelector::SourcePartitionSelector(QWidget *parent, const int minValue, const int maxValue)
	: QWidget(parent),
	  m_layout(new QHBoxLayout(this)),
	  m_menu(new QMenu(this)),
	  m_slider(new MultiValueSlider(this, Qt::Horizontal, minValue, maxValue)) {

	m_layout->setContentsMargins(0, 0, 0, 0);
	m_layout->setSpacing(0);
	setMinimumHeight(80);

	m_removeValueAction = m_menu->addAction("Remove");
	connect(m_removeValueAction, &QAction::triggered, this, &SourcePartitionSelector::removeSelectedHandle);

	m_layout->addWidget(m_slider, 1, Qt::AlignBottom);

	connect(m_slider, &MultiValueSlider::valuesChanged, this, &SourcePartitionSelector::updatePartitions);
	connect(m_slider, &MultiValueSlider::trackPressed, this, &SourcePartitionSelector::onSliderTrackPressed);
	connect(m_slider, &MultiValueSlider::onMouseEnter, this, &SourcePartitionSelector::onSliderMouseEnter);
	connect(m_slider, &MultiValueSlider::onHoveredValueChanged, this,
		&SourcePartitionSelector::onHoveredValueChanged);
	connect(m_slider, &MultiValueSlider::onHandleRightClicked, this,
		&SourcePartitionSelector::onHandleRightClicked);
	connect(m_slider, &MultiValueSlider::onMouseLeave, this,
		&SourcePartitionSelector::onSliderMouseLeave); // Ignore this clangd error, its bullshit
}

void SourcePartitionSelector::highlightPartition(const int partition) {
	if (m_highlightedPartition == partition) {
		return;
	}

	m_highlightedPartition = partition;
	update();
}

void SourcePartitionSelector::setHandleValue(const int index, const int value) const {
	m_slider->setValue(index, value);
}

QList<int> SourcePartitionSelector::getHandleValues() const {
	return m_slider->values();
}

void SourcePartitionSelector::paintEvent(QPaintEvent *) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	void *main_window_void = obs_frontend_get_main_window();
	if (!main_window_void) {
		return;
	}

	const QMainWindow *mainWindow = static_cast<QMainWindow *>(main_window_void);
	const QPalette palette = mainWindow->palette();

	const QColor highlight = palette.color(QPalette::Highlight);
	const QColor muted = palette.color(QPalette::WindowText);

	QColor hatchEven = highlight;
	hatchEven.setAlpha(90);
	QColor hatchOdd = muted;
	hatchOdd.setAlpha(70);

	QColor hatchSelected = highlight;
	hatchSelected.setAlpha(160);

	QColor washSelected = highlight;
	washSelected.setAlpha(45);

	QColor dimOverlay = palette.color(QPalette::Window);
	dimOverlay.setAlpha(110);

	QPen pen;
	pen.setWidth(3);
	pen.setStyle(Qt::SolidLine);

	constexpr int kHatchLineWidth = 2;
	constexpr int kHatchSpacing = 8;
	const QBrush hatchBrushEven = makeDiagHatchBrush(hatchEven, kHatchLineWidth, kHatchSpacing);
	const QBrush hatchBrushOdd = makeDiagHatchBrush(hatchOdd, kHatchLineWidth, kHatchSpacing);
	const QBrush hatchBrushSelected = makeDiagHatchBrush(hatchSelected, kHatchLineWidth, kHatchSpacing);

	const int partitionHeight = m_slider->geometry().center().y();
	const auto values = m_slider->values();
	const int partitionCount = static_cast<int>(values.count()) + 1;
	const bool hasSelection = m_highlightedPartition >= 0 && m_highlightedPartition < partitionCount;

	int selectedX = 0;
	int selectedW = 0;
	int prevX = 0;

	for (int i = 0; i < partitionCount; ++i) {
		const int x = (i < values.count())
				      ? m_slider->mapTo(this, QPoint(m_slider->pixelFromValue(values.at(i)), 0)).x()
				      : width();
		const int w = x - prevX;
		const bool selected = i == m_highlightedPartition;

		if (selected) {
			pen.setColor(highlight);
			painter.setBrush(hatchBrushSelected);
			selectedX = prevX;
			selectedW = w;
		} else if (i % 2 == 0) {
			pen.setColor(highlight);
			painter.setBrush(hatchBrushEven);
		} else {
			pen.setColor(muted);
			painter.setBrush(hatchBrushOdd);
		}

		painter.setPen(pen);
		painter.drawRect(prevX, 0, w, partitionHeight);

		if (hasSelection && !selected) {
			painter.fillRect(prevX, 0, w, partitionHeight, dimOverlay);
		}

		prevX = x;
	}

	if (!hasSelection) {
		return;
	}

	painter.fillRect(selectedX, 0, selectedW, partitionHeight, washSelected);

	pen.setColor(highlight);
	pen.setWidth(2);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);
	painter.drawRect(QRect(selectedX, 0, selectedW, partitionHeight).adjusted(1, 1, -1, -1));

	painter.fillRect(selectedX, partitionHeight - 3, selectedW, 3, highlight);
}

void SourcePartitionSelector::mousePressEvent(QMouseEvent *event) {
	const QPointF clickPosition = event->position();
	if (clickPosition.y() >= m_slider->pos().y()) {
		event->ignore();
		return;
	}

	const int value = m_slider->valueFromPixel(static_cast<int>(clickPosition.x()));
	const auto &values = m_slider->values();

	int index = 0;
	while (index < values.count() && value >= values.at(index)) {
		++index;
	}

	emit onPartitionClicked(index);
	event->accept();
}

void SourcePartitionSelector::onSliderTrackPressed(const int value) {
	m_slider->addValue(value);
	emit onValuesChanged();
	updatePartitions();
}

void SourcePartitionSelector::updatePartitions() {
	emit onValuesChanged();
	update();
}

void SourcePartitionSelector::onSliderMouseEnter(QEnterEvent *) const {
	QApplication::setOverrideCursor(QCursor(Qt::DragCopyCursor));
}

void SourcePartitionSelector::onSliderMouseLeave(QEvent *) const {
	QApplication::restoreOverrideCursor();
}

void SourcePartitionSelector::onHoveredValueChanged(const int value) const {
	if (value == -1) {
		QApplication::setOverrideCursor(QCursor(Qt::DragCopyCursor));
	} else {
		QApplication::restoreOverrideCursor();
	}
}

void SourcePartitionSelector::onHandleRightClicked(const QMouseEvent *event, const int index) {
	m_handleIndex = index;
	m_menu->popup(m_slider->mapToGlobal(event->pos()));
}

void SourcePartitionSelector::removeSelectedHandle() const {
	m_slider->removeValue(m_handleIndex);
	emit onValuesChanged();
}
