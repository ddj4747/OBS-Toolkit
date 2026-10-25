#pragma once

#include <obs-module.h>
#include <plugin-support.h>
#include <string>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class PluginInfoSource {
public:
	static void registerType();
	static PluginInfoSource *fromSource(obs_source_t *source);
	static const char *id();

	PluginInfoSource(const PluginInfoSource &) = delete;
	PluginInfoSource &operator=(const PluginInfoSource &) = delete;

private:
	enum class BitrateFormat { Auto, Bits, Kilobits, Megabits, Bytes, Kilobytes, Megabytes };

	PluginInfoSource(obs_data_t *settings, obs_source_t *source);
	~PluginInfoSource();

	static const char *OnGetName(void *type_data);
	static void *OnCreate(obs_data_t *settings, obs_source_t *source);
	static void OnDestroy(void *data);
	static uint32_t OnGetWidth(void *data);
	static uint32_t OnGetHeight(void *data);
	static obs_properties_t *OnGetProperties(void *data);
	static void OnGetDefaults(obs_data_t *settings);
	static void OnUpdate(void *data, obs_data_t *settings);
	static void OnVideoTick(void *data, float seconds);
	static void OnVideoRender(void *data, gs_effect_t *effect);

	static void readSettings(obs_data_t *settings, PluginInfoSource *source);
	static BitrateFormat bitrateFormatFromString(const char *value);
	static std::string formatBitrate(uint64_t bitsPerSecond, BitrateFormat format);
	static void setOutlineFieldsVisible(obs_properties_t *props, bool outline);
	static bool onOutlineChanged(obs_properties_t *props, obs_property_t *property, obs_data_t *settings);
	static void setFontDefaults(obs_data_t *settings);

	void createTextSource();
	void applyAppearance(obs_data_t *settings, const std::string &text);
	void applyText(const std::string &text);
	void refreshText();
	void render() const;

	NO_DISCARD uint32_t width() const;
	NO_DISCARD uint32_t height() const;
	NO_DISCARD std::string bitrateText() const;

	obs_source_t *m_source = nullptr;
	obs_source_t *m_textSource = nullptr;
	std::string m_trackedName;
	std::string m_shownText;
	BitrateFormat m_format = BitrateFormat::Auto;
	bool m_outline = true;
	bool m_textChildAdded = false;
};
