#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class PluginWebPanelWindow : public QWidget {
	Q_OBJECT
public:
	PluginWebPanelWindow() = delete;

	explicit PluginWebPanelWindow(QWidget *parent);

	~PluginWebPanelWindow() override;

	PluginWebPanelWindow(const PluginWebPanelWindow &) = delete;

	PluginWebPanelWindow(PluginWebPanelWindow &&) = delete;

	PluginWebPanelWindow &operator=(const PluginWebPanelWindow &) = delete;

	PluginWebPanelWindow &operator=(const PluginWebPanelWindow &&) = delete;

protected:
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void onCopyClicked() const;
	void onGenerateClicked();
	void onEnabledChanged();

private:
	void loadSettings();
	void saveSettings() const;
	void applyAccessKey() const;
	void applyEnabled() const;
	void applyNewUrl() const;
	void refreshQrCode();
	void updateImagePreview();
	static QString generateAccessKey();

	QVBoxLayout *m_mainLayout;
	QCheckBox *m_enabledCheckBox;
	QLineEdit *m_accessKeyEdit;
	QPushButton *m_copyButton;
	QPushButton *m_generateButton;
	QLabel *m_qrLabel;
	QPixmap m_qrCode;
	QString m_accessKey;
	bool m_updatingImagePreview{false};
};
