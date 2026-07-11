#pragma once

#include <MultiValueSlider.h>
#include <QEnterEvent>
#include <QEvent>
#include <qboxlayout.h>

class SourcePartitionSelector : public QWidget {
	Q_OBJECT
public:
	explicit SourcePartitionSelector(QWidget *parent = nullptr, int minValue = 0, int maxValue = 10000);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

private slots:
	void updatePartitions();
	void onSliderTrackPressed(int value);
	void onSliderMouseEnter(QEnterEvent *event);
	void onSliderMouseLeave(QEvent *event);

private:
	QHBoxLayout *m_layout;
	MultiValueSlider *m_slider;
};
