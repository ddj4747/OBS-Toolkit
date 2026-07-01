#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QListWidget>
#include <QIcon>
#include <QPushButton>
#include <QListWidgetItem>
#include <obs-frontend-api.h>
#include <QShowEvent>

class SourceSelectorWindow final : public QWidget {
	Q_OBJECT
public:
	SourceSelectorWindow() = delete;

	explicit SourceSelectorWindow(QWidget *parent);

	void refreshSourceList(const QList<QString> &excludedSources = QList<QString>());

private:
	void processSourceCallback(const obs_source_t *source, const QList<QString> *excludedSources) const;

	static QIcon getIconForSource(const obs_source_t *source);

	std::unique_ptr<QVBoxLayout> m_layout;
	std::unique_ptr<QHBoxLayout> m_buttonLayout;
	std::unique_ptr<QListWidget> m_listWidget;
	std::unique_ptr<QPushButton> m_addButton;
	std::unique_ptr<QPushButton> m_cancelButton;
};
