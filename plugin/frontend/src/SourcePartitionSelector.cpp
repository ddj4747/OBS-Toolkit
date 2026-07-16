#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QPalette>
#include <QColor>
#include <SourcePartitionSelector.h>
#include <QPainter>
#include <QMessageBox>
#include <QApplication>
#include <qevent.h>

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

void SourcePartitionSelector::paintEvent(QPaintEvent *) {
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	void *main_window_void = obs_frontend_get_main_window();
	if (!main_window_void) {
		return;
	}

	const QMainWindow *mainWindow = static_cast<QMainWindow *>(main_window_void);
	const QPalette palette = mainWindow->palette();

	QColor penColor1 = palette.color(QPalette::Highlight);
	QColor brushColor1 = palette.color(QPalette::Highlight);
	brushColor1.setAlpha(80);

	QColor penColor2 = palette.color(QPalette::WindowText);
	QColor brushColor2 = palette.color(QPalette::ButtonText);
	brushColor2.setAlpha(80);

	QPen pen;
	pen.setWidth(2);
	pen.setStyle(Qt::SolidLine);
	QBrush brush;

	int prevX = 0;
	const int partitionHeight = m_slider->geometry().center().y();

	const auto values = m_slider->values();
	for (int i = 0; i < values.count(); i++) {
		if (i % 2 == 0) {
			pen.setColor(penColor1);
			brush.setColor(brushColor1);
			brush.setStyle(Qt::FDiagPattern);
		} else {
			pen.setColor(penColor2);
			brush.setColor(brushColor2);
			brush.setStyle(Qt::BDiagPattern);
		}

		painter.setPen(pen);
		painter.setBrush(brush);

		const int x = m_slider->mapTo(this, QPoint(m_slider->pixelFromValue(values.at(i)), 0)).x();
		painter.drawRect(prevX, 0, x - prevX, partitionHeight);
		prevX = x;
	}

	if (values.count() % 2 == 0) {
		pen.setColor(penColor1);
		brush.setColor(brushColor1);
		brush.setStyle(Qt::FDiagPattern);
	} else {
		pen.setColor(penColor2);
		brush.setColor(brushColor2);
		brush.setStyle(Qt::BDiagPattern);
	}

	painter.setPen(pen);
	painter.setBrush(brush);
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
}
