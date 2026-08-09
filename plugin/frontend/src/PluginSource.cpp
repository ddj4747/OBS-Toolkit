#include <PluginSource.h>

#include <QByteArray>

const char *PluginSource::id() {
	static const QByteArray cached = QByteArray(PLUGIN_NAME) + " Source";
	return cached.constData();
}

void PluginSource::registerType() {
	obs_source_info info{};
	info.id = id();
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO;
	info.get_name = OnGetName;
	info.create = OnCreate;
	info.destroy = OnDestroy;
	info.get_width = OnGetWidth;
	info.get_height = OnGetHeight;

	obs_register_source(&info);
}

obs_source_t *PluginSource::create(const QString &name) {
	return obs_source_create(id(), name.toUtf8().constData(), nullptr, nullptr);
}

PluginSource *PluginSource::fromSource(obs_source_t *source) {
	return static_cast<PluginSource *>(obs_obj_get_data(source));
}

PluginSource::PluginSource(obs_data_t *, obs_source_t *source) : m_source(source) {}

PluginSource::~PluginSource() = default;

obs_source_t *PluginSource::getSource() const {
	return m_source;
}

uint32_t PluginSource::width() const {
	return m_width;
}

uint32_t PluginSource::height() const {
	return m_height;
}

const char *PluginSource::OnGetName(void *) {
	return id();
}

void *PluginSource::OnCreate(obs_data_t *settings, obs_source_t *source) {
	return new PluginSource(settings, source);
}

void PluginSource::OnDestroy(void *data) {
	delete static_cast<PluginSource *>(data);
}

uint32_t PluginSource::OnGetWidth(void *data) {
	return static_cast<PluginSource *>(data)->width();
}

uint32_t PluginSource::OnGetHeight(void *data) {
	return static_cast<PluginSource *>(data)->height();
}
