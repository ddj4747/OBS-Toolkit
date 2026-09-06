#include <PluginSource.h>

#include <QByteArray>
#include <magic_enum/magic_enum.hpp>

namespace {
constexpr uint16_t SRT_FRAME_RECEIVER_PORT = 8890;
}

MediamtxManager *PluginSource::s_mediamtxManager = nullptr;

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
	info.get_properties = OnGetProperties;
	info.get_defaults = OnGetDefaults;
	info.update = OnUpdate;
	info.activate = OnActivate;
	info.deactivate = OnDeactivate;

	obs_register_source(&info);
}

obs_source_t *PluginSource::create(const QString &name) {
	return obs_source_create(id(), name.toUtf8().constData(), nullptr, nullptr);
}

PluginSource *PluginSource::fromSource(obs_source_t *source) {
	return static_cast<PluginSource *>(obs_obj_get_data(source));
}

PluginSource::PluginSource(obs_data_t *settings, obs_source_t *source) : m_source(source) {
	(void)readSettings(settings, m_protocol, m_streamId);
}

PluginSource::~PluginSource() {
	stopReceiver();
	delete m_goirlProcess;
	m_goirlProcess = nullptr;
}

obs_source_t *PluginSource::getSource() const {
	return m_source;
}

uint32_t PluginSource::width() const {
	return m_width;
}

uint32_t PluginSource::height() const {
	return m_height;
}

bool PluginSource::readSettings(obs_data_t *settings, Protocol &protocol, std::string &streamId) {
	const char *protocolString = obs_data_get_string(settings, "Protocol");
	const std::optional protocolOpt = magic_enum::enum_cast<Protocol>(protocolString);
	if (!protocolOpt.has_value()) {
		obs_log(LOG_ERROR, "Invalid protocol %s", protocolString);
		return false;
	}

	protocol = protocolOpt.value();
	streamId = obs_data_get_string(settings, "StreamId");
	return true;
}

void PluginSource::startReceiver() {
	if (m_running) {
		return;
	}

	m_lastProtocol = m_protocol;
	m_running = true;

	if (m_protocol == Protocol::SRTLA) {
		if (!m_goirlProcess) {
			// TODO: make it choose unique port, that will be available to forward
			m_goirlProcess = new GoIRL_Process(5000);

			obs_log(LOG_INFO, "Started go-irl");

			QObject::connect(m_goirlProcess, &GoIRL_Process::serverStarted, [this]() { onGoIRLStarted(); });
			QObject::connect(m_goirlProcess, &GoIRL_Process::serverStopped, [this]() { onGoIRLStopped(); });
			QObject::connect(m_goirlProcess, &GoIRL_Process::serverError,
					 [this](const GoIRL_Process::ServerError error) { onGoIRLError(error); });
		}

		m_goirlProcess->startServer(m_streamId);
		startFrameReceiver();
		return;
	}

	if (!s_mediamtxManager) {
		s_mediamtxManager = new MediamtxManager();

		QObject::connect(s_mediamtxManager, &MediamtxManager::serverStarted, [this]() { onMediamtxStarted(); });
		QObject::connect(s_mediamtxManager, &MediamtxManager::serverStopped, [this]() { onMediamtxStopped(); });
		QObject::connect(s_mediamtxManager, &MediamtxManager::serverError,
				 [this](const MediamtxManager::ServerError error) { onMediamtxError(error); });
		QObject::connect(s_mediamtxManager, &MediamtxManager::inputAdded,
				 [this](const QString &streamId, const QString &publishUrl) {
					 onMediamtxInputAdded(streamId, publishUrl);
				 });
		QObject::connect(s_mediamtxManager, &MediamtxManager::inputRemoved,
				 [this](const QString &streamId) { onMediamtxInputRemoved(streamId); });
		QObject::connect(s_mediamtxManager, &MediamtxManager::inputError,
				 [this](const QString &streamId, const QString &error) {
					 onMediamtxInputError(streamId, error);
				 });

		s_mediamtxManager->startServer();
	}

	s_mediamtxManager->addInput(m_streamId, m_protocol, "192.168.1.40"); // TEMPORARY ADDRESS
	startFrameReceiver();
}

void PluginSource::onMediamtxStarted() {
	obs_log(LOG_INFO, "mediamtx server started");
}

void PluginSource::onMediamtxStopped() {
	obs_log(LOG_INFO, "mediamtx server stopped");
}

void PluginSource::onMediamtxError(const MediamtxManager::ServerError error) {
	obs_log(LOG_ERROR, "mediamtx server error: %d", static_cast<int>(error));
}

void PluginSource::onMediamtxInputAdded(const QString &streamId, const QString &publishUrl) {
	obs_log(LOG_INFO, "mediamtx input '%s' added: %s", qUtf8Printable(streamId), qUtf8Printable(publishUrl));
}

void PluginSource::onMediamtxInputRemoved(const QString &streamId) {
	obs_log(LOG_INFO, "mediamtx input '%s' removed", qUtf8Printable(streamId));
}

void PluginSource::onMediamtxInputError(const QString &streamId, const QString &error) {
	obs_log(LOG_ERROR, "mediamtx input '%s' error: %s", qUtf8Printable(streamId), qUtf8Printable(error));
}

void PluginSource::onGoIRLStarted() {
	obs_log(LOG_INFO, "go-irl SRTLA server started");
}

void PluginSource::onGoIRLStopped() {
	obs_log(LOG_INFO, "go-irl SRTLA server stopped");
}

void PluginSource::onGoIRLError(const GoIRL_Process::ServerError error) {
	obs_log(LOG_ERROR, "go-irl SRTLA server error: %d", static_cast<int>(error));
}

void PluginSource::startFrameReceiver() {
	if (m_frameReceiver) {
		return;
	}

	m_frameReceiver = new SRT_FrameReceiver(SRT_FRAME_RECEIVER_PORT, "test-client");
	m_frameReceiver->connectReceiver(
		[this](const obs_source_frame &frame) { obs_source_output_video(m_source, &frame); },
		[this](const obs_source_audio &audio) { obs_source_output_audio(m_source, &audio); });
}

void PluginSource::stopReceiver() {
	if (!m_running) {
		return;
	}

	m_running = false;
	delete m_frameReceiver;
	m_frameReceiver = nullptr;

	if (m_lastProtocol == Protocol::SRTLA) {
		if (m_goirlProcess) {
			m_goirlProcess->stopServer();
		}
	} else if (s_mediamtxManager) {
		s_mediamtxManager->removeInput(m_streamId);
	}
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

obs_properties_t *PluginSource::OnGetProperties(void *) {
	obs_properties_t *props = obs_properties_create();

	obs_property_t *protocolList = obs_properties_add_list(props, "Protocol", "Select Protocol",
							       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_t *streamId = obs_properties_add_text(props, "StreamId", "Stream ID", OBS_TEXT_DEFAULT);
	obs_property_set_enabled(streamId, false);

	obs_property_list_add_string(protocolList, "SRTLA", "SRTLA");
	obs_property_list_add_string(protocolList, "SRT", "SRT");
	obs_property_list_add_string(protocolList, "WebRTC", "WebRTC");
	obs_property_list_add_string(protocolList, "RTSP", "RTSP");
	obs_property_list_add_string(protocolList, "RTMP", "RTMP");

	return props;
}

void PluginSource::OnGetDefaults(obs_data_t *settings) {
	obs_data_set_default_string(settings, "Protocol", "SRTLA");
	obs_data_set_default_string(settings, "StreamId", "test");
}

void PluginSource::OnUpdate(void *data, obs_data_t *settings) {
	PluginSource *source = static_cast<PluginSource *>(data);
	Protocol protocol;
	std::string streamId;
	if (!readSettings(settings, protocol, streamId)) {
		return;
	}

	if (source->m_protocol == protocol && source->m_streamId == streamId) {
		return;
	}

	if (source->m_activated) {
		source->stopReceiver();
	}

	source->m_protocol = protocol;
	source->m_streamId = std::move(streamId);
	obs_log(LOG_INFO, "Source protocol setting: '%s' (%d)", magic_enum::enum_name(protocol).data(),
		static_cast<int>(protocol));

	if (source->m_activated) {
		source->startReceiver();
	}
}

void PluginSource::OnActivate(void *data) {
	PluginSource *source = static_cast<PluginSource *>(data);
	if (source->m_activated) {
		return;
	}

	source->m_activated = true;
	source->startReceiver();
}

void PluginSource::OnDeactivate(void *data) {
	PluginSource *source = static_cast<PluginSource *>(data);
	if (!source->m_activated) {
		return;
	}

	source->m_activated = false;
	source->stopReceiver();
}
