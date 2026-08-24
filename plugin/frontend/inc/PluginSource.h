#pragma once

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QString>
#include <plugin-support.h>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class PluginSource {
public:
	static void registerType();
	static obs_source_t *create(const QString &name);
	static PluginSource *fromSource(obs_source_t *source);
	static const char *id();

	NO_DISCARD obs_source_t *getSource() const;

	PluginSource(const PluginSource &) = delete;
	PluginSource &operator=(const PluginSource &) = delete;

private:
	PluginSource(obs_data_t *settings, obs_source_t *source);
	~PluginSource();

	static const char *OnGetName(void *type_data);
	static void *OnCreate(obs_data_t *settings, obs_source_t *source);
	static void OnDestroy(void *data);
	static uint32_t OnGetWidth(void *data);
	static uint32_t OnGetHeight(void *data);
	static obs_properties_t *OnGetProperties(void *data);

	NO_DISCARD uint32_t width() const;
	NO_DISCARD uint32_t height() const;

	obs_source_t *m_source = nullptr;
	uint32_t m_width{};
	uint32_t m_height{};
};
