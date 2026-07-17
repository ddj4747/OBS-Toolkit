#pragma once

#include <QGroupBox>
#include <QSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <obs-frontend-api.h>

class SourcePartitionSelector;

class PartitionSettingsWidget final : public QGroupBox {
	Q_OBJECT
public:
	PartitionSettingsWidget() = delete;
	explicit PartitionSettingsWidget(QWidget *parent, SourcePartitionSelector *selector, int index = -1,
					 int min = 0, int max = INT_MAX);
	~PartitionSettingsWidget() override;

	void updateValue(int max) const;
	void setHighlighted(bool highlighted);

signals:
	void onPartitionFocused(int index) const;

protected:
	void enterEvent(QEnterEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	static void onFrontendEvent(obs_frontend_event event, void *data);

	void onValueChanged() const;
	void refreshSceneBox() const;

	SourcePartitionSelector *m_selector;
	QLabel *m_minValueLabel;
	QLabel *m_maxValueLabel;
	QLabel *m_sceneBoxLabel;

	QHBoxLayout *m_minValueLayout;
	QHBoxLayout *m_maxValueLayout;
	QHBoxLayout *m_sceneBoxLayout;

	QVBoxLayout *m_mainLayout;

	QSpinBox *m_minValue;
	QSpinBox *m_maxValue;
	QComboBox *m_sceneBox;

	int m_index;
	bool m_highlighted = false;
};
