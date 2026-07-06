#pragma once

#include <QVBoxLayout>
#include <QPushButton>
#include <QListWidgetItem>
#include <obs-frontend-api.h>
#include <QCloseEvent>

class SourceSelectorWindow final : public QWidget {
	Q_OBJECT
public:
	SourceSelectorWindow() = delete;

	explicit SourceSelectorWindow(QWidget *parent);

	~SourceSelectorWindow() override;

	SourceSelectorWindow(const SourceSelectorWindow &) = delete;

	SourceSelectorWindow(SourceSelectorWindow &&) = delete;

	SourceSelectorWindow &operator=(const SourceSelectorWindow &) = delete;

	SourceSelectorWindow &operator=(const SourceSelectorWindow &&) = delete;

	void refreshSourceList(const QList<QString> &excludedSources = QList<QString>());

	void detach();

protected:
	bool event(QEvent *event) override;

	void showEvent(QShowEvent *event) override;

	void hideEvent(QHideEvent *event) override;

private slots:
	void onAddButtonClicked();
	void onCancelButtonClicked();

private:
	void processSourceCallback(const obs_source_t *source, const QList<QString> *excludedSources) const;

	uint64_t m_sourceModificationSignalKey = 0;
	bool m_detached{false};
	QVBoxLayout *m_layout;
	QHBoxLayout *m_buttonLayout;
	QListWidget *m_listWidget;
	QPushButton *m_addButton;
	QPushButton *m_cancelButton;
};
