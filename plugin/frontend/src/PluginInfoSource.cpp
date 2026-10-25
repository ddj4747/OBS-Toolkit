#include <PluginInfoSource.h>
#include <PluginSource.h>

#include <QByteArray>
#include <graphics/image-file.h>
#include <algorithm>
#include <format>
#include <optional>
#include <string_view>

vec4 straightColor(const uint32_t color, const int opacity) {
	const float alpha = static_cast<float>(std::clamp(opacity, 0, 100)) / 100.0f;
	vec4 result;
	vec4_set(&result, static_cast<float>(color & 0xFFu) / 255.0f, static_cast<float>((color >> 8) & 0xFFu) / 255.0f,
		 static_cast<float>((color >> 16) & 0xFFu) / 255.0f, alpha);
	return result;
}

void PluginInfoSource::setOutlineFieldsVisible(obs_properties_t *props, const bool outline) {
	obs_property_set_visible(obs_properties_get(props, "outline_size"), outline);
	obs_property_set_visible(obs_properties_get(props, "outline_color"), outline);
	obs_property_set_visible(obs_properties_get(props, "outline_opacity"), outline);
}

bool PluginInfoSource::onOutlineChanged(obs_properties_t *props, obs_property_t *, obs_data_t *settings) {
	setOutlineFieldsVisible(props, obs_data_get_bool(settings, "outline"));
	return true;
}

void PluginInfoSource::setIconOutlineFieldsVisible(obs_properties_t *props, const bool outline) {
	obs_property_set_visible(obs_properties_get(props, "IconOutlineSize"), outline);
	obs_property_set_visible(obs_properties_get(props, "IconOutlineColor"), outline);
	obs_property_set_visible(obs_properties_get(props, "IconOutlineOpacity"), outline);
}

bool PluginInfoSource::onIconOutlineChanged(obs_properties_t *props, obs_property_t *, obs_data_t *settings) {
	setIconOutlineFieldsVisible(props, obs_data_get_bool(settings, "IconOutline"));
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
	loadIcon();
	readSettings(settings, this);
	applyAppearance(settings, bitrateText());
}

PluginInfoSource::~PluginInfoSource() {
	if (m_textSource) {
		if (m_textChildAdded) {
			obs_source_remove_active_child(m_source, m_textSource);
		}
		obs_source_release(m_textSource);
		m_textSource = nullptr;
	}

	obs_enter_graphics();
	freeIcon();
	obs_leave_graphics();
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

void PluginInfoSource::loadIcon() {
	for (int level = 0; level < static_cast<int>(m_iconMasks.size()); ++level) {
		const std::string fileName = std::format("quality-{}.png", level);
		char *path = obs_module_file(fileName.c_str());
		if (!path) {
			obs_log(LOG_WARNING, "Quality icon %s was not found", fileName.c_str());
			return;
		}

		gs_image_file4 image{};
		gs_image_file4_init(&image, path, GS_IMAGE_ALPHA_STRAIGHT);
		bfree(path);

		gs_image_file_t &source = image.image3.image2.image;
		const size_t byteCount = static_cast<size_t>(source.cx) * source.cy * 4;
		if (!source.loaded || source.format != GS_RGBA || !source.texture_data || source.cx == 0 ||
		    source.cy == 0) {
			obs_log(LOG_WARNING, "Failed to read quality icon %s", fileName.c_str());
			gs_image_file4_free(&image);
			return;
		}

		m_iconMasks[level].cx = source.cx;
		m_iconMasks[level].cy = source.cy;
		m_iconMasks[level].pixels.assign(source.texture_data, source.texture_data + byteCount);
		gs_image_file4_free(&image);
	}

	char *effectPath = obs_module_file("bitrate-icon.effect");
	char *error = nullptr;
	obs_enter_graphics();
	if (effectPath) {
		m_iconEffect = gs_effect_create_from_file(effectPath, &error);
	}
	obs_leave_graphics();
	bfree(effectPath);
	if (error) {
		obs_log(LOG_WARNING, "Failed to create bitrate icon effect: %s", error);
		bfree(error);
	}
	if (!m_iconEffect) {
		obs_log(LOG_WARNING, "Failed to load bitrate icon effect");
		return;
	}

	m_iconLoaded = true;
}

void PluginInfoSource::freeIcon() {
	if (m_iconEffect) {
		gs_effect_destroy(m_iconEffect);
		m_iconEffect = nullptr;
	}
	if (m_iconTexture) {
		gs_texture_destroy(m_iconTexture);
		m_iconTexture = nullptr;
	}
	m_iconLevel = -1;
	m_iconTextureWidth = 0;
	m_iconTextureHeight = 0;
	m_iconLoaded = false;
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
	obs_data_set_int(textSettings, "bk_color", 0);
	obs_data_set_int(textSettings, "bk_opacity", 0);
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
		if (!m_showSourceName) {
			return "unavailable";
		}
		return std::format("{}: unavailable", m_trackedName);
	}

	const std::string formatted = formatBitrate(*bitrate, m_format);
	if (!m_showSourceName) {
		return formatted;
	}
	return std::format("{}: {}", m_trackedName, formatted);
}

double PluginInfoSource::codecBitrateFactor(const int codecId) {
	switch (static_cast<AVCodecID>(codecId)) {
	case AV_CODEC_ID_AV1:
		return 0.50;
	case AV_CODEC_ID_VP9:
		return 0.65;
	case AV_CODEC_ID_HEVC:
		return 0.70;
	case AV_CODEC_ID_H264:
		return 1.0;
	case AV_CODEC_ID_VP8:
		return 1.10;
	case AV_CODEC_ID_MPEG4:
		return 1.60;
	case AV_CODEC_ID_MJPEG:
		return 6.0;
	default:
		return 1.0;
	}
}

int PluginInfoSource::qualityBars(const uint64_t bitsPerSecond, const uint32_t width, const uint32_t height,
				  const int codecId) {
	if (bitsPerSecond == 0 || width == 0 || height == 0) {
		return 0;
	}

	constexpr double kHealthyBitsPerPixel = 0.1 * 30.0;
	const double pixels = static_cast<double>(width) * static_cast<double>(height);
	const double ratio =
		(static_cast<double>(bitsPerSecond) / pixels) / (kHealthyBitsPerPixel * codecBitrateFactor(codecId));
	if (ratio >= 1.0) {
		return 4;
	}
	if (ratio >= 0.70) {
		return 3;
	}
	if (ratio >= 0.50) {
		return 2;
	}
	return 1;
}

bool PluginInfoSource::shouldDrawIcon() const {
	if (!m_iconLoaded || m_trackedName.empty()) {
		return false;
	}
	return PluginSource::bitrateForName(m_trackedName).has_value();
}

PluginInfoSource::IconPlacement PluginInfoSource::iconPlacement(const uint32_t textHeight) const {
	IconPlacement placement;
	if (!shouldDrawIcon() || textHeight == 0) {
		return placement;
	}

	const uint32_t maskWidth = m_iconMasks[0].cx;
	if (maskWidth == 0) {
		return placement;
	}

	const uint32_t paddedWidth = maskWidth + kIconPadding * 2;
	placement.height = std::max(1u, textHeight * paddedWidth / maskWidth);
	placement.width = placement.height;
	placement.gap = std::max(4u, textHeight / 6u);
	placement.offsetY = textHeight > placement.height ? (textHeight - placement.height) / 2u : 0u;
	return placement;
}

uint32_t PluginInfoSource::width() const {
	if (!m_textSource) {
		return 0;
	}

	const uint32_t textWidth = obs_source_get_width(m_textSource);
	const IconPlacement icon = iconPlacement(obs_source_get_height(m_textSource));
	if (icon.width == 0) {
		return textWidth;
	}
	return textWidth + icon.gap + icon.width;
}

uint32_t PluginInfoSource::height() const {
	if (!m_textSource) {
		return 0;
	}

	const uint32_t textHeight = obs_source_get_height(m_textSource);
	return std::max(textHeight, iconPlacement(textHeight).height);
}

void PluginInfoSource::updateIconTexture(const int level) const {
	const IconMask &mask = m_iconMasks[static_cast<size_t>(level)];
	if (mask.pixels.empty() || mask.cx == 0 || mask.cy == 0) {
		return;
	}
	if (m_iconTexture && m_iconLevel == level) {
		return;
	}

	const uint32_t paddedWidth = mask.cx + kIconPadding * 2;
	const uint32_t paddedHeight = mask.cy + kIconPadding * 2;
	std::vector<uint8_t> padded(static_cast<size_t>(paddedWidth) * paddedHeight * 4, 0);
	for (uint32_t y = 0; y < mask.cy; ++y) {
		const uint8_t *sourceRow = mask.pixels.data() + static_cast<size_t>(y) * mask.cx * 4;
		uint8_t *destRow =
			padded.data() + (static_cast<size_t>(y + kIconPadding) * paddedWidth + kIconPadding) * 4;
		std::copy_n(sourceRow, static_cast<size_t>(mask.cx) * 4, destRow);
	}

	const uint8_t *pixels = padded.data();
	if (m_iconTexture) {
		gs_texture_destroy(m_iconTexture);
		m_iconTexture = nullptr;
	}
	m_iconTexture = gs_texture_create(paddedWidth, paddedHeight, GS_RGBA, 1, &pixels, 0);
	m_iconLevel = level;
	m_iconTextureWidth = paddedWidth;
	m_iconTextureHeight = paddedHeight;
}

void PluginInfoSource::drawIcon(const uint32_t iconWidth, const uint32_t iconHeight) const {
	const std::optional<PluginSource::StreamStats> stats = PluginSource::statsForName(m_trackedName);
	if (!m_iconLoaded || !m_iconEffect || !stats.has_value() || iconWidth == 0 || iconHeight == 0) {
		return;
	}

	const int level = std::clamp(qualityBars(stats->bitrate, stats->width, stats->height, stats->codec), 0, 4);
	updateIconTexture(level);
	if (!m_iconTexture || m_iconTextureWidth == 0 || m_iconTextureHeight == 0) {
		return;
	}

	const vec4 fill = straightColor(m_iconColor, m_iconOpacity);
	const vec4 stroke = straightColor(m_iconOutlineColor, m_iconOutline ? m_iconOutlineOpacity : 0);
	const float radius = m_iconOutline
				     ? static_cast<float>(std::max(m_iconOutlineSize, 0)) *
					       static_cast<float>(m_iconTextureWidth) / static_cast<float>(iconWidth)
				     : 0.0f;
	vec2 pixelSize;
	vec2_set(&pixelSize, 1.0f / static_cast<float>(m_iconTextureWidth),
		 1.0f / static_cast<float>(m_iconTextureHeight));

	gs_effect_set_texture(gs_effect_get_param_by_name(m_iconEffect, "image"), m_iconTexture);
	gs_effect_set_vec4(gs_effect_get_param_by_name(m_iconEffect, "color"), &fill);
	gs_effect_set_vec4(gs_effect_get_param_by_name(m_iconEffect, "outline_color"), &stroke);
	gs_effect_set_vec2(gs_effect_get_param_by_name(m_iconEffect, "pixel_size"), &pixelSize);
	gs_effect_set_float(gs_effect_get_param_by_name(m_iconEffect, "outline_radius"), radius);
	while (gs_effect_loop(m_iconEffect, "Draw")) {
		gs_draw_sprite(m_iconTexture, 0, iconWidth, iconHeight);
	}
}

void PluginInfoSource::drawBackground(const uint32_t width, const uint32_t height) const {
	if (width == 0 || height == 0 || m_backgroundOpacity <= 0) {
		return;
	}

	const float alpha = static_cast<float>(std::clamp(m_backgroundOpacity, 0, 100)) / 100.0f;
	const float red = static_cast<float>(m_backgroundColor & 0xFFu) / 255.0f;
	const float green = static_cast<float>((m_backgroundColor >> 8) & 0xFFu) / 255.0f;
	const float blue = static_cast<float>((m_backgroundColor >> 16) & 0xFFu) / 255.0f;
	vec4 color;
	vec4_set(&color, red * alpha, green * alpha, blue * alpha, alpha);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &color);
	while (gs_effect_loop(solid, "Solid")) {
		gs_draw_sprite(nullptr, 0, width, height);
	}
}

void PluginInfoSource::render() const {
	if (!m_textSource) {
		return;
	}

	const uint32_t textWidth = obs_source_get_width(m_textSource);
	const uint32_t textHeight = obs_source_get_height(m_textSource);
	const IconPlacement icon = iconPlacement(textHeight);
	const uint32_t sourceHeight = std::max(textHeight, icon.height);
	const uint32_t fullWidth = icon.width == 0 ? textWidth : textWidth + icon.gap + icon.width;
	const float textY = sourceHeight > textHeight ? static_cast<float>(sourceHeight - textHeight) * 0.5f : 0.0f;
	drawBackground(fullWidth, sourceHeight);

	gs_matrix_push();
	gs_matrix_translate3f(0.0f, textY, 0.0f);
	obs_source_video_render(m_textSource);
	gs_matrix_pop();

	if (icon.width == 0) {
		return;
	}

	const float iconY = sourceHeight > icon.height ? static_cast<float>(sourceHeight - icon.height) * 0.5f : 0.0f;
	gs_matrix_push();
	gs_matrix_translate3f(static_cast<float>(textWidth + icon.gap), iconY, 0.0f);
	drawIcon(icon.width, icon.height);
	gs_matrix_pop();
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
	if (!info->m_trackedName.empty() && std::ranges::find(names, info->m_trackedName) == names.end()) {
		names.push_back(info->m_trackedName);
		std::ranges::sort(names);
	}

	for (const std::string &name : names) {
		obs_property_list_add_string(sourceList, name.c_str(), name.c_str());
	}

	obs_properties_add_bool(props, "ShowSourceName", "Show source name");

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

	obs_properties_t *iconProps = obs_properties_create();
	obs_properties_add_color(iconProps, "IconColor", "Color");
	obs_properties_add_int_slider(iconProps, "IconOpacity", "Opacity", 0, 100, 1);
	obs_property_t *iconOutline = obs_properties_add_bool(iconProps, "IconOutline", "Outline");
	obs_property_set_modified_callback(iconOutline, onIconOutlineChanged);
	obs_properties_add_int_slider(iconProps, "IconOutlineSize", "Outline size", 1, 10, 1);
	obs_properties_add_color(iconProps, "IconOutlineColor", "Outline color");
	obs_properties_add_int_slider(iconProps, "IconOutlineOpacity", "Outline opacity", 0, 100, 1);
	setIconOutlineFieldsVisible(iconProps, info->m_iconOutline);
	obs_properties_add_group(props, "IconStyle", "Icon", OBS_GROUP_NORMAL, iconProps);

	return props;
}

void PluginInfoSource::OnGetDefaults(obs_data_t *settings) {
	obs_data_set_default_string(settings, "TrackedSource", "");
	obs_data_set_default_bool(settings, "ShowSourceName", true);
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
	obs_data_set_default_int(settings, "IconColor", 0xFFFFFF);
	obs_data_set_default_int(settings, "IconOpacity", 100);
	obs_data_set_default_bool(settings, "IconOutline", true);
	obs_data_set_default_int(settings, "IconOutlineSize", 2);
	obs_data_set_default_int(settings, "IconOutlineColor", 0);
	obs_data_set_default_int(settings, "IconOutlineOpacity", 100);
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
	source->m_showSourceName = obs_data_get_bool(settings, "ShowSourceName");
	source->m_format = bitrateFormatFromString(obs_data_get_string(settings, "BitrateFormat"));
	source->m_outline = obs_data_get_bool(settings, "outline");
	source->m_color = static_cast<uint32_t>(obs_data_get_int(settings, "color"));
	source->m_opacity = static_cast<int>(obs_data_get_int(settings, "opacity"));
	source->m_backgroundColor = static_cast<uint32_t>(obs_data_get_int(settings, "bk_color"));
	source->m_backgroundOpacity = static_cast<int>(obs_data_get_int(settings, "bk_opacity"));
	source->m_iconColor = static_cast<uint32_t>(obs_data_get_int(settings, "IconColor"));
	source->m_iconOpacity = static_cast<int>(obs_data_get_int(settings, "IconOpacity"));
	source->m_iconOutline = obs_data_get_bool(settings, "IconOutline");
	source->m_iconOutlineSize = static_cast<int>(obs_data_get_int(settings, "IconOutlineSize"));
	source->m_iconOutlineColor = static_cast<uint32_t>(obs_data_get_int(settings, "IconOutlineColor"));
	source->m_iconOutlineOpacity = static_cast<int>(obs_data_get_int(settings, "IconOutlineOpacity"));
}
