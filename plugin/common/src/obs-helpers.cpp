#include <obs-helpers.h>

#include <obs-frontend-api.h>
#include <obs.h>

#include <QIcon>
#include <QListWidgetItem>
#include <mutex>
#include <ranges>

#include <plugin-support.h>

namespace obs_helpers {

QIcon getIconFromSource(const obs_source *source) {
	if (!source) {
		return {};
	}

	const char *id = obs_source_get_id(source);
	const obs_icon_type type = obs_source_get_icon_type(id);
	const QWidget *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());

	if (!mainWindow) {
		static bool logged = false;
		if (!logged) {
			obs_log(LOG_WARNING, "main window unavailable, source icons will not be shown");
			logged = true;
		}
		return {};
	}

	switch (type) {
	case OBS_ICON_TYPE_IMAGE:
		return mainWindow->property("imageIcon").value<QIcon>();
	case OBS_ICON_TYPE_COLOR:
		return mainWindow->property("colorIcon").value<QIcon>();
	case OBS_ICON_TYPE_SLIDESHOW:
		return mainWindow->property("slideshowIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_INPUT:
		return mainWindow->property("audioInputIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return mainWindow->property("audioOutputIcon").value<QIcon>();
	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		return mainWindow->property("audioProcessOutputIcon").value<QIcon>();
	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return mainWindow->property("desktopCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return mainWindow->property("windowCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_GAME_CAPTURE:
		return mainWindow->property("gameCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_CAMERA:
		return mainWindow->property("cameraIcon").value<QIcon>();
	case OBS_ICON_TYPE_TEXT:
		return mainWindow->property("textIcon").value<QIcon>();
	case OBS_ICON_TYPE_MEDIA:
		return mainWindow->property("mediaIcon").value<QIcon>();
	case OBS_ICON_TYPE_BROWSER:
		return mainWindow->property("browserIcon").value<QIcon>();
	case OBS_ICON_TYPE_UNKNOWN:
	default:
		return mainWindow->property("defaultIcon").value<QIcon>();
	}
}

QIcon getIconFromPath(const QString &path) {
	const bool darkTheme = obs_frontend_is_theme_dark();
	const QString resource = QString("theme:%1/%2").arg(darkTheme ? "Dark" : "Light").arg(path);
	return QIcon(resource);
}

std::vector<std::pair<uint64_t, std::function<void(calldata_t *cd)>>> s_callbacks;
uint64_t s_currentId = 1;
std::mutex s_callbacksMutex;

void onSourceListChange(void *, calldata_t *cd) {
	std::lock_guard<std::mutex> lock(s_callbacksMutex);
	for (const auto &callback : s_callbacks | std::views::values) {
		callback(cd);
	}
}

void disconnectSourceEditSignals(const uint64_t id) {
	if (id == 0) {
		return;
	}

	std::lock_guard<std::mutex> lock(s_callbacksMutex);
	auto it = s_callbacks.begin();
	bool found = false;

	while (it != s_callbacks.end()) {
		if (it->first == id) {
			s_callbacks.erase(it);
			found = true;
			break;
		}

		++it;
	}

	if (!found) {
		return;
	}

	if (s_callbacks.empty()) {
		signal_handler_t *handler = obs_get_signal_handler();
		if (!handler) {
			return;
		}

		signal_handler_disconnect(handler, "source_create", onSourceListChange, nullptr);
		signal_handler_disconnect(handler, "source_destroy", onSourceListChange, nullptr);
		signal_handler_disconnect(handler, "source_rename", onSourceListChange, nullptr);
	}
}

uint64_t connectSourceEditSignals(std::function<void(calldata_t *cd)> callback) {
	std::lock_guard<std::mutex> lock(s_callbacksMutex);
	s_callbacks.emplace_back(s_currentId, std::move(callback));
	s_currentId++;

	if (s_callbacks.size() == 1) {
		signal_handler_t *handler = obs_get_signal_handler();
		assert(handler);

		signal_handler_connect(handler, "source_create", onSourceListChange, nullptr);
		signal_handler_connect(handler, "source_destroy", onSourceListChange, nullptr);
		signal_handler_connect(handler, "source_rename", onSourceListChange, nullptr);
	}

	return s_currentId - 1;
}

} // namespace obs_helpers
