#pragma once

#include <obs-module.h>
#include <plugin-support.h>
#include <array>
#include <string>
#include <vector>

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
	static void setIconOutlineFieldsVisible(obs_properties_t *props, bool outline);
	static bool onIconOutlineChanged(obs_properties_t *props, obs_property_t *property, obs_data_t *settings);
	static void setFontDefaults(obs_data_t *settings);
	static int qualityBars(uint64_t bitsPerSecond, uint32_t width, uint32_t height, int codecId);
	static double codecBitrateFactor(int codecId);

	struct IconPlacement {
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t gap = 0;
		uint32_t offsetY = 0;
	};

	struct IconMask {
		uint32_t cx = 0;
		uint32_t cy = 0;
		std::vector<uint8_t> pixels;
	};

	static constexpr uint32_t kIconPadding = 96;

	void createTextSource();
	void loadIcon();
	void freeIcon();
	void applyAppearance(obs_data_t *settings, const std::string &text);
	void applyText(const std::string &text);
	void refreshText();
	void updateIconTexture(int level) const;
	void drawBackground(uint32_t width, uint32_t height) const;
	void drawIcon(uint32_t iconWidth, uint32_t iconHeight) const;
	void render() const;

	NO_DISCARD uint32_t width() const;
	NO_DISCARD uint32_t height() const;
	NO_DISCARD std::string bitrateText() const;
	NO_DISCARD bool shouldDrawIcon() const;
	NO_DISCARD IconPlacement iconPlacement(uint32_t textHeight) const;

	obs_source_t *m_source = nullptr;
	obs_source_t *m_textSource = nullptr;
	std::array<IconMask, 5> m_iconMasks{};
	gs_effect_t *m_iconEffect = nullptr;
	mutable gs_texture_t *m_iconTexture = nullptr;
	mutable int m_iconLevel = -1;
	mutable uint32_t m_iconTextureWidth = 0;
	mutable uint32_t m_iconTextureHeight = 0;
	std::string m_trackedName;
	std::string m_shownText;
	BitrateFormat m_format = BitrateFormat::Auto;
	uint32_t m_color = 0xFFFFFF;
	uint32_t m_backgroundColor = 0;
	uint32_t m_iconColor = 0xFFFFFF;
	uint32_t m_iconOutlineColor = 0;
	int m_opacity = 100;
	int m_backgroundOpacity = 0;
	int m_iconOpacity = 100;
	int m_iconOutlineSize = 2;
	int m_iconOutlineOpacity = 100;
	bool m_outline = true;
	bool m_iconOutline = true;
	bool m_showSourceName = true;
	bool m_textChildAdded = false;
	bool m_iconLoaded = false;
};
