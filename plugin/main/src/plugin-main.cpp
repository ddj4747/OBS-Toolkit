/*
Plugin Name
Copyright (C) 2026 ddj4747

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <QMainWindow>

#include <EventManager.h>
#include <PluginFrontend.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {
void on_frontend_event(const obs_frontend_event event, void *) {
	if (event != OBS_FRONTEND_EVENT_FINISHED_LOADING)
		return;

	if (PluginFrontend::isRunning())
		return;

	PluginFrontend::start();
}
} // namespace

bool obs_module_load(void) {
	obs_frontend_add_event_callback(on_frontend_event, nullptr);
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void) {
	PluginFrontend::stop();
	EventManager::destroy();
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);
	obs_log(LOG_INFO, "plugin unloaded");
}
