#pragma once

#include <QEvent>
#include <QList>
#include <utility>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class SourceAddedEvent final : public QEvent {
public:
	static Type type;

	explicit SourceAddedEvent(QList<QString> names) : QEvent(type), m_names(std::move(names)) {}

	NO_DISCARD const QList<QString> &names() const { return m_names; }

	NO_DISCARD SourceAddedEvent *clone() const override { return new SourceAddedEvent(*this); }

private:
	QList<QString> m_names;
};

class SourceRemovedEvent final : public QEvent {
public:
	static Type type;

	explicit SourceRemovedEvent(QList<QString> names) : QEvent(type), m_names(std::move(names)) {}

	NO_DISCARD const QList<QString> &names() const { return m_names; }

	NO_DISCARD SourceRemovedEvent *clone() const override { return new SourceRemovedEvent(*this); }

private:
	QList<QString> m_names;
};
