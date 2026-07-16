#pragma once
#include <QEnterEvent>
#include <QEvent>
#include <QWidget>
#include <QStyleOptionSlider>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class MultiValueSlider : public QSlider {
	Q_OBJECT
public:
	explicit MultiValueSlider(QWidget *parent = nullptr);
	explicit MultiValueSlider(QWidget *parent, Qt::Orientation orientation, int minValue, int maxValue);

	void addValue(int value);
	void removeValue(int index);

	void setMaxValue(int maxValue);
	NO_DISCARD int getMaxValue() const;

	void setMinValue(int minValue);
	NO_DISCARD int getMinValue() const;

	NO_DISCARD QList<int> values() const;

	NO_DISCARD int valueFromPixel(int pixelPos) const;
	NO_DISCARD int pixelFromValue(int value) const;

signals:
	void valuesChanged();
	void onHandlerPressed(int index);
	void trackPressed(int value);
	void onMouseEnter(QEnterEvent *event);
	void onMouseLeave(QEvent *event);
	void onHoveredValueChanged(int value);
	void onHandleRightClicked(QMouseEvent *event, int index);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;

	NO_DISCARD int handleAt(const QPointF &pos) const;
	void updateHoveredHandle(const QPointF &pos);

	QStyleOptionSlider m_sliderDesign;
	QList<int> m_values;
	int m_activeValueIndex = -1;
	int m_hoveredValueIndex = -1;
	bool m_isMousePressed{false};
};
