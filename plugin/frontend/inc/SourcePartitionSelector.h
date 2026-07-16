#pragma once

#include <MultiValueSlider.h>
#include <QEnterEvent>
#include <QEvent>
#include <QMenu>
#include <qboxlayout.h>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class SourcePartitionSelector : public QWidget {
	Q_OBJECT
public:
	explicit SourcePartitionSelector(QWidget *parent = nullptr, int minValue = 0, int maxValue = 10000);

	void setHandleValue(int index, int value) const;
	NO_DISCARD QList<int> getHandleValues() const;

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

signals:
	void onValuesChanged() const;

private slots:
	void updatePartitions();
	void onSliderTrackPressed(int value);
	void onSliderMouseEnter(QEnterEvent *event) const;
	void onSliderMouseLeave(QEvent *event) const;
	void onHoveredValueChanged(int value) const;
	void onHandleRightClicked(const QMouseEvent *event, int index);
	void removeSelectedHandle() const;

private:
	QHBoxLayout *m_layout;
	QMenu *m_menu;
	QAction *m_removeValueAction{nullptr};
	MultiValueSlider *m_slider;
	int m_handleIndex = -1;
};
