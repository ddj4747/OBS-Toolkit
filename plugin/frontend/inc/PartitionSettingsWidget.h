#pragma once

#include <QGroupBox>
#include <QSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

class SourcePartitionSelector;

class PartitionSettingsWidget final : public QGroupBox {
	Q_OBJECT
public:
	PartitionSettingsWidget() = delete;
	explicit PartitionSettingsWidget(QWidget *parent, SourcePartitionSelector *selector, int index = -1,
					 int min = 0, int max = INT_MAX);
	void updateValue(int max) const;
	void setHighlighted(bool highlighted);

signals:
	void onPartitionFocused(int index) const;

protected:
	void enterEvent(QEnterEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	void onValueChanged() const;

	SourcePartitionSelector *m_selector;
	QLabel *m_minValueLabel;
	QLabel *m_maxValueLabel;

	QHBoxLayout *m_minValueLayout;
	QHBoxLayout *m_maxValueLayout;
	QVBoxLayout *m_mainLayout;

	QSpinBox *m_minValue;
	QSpinBox *m_maxValue;

	int m_index;
	bool m_highlighted = false;
};
