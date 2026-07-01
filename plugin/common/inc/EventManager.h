#pragma once

#include <QObject>
#include <QPointer>
#include <QEvent>
#include <vector>
#include <mutex>

class EventManager final {
public:
	EventManager(const EventManager &) = delete;
	EventManager &operator=(const EventManager &) = delete;

	static EventManager *get();
	static void destroy();

	void addFrontendEventListener(const QPointer<QObject> &listener);
	void addBackendEventListener(const QPointer<QObject> &listener);
	void sendFrontendEvent(QEvent *event);
	void sendBackendEvent(QEvent *event);
	void sendGlobalEvent(QEvent *event);

private:
	EventManager() = default;
	~EventManager() = default;

	static void sendEvent(std::vector<QPointer<QObject>> &to, QEvent *event, bool ownership = true);

	static EventManager *s_instance;
	static std::mutex s_mutex;

	std::vector<QPointer<QObject>> m_frontendObjects;
	std::vector<QPointer<QObject>> m_backendObjects;
};
