#pragma once

#include <QVBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QListWidgetItem>
#include <obs-frontend-api.h>
#include <QCloseEvent>

class SourceSelectorWindow final : public QWidget {
	Q_OBJECT
public:
	SourceSelectorWindow() = delete;

	explicit SourceSelectorWindow(QWidget *parent);

	~SourceSelectorWindow();

	SourceSelectorWindow(const SourceSelectorWindow &) = delete;

	SourceSelectorWindow(SourceSelectorWindow &&) = delete;

	SourceSelectorWindow &operator=(const SourceSelectorWindow &) = delete;

	SourceSelectorWindow &operator=(const SourceSelectorWindow &&) = delete;

	void refreshSourceList(const QList<QString> &excludedSources = QList<QString>());

protected:
	void showEvent(QShowEvent *event) override;

	void hideEvent(QHideEvent *event) override;

private:
	void processSourceCallback(const obs_source_t *source, const QList<QString> *excludedSources) const;

	static void onSourceListChange(void *data, calldata_t *cd);

	static QIcon getIconForSource(const obs_source_t *source);

	QVBoxLayout *m_layout;
	QHBoxLayout *m_buttonLayout;
	QListWidget *m_listWidget;
	QPushButton *m_addButton;
	QPushButton *m_cancelButton;
};
