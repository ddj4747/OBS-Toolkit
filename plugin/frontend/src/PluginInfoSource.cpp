#include <PluginInfoSource.h>
#include <PluginSource.h>

#include <QByteArray>
#include <algorithm>
#include <format>
#include <optional>
#include <string_view>

void PluginInfoSource::setOutlineFieldsVisible(obs_properties_t *props, const bool outline) {
	obs_property_set_visible(obs_properties_get(props, "outline_size"), outline);
	obs_property_set_visible(obs_properties_get(props, "outline_color"), outline);
	obs_property_set_visible(obs_properties_get(props, "outline_opacity"), outline);
}

bool PluginInfoSource::onOutlineChanged(obs_properties_t *props, obs_property_t *, obs_data_t *settings) {
	setOutlineFieldsVisible(props, obs_data_get_bool(settings, "outline"));
	return true;
}

void PluginInfoSource::setFontDefaults(obs_data_t *settings) {
	obs_data_t *font = obs_data_create();
	obs_data_set_default_string(font, "face", "Arial");
	obs_data_set_default_string(font, "style", "Regular");
	obs_data_set_default_int(font, "size", 48);
	obs_data_set_default_int(font, "flags", 0);
	obs_data_set_default_obj(settings, "font", font);
	obs_data_release(font);
}

void PluginInfoSource::registerType() {
	obs_source_info info{};

	info.id = id();
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	info.get_name = OnGetName;
	info.create = OnCreate;
	info.destroy = OnDestroy;
	info.get_width = OnGetWidth;
	info.get_height = OnGetHeight;
	info.get_properties = OnGetProperties;
	info.get_defaults = OnGetDefaults;
	info.update = OnUpdate;
	info.video_tick = OnVideoTick;
	info.video_render = OnVideoRender;

	obs_register_source(&info);
}

PluginInfoSource *PluginInfoSource::fromSource(obs_source_t *source) {
	return static_cast<PluginInfoSource *>(obs_obj_get_data(source));
}

const char *PluginInfoSource::id() {
	static const QByteArray cached = QByteArray(PLUGIN_NAME) + " Info Source";
	return cached.constData();
}

PluginInfoSource::PluginInfoSource(obs_data_t *settings, obs_source_t *source) : m_source(source) {
	createTextSource();
	readSettings(settings, this);
	applyAppearance(settings, bitrateText());
}

PluginInfoSource::~PluginInfoSource() {
	if (!m_textSource) {
		return;
	}

	if (m_textChildAdded) {
		obs_source_remove_active_child(m_source, m_textSource);
	}
	obs_source_release(m_textSource);
	m_textSource = nullptr;
}

void PluginInfoSource::createTextSource() {
	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "text", "");
	obs_data_set_bool(settings, "antialiasing", true);
	m_textSource = obs_source_create_private("text_gdiplus", "info-text", settings);
	obs_data_release(settings);

	if (!m_textSource) {
		obs_log(LOG_ERROR, "Failed to create text_gdiplus source");
		return;
	}

	m_textChildAdded = obs_source_add_active_child(m_source, m_textSource);
}

void PluginInfoSource::applyAppearance(obs_data_t *settings, const std::string &text) {
	m_shownText = text;
	if (!m_textSource) {
		return;
	}

	obs_data_t *textSettings = obs_source_get_settings(m_textSource);
	obs_data_t *font = obs_data_get_obj(settings, "font");
	if (font) {
		obs_data_set_obj(textSettings, "font", font);
		obs_data_release(font);
	}

	obs_data_set_int(textSettings, "color", obs_data_get_int(settings, "color"));
	obs_data_set_int(textSettings, "opacity", obs_data_get_int(settings, "opacity"));
	obs_data_set_bool(textSettings, "outline", obs_data_get_bool(settings, "outline"));
	obs_data_set_int(textSettings, "outline_size", obs_data_get_int(settings, "outline_size"));
	obs_data_set_int(textSettings, "outline_color", obs_data_get_int(settings, "outline_color"));
	obs_data_set_int(textSettings, "outline_opacity", obs_data_get_int(settings, "outline_opacity"));
	obs_data_set_int(textSettings, "bk_color", obs_data_get_int(settings, "bk_color"));
	obs_data_set_int(textSettings, "bk_opacity", obs_data_get_int(settings, "bk_opacity"));
	obs_data_set_bool(textSettings, "antialiasing", true);
	obs_data_set_string(textSettings, "text", text.c_str());

	obs_source_update(m_textSource, textSettings);
	obs_data_release(textSettings);
}

void PluginInfoSource::applyText(const std::string &text) {
	if (!m_textSource || text == m_shownText) {
		return;
	}

	m_shownText = text;
	obs_data_t *textSettings = obs_source_get_settings(m_textSource);
	obs_data_set_string(textSettings, "text", text.c_str());
	obs_source_update(m_textSource, textSettings);
	obs_data_release(textSettings);
}

void PluginInfoSource::refreshText() {
	applyText(bitrateText());
}

PluginInfoSource::BitrateFormat PluginInfoSource::bitrateFormatFromString(const char *value) {
	const std::string_view format = value ? value : "";
	if (format == "bps") {
		return BitrateFormat::Bits;
	}
	if (format == "kbps") {
		return BitrateFormat::Kilobits;
	}
	if (format == "mbps") {
		return BitrateFormat::Megabits;
	}
	if (format == "Bps") {
		return BitrateFormat::Bytes;
	}
	if (format == "KBps") {
		return BitrateFormat::Kilobytes;
	}
	if (format == "MBps") {
		return BitrateFormat::Megabytes;
	}
	return BitrateFormat::Auto;
}

std::string PluginInfoSource::formatBitrate(const uint64_t bitsPerSecond, const BitrateFormat format) {
	switch (format) {
	case BitrateFormat::Bits:
		return std::format("{} bps", bitsPerSecond);
	case BitrateFormat::Kilobits:
		return std::format("{:.1f} kbps", static_cast<double>(bitsPerSecond) / 1000.0);
	case BitrateFormat::Megabits:
		return std::format("{:.2f} Mbps", static_cast<double>(bitsPerSecond) / 1000000.0);
	case BitrateFormat::Bytes:
		return std::format("{} B/s", bitsPerSecond / 8);
	case BitrateFormat::Kilobytes:
		return std::format("{:.1f} KB/s", static_cast<double>(bitsPerSecond) / 8000.0);
	case BitrateFormat::Megabytes:
		return std::format("{:.2f} MB/s", static_cast<double>(bitsPerSecond) / 8000000.0);
	case BitrateFormat::Auto:
		break;
	}

	if (bitsPerSecond >= 10000000ULL) {
		return std::format("{:.0f} Mbps", static_cast<double>(bitsPerSecond) / 1000000.0);
	}
	if (bitsPerSecond >= 1000000ULL) {
		return std::format("{:.1f} Mbps", static_cast<double>(bitsPerSecond) / 1000000.0);
	}
	return std::format("{} kbps", (bitsPerSecond + 500ULL) / 1000ULL);
}

std::string PluginInfoSource::bitrateText() const {
	if (m_trackedName.empty()) {
		return "No source";
	}

	const std::optional<uint64_t> bitrate = PluginSource::bitrateForName(m_trackedName);
	if (!bitrate.has_value()) {
		return std::format("{}: unavailable", m_trackedName);
	}

	return std::format("{}: {}", m_trackedName, formatBitrate(*bitrate, m_format));
}

uint32_t PluginInfoSource::width() const {
	return m_textSource ? obs_source_get_width(m_textSource) : 0;
}

uint32_t PluginInfoSource::height() const {
	return m_textSource ? obs_source_get_height(m_textSource) : 0;
}

void PluginInfoSource::render() const {
	if (!m_textSource) {
		return;
	}

	obs_source_video_render(m_textSource);
}

const char *PluginInfoSource::OnGetName(void *) {
	return id();
}

void *PluginInfoSource::OnCreate(obs_data_t *settings, obs_source_t *source) {
	return new PluginInfoSource(settings, source);
}

void PluginInfoSource::OnDestroy(void *data) {
	delete static_cast<PluginInfoSource *>(data);
}

uint32_t PluginInfoSource::OnGetWidth(void *data) {
	return static_cast<PluginInfoSource *>(data)->width();
}

uint32_t PluginInfoSource::OnGetHeight(void *data) {
	return static_cast<PluginInfoSource *>(data)->height();
}

obs_properties_t *PluginInfoSource::OnGetProperties(void *data) {
	const PluginInfoSource *info = static_cast<PluginInfoSource *>(data);
	obs_properties_t *props = obs_properties_create();
	obs_property_t *sourceList = obs_properties_add_list(props, "TrackedSource", "Tracked source",
							     OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	std::vector<std::string> names = PluginSource::sourceNames();
	if (!info->m_trackedName.empty() && std::find(names.begin(), names.end(), info->m_trackedName) == names.end()) {
		names.push_back(info->m_trackedName);
		std::sort(names.begin(), names.end());
	}

	for (const std::string &name : names) {
		obs_property_list_add_string(sourceList, name.c_str(), name.c_str());
	}

	obs_property_t *formatList = obs_properties_add_list(props, "BitrateFormat", "Bitrate format",
							     OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(formatList, "Auto", "auto");
	obs_property_list_add_string(formatList, "bps", "bps");
	obs_property_list_add_string(formatList, "kbps", "kbps");
	obs_property_list_add_string(formatList, "Mbps", "mbps");
	obs_property_list_add_string(formatList, "B/s", "Bps");
	obs_property_list_add_string(formatList, "KB/s", "KBps");
	obs_property_list_add_string(formatList, "MB/s", "MBps");

	obs_properties_t *textProps = obs_properties_create();
	obs_properties_add_font(textProps, "font", "Font");
	obs_properties_add_color(textProps, "color", "Text color");
	obs_properties_add_int_slider(textProps, "opacity", "Text opacity", 0, 100, 1);
	obs_property_t *outline = obs_properties_add_bool(textProps, "outline", "Outline");
	obs_property_set_modified_callback(outline, onOutlineChanged);
	obs_properties_add_int_slider(textProps, "outline_size", "Outline size", 1, 20, 1);
	obs_properties_add_color(textProps, "outline_color", "Outline color");
	obs_properties_add_int_slider(textProps, "outline_opacity", "Outline opacity", 0, 100, 1);
	obs_properties_add_color(textProps, "bk_color", "Background color");
	obs_properties_add_int_slider(textProps, "bk_opacity", "Background opacity", 0, 100, 1);
	setOutlineFieldsVisible(textProps, info->m_outline);
	obs_properties_add_group(props, "TextStyle", "Text", OBS_GROUP_NORMAL, textProps);

	return props;
}

void PluginInfoSource::OnGetDefaults(obs_data_t *settings) {
	obs_data_set_default_string(settings, "TrackedSource", "");
	obs_data_set_default_string(settings, "BitrateFormat", "auto");
	setFontDefaults(settings);
	obs_data_set_default_int(settings, "color", 0xFFFFFF);
	obs_data_set_default_int(settings, "opacity", 100);
	obs_data_set_default_bool(settings, "outline", true);
	obs_data_set_default_int(settings, "outline_size", 2);
	obs_data_set_default_int(settings, "outline_color", 0);
	obs_data_set_default_int(settings, "outline_opacity", 100);
	obs_data_set_default_int(settings, "bk_color", 0);
	obs_data_set_default_int(settings, "bk_opacity", 0);
}

void PluginInfoSource::OnUpdate(void *data, obs_data_t *settings) {
	PluginInfoSource *source = static_cast<PluginInfoSource *>(data);
	readSettings(settings, source);
	source->applyAppearance(settings, source->bitrateText());
}

void PluginInfoSource::OnVideoTick(void *data, float) {
	static_cast<PluginInfoSource *>(data)->refreshText();
}

void PluginInfoSource::OnVideoRender(void *data, gs_effect_t *) {
	static_cast<PluginInfoSource *>(data)->render();
}

void PluginInfoSource::readSettings(obs_data_t *settings, PluginInfoSource *source) {
	const char *trackedSource = obs_data_get_string(settings, "TrackedSource");
	source->m_trackedName = trackedSource ? trackedSource : "";
	source->m_format = bitrateFormatFromString(obs_data_get_string(settings, "BitrateFormat"));
	source->m_outline = obs_data_get_bool(settings, "outline");
}
