#include <EventManager.h>
#include <QCoreApplication>

EventManager *EventManager::s_instance{nullptr};
std::mutex EventManager::s_mutex{};

EventManager *EventManager::get() {
	std::lock_guard<std::mutex> lock(EventManager::s_mutex);
	if (!s_instance) {
		s_instance = new EventManager();
	}

	return s_instance;
}

void EventManager::destroy() {
	std::lock_guard<std::mutex> lock(EventManager::s_mutex);
	if (s_instance) {
		delete s_instance;
		s_instance = nullptr;
	}
}

void EventManager::addFrontendEventListener(const QPointer<QObject> &listener) {
	std::lock_guard<std::mutex> lock(s_mutex);
	m_frontendObjects.push_back(listener);
}

void EventManager::addBackendEventListener(const QPointer<QObject> &listener) {
	std::lock_guard<std::mutex> lock(s_mutex);
	m_backendObjects.push_back(listener);
}

void EventManager::sendFrontendEvent(QEvent *event) {
	sendEvent(m_frontendObjects, event, true);
}

void EventManager::sendBackendEvent(QEvent *event) {
	sendEvent(m_backendObjects, event, true);
}

void EventManager::sendGlobalEvent(QEvent *event) {
	sendEvent(m_frontendObjects, event, false);
	sendEvent(m_backendObjects, event, true);
}

void EventManager::sendEvent(std::vector<QPointer<QObject>> &to, QEvent *event, const bool ownership) {
	if (!event) {
		return;
	}

	std::vector<QPointer<QObject>> targets;
	{
		std::lock_guard<std::mutex> lock(s_mutex);

		std::erase_if(to, [](const QPointer<QObject> &obj) { return obj.isNull(); });

		if (to.empty()) {

			if (ownership) {
				delete event;
			}

			return;
		}

		targets = to;
	}

	if (targets.empty()) {
		if (ownership) {
			delete event;
		}

		return;
	}

	for (size_t i = 0; i < targets.size() - ownership; ++i) {
		const auto &obj = targets[i];
		if (obj.isNull() || !QCoreApplication::instance()) {
			continue;
		}

		QCoreApplication::postEvent(obj.data(), event->clone());
	}

	// Last object will use the not cloned object
	// At least most of the time it will (probably) (most surely)
	if (ownership) {
		const auto &obj = targets.at(targets.size() - 1);
		if (obj.isNull() || !QCoreApplication::instance()) {
			delete event;
			return;
		}

		QCoreApplication::postEvent(obj.data(), event);
	}
}
