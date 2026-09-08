#include <PluginSource.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QThread>
#include <magic_enum/magic_enum.hpp>

namespace {
constexpr uint16_t SRT_FRAME_RECEIVER_PORT = 8890;
} // namespace

MediamtxManager *PluginSource::s_mediamtxManager = nullptr;
std::mutex PluginSource::s_instancesMutex;
std::set<PluginSource *> PluginSource::s_instances;
std::atomic<bool> PluginSource::s_shutdownPrepared = false;

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

void PluginSource::prepareForShutdown() {
	std::lock_guard<std::mutex> lock(s_instancesMutex);
	s_shutdownPrepared.store(true);

	for (PluginSource *source : s_instances) {
		source->prepareInstanceForShutdown();
	}

	if (!s_mediamtxManager) {
		return;
	}

	delete s_mediamtxManager;
	s_mediamtxManager = nullptr;
}

PluginSource::PluginSource(obs_data_t *settings, obs_source_t *source) : m_source(source) {
	for (const AVCodecID codecId : c_videoCodecs) {
		m_decoderInfos.emplace(codecId, findDecoders(codecId));
	}

	for (const AVCodecID codecId : c_audioCodecs) {
		m_decoderInfos.emplace(codecId, findDecoders(codecId));
	}

	(void)readSettings(settings, this);

	std::lock_guard<std::mutex> lock(s_instancesMutex);
	s_instances.insert(this);
}

PluginSource::~PluginSource() {
	stopReceiver();
	delete m_goirlProcess;
	m_goirlProcess = nullptr;

	std::lock_guard<std::mutex> lock(s_instancesMutex);
	s_instances.erase(this);
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

bool PluginSource::readSettings(obs_data_t *settings, PluginSource *source) {
	const char *protocolString = obs_data_get_string(settings, "Protocol");
	const std::optional protocolOpt = magic_enum::enum_cast<Protocol>(protocolString);
	if (!protocolOpt.has_value()) {
		obs_log(LOG_ERROR, "Invalid protocol %s", protocolString);
		return false;
	}

	std::map<AVCodecID, const AVCodec *> selectedCodecs;
	for (const auto &[codecId, decoders] : source->m_decoderInfos) {
		if (decoders.empty()) {
			continue;
		}

		const char *selectedName = obs_data_get_string(settings, avcodec_get_name(codecId));
		const DecoderInfo *selectedDecoder = nullptr;
		for (const DecoderInfo &decoder : decoders) {
			if (decoder.name == selectedName) {
				selectedDecoder = &decoder;
				break;
			}
		}

		if (!selectedDecoder) {
			if (selectedName[0] != '\0') {
				obs_log(LOG_WARNING, "Decoder '%s' for codec '%s' is unavailable; using '%s'",
					selectedName, avcodec_get_name(codecId), decoders.front().name.c_str());
			}
			selectedDecoder = &decoders.front();
		}

		selectedCodecs.emplace(codecId, selectedDecoder->codec);
	}

	source->m_protocol = protocolOpt.value();
	source->m_streamId = obs_data_get_string(settings, "StreamId");
	source->m_selectedCodecs = std::move(selectedCodecs);
	return true;
}

void PluginSource::startReceiver() {
	if (m_running) {
		return;
	}

	const uint16_t availablePort = PortForwarder::getAvailablePort();
	m_portForwarder = new PortForwarder(availablePort, PortForwarder::Protocol::UDP);
	m_lastProtocol = m_protocol;
	m_running = true;

	QObject::connect(
		m_portForwarder, &PortForwarder::onPortForwardFinished, [this, availablePort](const bool success) {
			if (!success) {
				stopReceiver();
				return;
			}

			if (!m_portForwarder) {
				stopReceiver();
				return;
			}

			const std::optional<std::string> publicAddressOpt = m_portForwarder->publicAddress();
			if (!publicAddressOpt.has_value()) {
				stopReceiver();
				return;
			}

			const std::string &publicAddress = publicAddressOpt.value();

			if (m_protocol == Protocol::SRTLA) {
				if (!m_goirlProcess) {
					m_goirlProcess = new GoIRL_Process(availablePort);

					obs_log(LOG_INFO, "Started go-irl");

					QObject::connect(m_goirlProcess, &GoIRL_Process::serverStarted,
							 [this]() { onGoIRLStarted(); });
					QObject::connect(m_goirlProcess, &GoIRL_Process::serverStopped,
							 [this]() { onGoIRLStopped(); });
					QObject::connect(m_goirlProcess, &GoIRL_Process::serverError,
							 [this](const GoIRL_Process::ServerError error) {
								 onGoIRLError(error);
							 });
				}

				m_goirlProcess->startServer(m_streamId);
				m_streamUrl = m_goirlProcess->getStreamUrl(publicAddress);

				updateProperties();
				startFrameReceiver();
				return;
			}

			if (!s_mediamtxManager) {
				s_mediamtxManager = new MediamtxManager();

				QObject::connect(s_mediamtxManager, &MediamtxManager::serverStarted,
						 [this]() { onMediamtxStarted(); });
				QObject::connect(s_mediamtxManager, &MediamtxManager::serverStopped,
						 [this]() { onMediamtxStopped(); });
				QObject::connect(s_mediamtxManager, &MediamtxManager::serverError,
						 [this](const MediamtxManager::ServerError error) {
							 onMediamtxError(error);
						 });
				QObject::connect(s_mediamtxManager, &MediamtxManager::inputAdded,
						 [this](const QString &streamId, const QString &publishUrl) {
							 m_streamUrl = publishUrl.toStdString();
							 onMediamtxInputAdded(streamId, publishUrl);

							 updateProperties();
						 });
				QObject::connect(s_mediamtxManager, &MediamtxManager::inputRemoved,
						 [this](const QString &streamId) { onMediamtxInputRemoved(streamId); });
				QObject::connect(s_mediamtxManager, &MediamtxManager::inputError,
						 [this](const QString &streamId, const QString &error) {
							 onMediamtxInputError(streamId, error);
						 });

				s_mediamtxManager->startServer();
			}

			s_mediamtxManager->addInput(m_streamId, m_protocol, publicAddress);
			startFrameReceiver();
		});

	m_portForwarder->moveToThread(QCoreApplication::instance()->thread());
	PortForwarder *portForwarder = m_portForwarder;
	QMetaObject::invokeMethod(portForwarder, [portForwarder]() { portForwarder->forward(); }, Qt::QueuedConnection);
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

void PluginSource::updateProperties() const {
	obs_data_t *settings = obs_source_get_settings(m_source);
	obs_data_set_string(settings, "StreamUrl", m_streamUrl.c_str());
	obs_data_set_string(settings, "StreamId", m_streamId.c_str());
	obs_data_set_string(settings, "Protocol", magic_enum::enum_name(m_protocol).data());

	obs_source_update(m_source, settings);
	obs_data_release(settings);
	obs_source_update_properties(m_source);
}

void PluginSource::startFrameReceiver() {
	if (m_frameReceiver) {
		return;
	}

	m_frameReceiver = new SRT_FrameReceiver(SRT_FRAME_RECEIVER_PORT, "test-client", m_selectedCodecs);
	m_frameReceiver->connectReceiver(
		[this](const obs_source_frame &frame) {
			m_width = frame.width;
			m_height = frame.height;
			obs_source_output_video(m_source, &frame);
		},
		[this](const obs_source_audio &audio) { obs_source_output_audio(m_source, &audio); });
}

void PluginSource::stopReceiver() {
	if (!m_running) {
		return;
	}

	m_running = false;
	if (m_frameReceiver) {
		delete m_frameReceiver;
		m_frameReceiver = nullptr;
	}

	if (m_portForwarder) {
		(void)m_portForwarder->disconnect();
		if (s_shutdownPrepared.load() && m_portForwarder->thread() == QThread::currentThread()) {
			delete m_portForwarder;
		} else {
			m_portForwarder->deleteLater();
		}
		m_portForwarder = nullptr;
	}

	if (m_lastProtocol == Protocol::SRTLA) {
		if (m_goirlProcess) {
			m_goirlProcess->stopServer();
		}
	} else if (s_mediamtxManager) {
		s_mediamtxManager->removeInput(m_streamId);
	}
}

void PluginSource::prepareInstanceForShutdown() {
	stopReceiver();

	delete m_goirlProcess;
	m_goirlProcess = nullptr;
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

obs_properties_t *PluginSource::OnGetProperties(void *data) {
	PluginSource *source = static_cast<PluginSource *>(data);
	obs_properties_t *props = obs_properties_create();

	obs_property_t *protocolList = obs_properties_add_list(props, "Protocol", "Select Protocol",
							       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_t *streamId = obs_properties_add_text(props, "StreamId", "Stream ID", OBS_TEXT_DEFAULT);
	obs_property_set_enabled(streamId, false);

	obs_property_t *streamUrl = obs_properties_add_text(props, "StreamUrl", "Stream URL", OBS_TEXT_DEFAULT);
	obs_property_set_enabled(streamUrl, false);

	obs_property_list_add_string(protocolList, "SRTLA", "SRTLA");
	obs_property_list_add_string(protocolList, "SRT", "SRT");
	obs_property_list_add_string(protocolList, "WebRTC", "WebRTC");
	obs_property_list_add_string(protocolList, "RTSP", "RTSP");
	obs_property_list_add_string(protocolList, "RTMP", "RTMP");

	obs_properties_t *advanced = obs_properties_create();

	obs_properties_t *videoCodecs = obs_properties_create();
	for (const AVCodecID codecId : c_videoCodecs) {
		const auto decoderIt = source->m_decoderInfos.find(codecId);
		if (decoderIt == source->m_decoderInfos.end()) {
			continue;
		}

		const char *codecName = avcodec_get_name(codecId);
		obs_property_t *videoCodecList = obs_properties_add_list(videoCodecs, codecName, codecName,
									 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

		for (const DecoderInfo &decoder : decoderIt->second) {
			obs_property_list_add_string(videoCodecList, decoder.name.c_str(), decoder.name.c_str());
		}
	}
	obs_properties_add_group(advanced, "VideoCodecs", "Video Codecs", OBS_GROUP_NORMAL, videoCodecs);

	obs_properties_t *audioCodecs = obs_properties_create();
	for (const AVCodecID codecId : c_audioCodecs) {
		const auto decoderIt = source->m_decoderInfos.find(codecId);
		if (decoderIt == source->m_decoderInfos.end()) {
			continue;
		}

		const char *codecName = avcodec_get_name(codecId);
		obs_property_t *audioCodecList = obs_properties_add_list(audioCodecs, codecName, codecName,
									 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

		for (const DecoderInfo &decoder : decoderIt->second) {
			obs_property_list_add_string(audioCodecList, decoder.name.c_str(), decoder.name.c_str());
		}
	}
	obs_properties_add_group(advanced, "AudioCodecs", "Audio Codecs", OBS_GROUP_NORMAL, audioCodecs);

	obs_properties_add_group(props, "AdvancedOptions", "Advanced Options", OBS_GROUP_NORMAL, advanced);

	return props;
}

void PluginSource::OnGetDefaults(obs_data_t *settings) {
	obs_data_set_default_string(settings, "Protocol", "SRTLA");
	obs_data_set_default_string(settings, "StreamId", "test");
	obs_data_set_default_string(settings, "StreamUrl", "");
}

void PluginSource::OnUpdate(void *data, obs_data_t *settings) {
	PluginSource *source = static_cast<PluginSource *>(data);
	const Protocol previousProtocol = source->m_protocol;
	const std::string previousStreamId = source->m_streamId;
	const auto previousCodecs = source->m_selectedCodecs;

	if (!readSettings(settings, source)) {
		return;
	}

	if (source->m_protocol == previousProtocol && source->m_streamId == previousStreamId &&
	    source->m_selectedCodecs == previousCodecs) {
		return;
	}

	if (source->m_activated) {
		source->stopReceiver();
	}

	obs_log(LOG_INFO, "Source protocol setting: '%s' (%d)", magic_enum::enum_name(source->m_protocol).data(),
		static_cast<int>(source->m_protocol));

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
