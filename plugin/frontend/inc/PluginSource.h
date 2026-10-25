#pragma once

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QString>
#include <plugin-support.h>
#include <MediamtxManager.h>
#include <SRT_FrameReceiver.h>
#include <StreamHandler.h>
#include <PortForwardUtils.h>
#include <atomic>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class QObject;

class PluginSource {
public:
	static void registerType();
	static obs_source_t *create(const QString &name);
	static PluginSource *fromSource(obs_source_t *source);
	static const char *id();
	static void prepareForShutdown();
	static std::vector<std::pair<std::string, uint64_t>> getBitrates();
	static std::vector<std::string> sourceNames();
	static std::optional<uint64_t> bitrateForName(const std::string &name);

	NO_DISCARD obs_source_t *getSource() const;
	NO_DISCARD uint64_t getBitrate() const;
	NO_DISCARD bool active() const;

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
	static void OnGetDefaults(obs_data_t *settings);
	static void OnUpdate(void *data, obs_data_t *settings);
	static void OnActivate(void *data);
	static void OnDeactivate(void *data);

	NO_DISCARD uint32_t width() const;
	NO_DISCARD uint32_t height() const;

	static bool readSettings(obs_data_t *settings, PluginSource *source);
	static std::string generateStreamId();

	void startReceiver();
	void stopReceiver();
	void startFrameReceiver();
	void stopFrameReceiver();
	void destroyPortForwarder();
	void destroyStreamHandler();
	void onPortForwardFinished(const PortForwarder *portForwarder, bool success);
	void startStreamHandler();
	void prepareInstanceForShutdown();

	void onStreamReady(const QString &streamId, const QString &publishUrl);
	void onStreamAvailable(const QString &streamId);
	void onStreamUnavailable(const QString &streamId);
	void onStreamStopped(const QString &streamId);
	void onStreamError(const QString &streamId, const QString &error);
	void updateProperties() const;
	NO_DISCARD QString larixLink() const;
	NO_DISCARD QString irlProLink() const;
	NO_DISCARD QString moblinLink() const;
	NO_DISCARD QString connectionLink(const QString &scheme) const;

	struct ForwardingConfig {
		uint16_t port;
		PortForwarder::Protocol protocol;
	};

	static constexpr uint16_t c_srtPort = 8890;
	static constexpr uint16_t c_srtlaPort = 5000;
	static constexpr uint16_t c_rtmpPort = 1935;
	static constexpr uint16_t c_rtspPort = 8554;
	static constexpr uint16_t c_rtspRtpPort = 8000;
	static constexpr uint16_t c_rtspRtcpPort = 8001;
	static constexpr uint16_t c_webrtcHttpPort = 8889;
	static constexpr uint16_t c_webrtcIcePort = 8189;
	static constexpr uint16_t c_moqHttpPort = 8892;
	static constexpr uint16_t c_moqQuicPort = 8893;
	static constexpr int c_larixSrtLatencyMs = 2000;
	static constexpr const char *c_qrNamePrefix = "pluginSourceQr";

	NO_DISCARD std::vector<ForwardingConfig> forwardingConfigs() const;
	static void deleteOnOwningThread(QObject *object);
	void copySettingToClipboard(const char *key) const;
	static bool onCopyStreamIdClicked(obs_properties_t *props, obs_property_t *property, void *data);
	static bool onCopyStreamUrlClicked(obs_properties_t *props, obs_property_t *property, void *data);
	static void attachAppQrCodes(const QString &larixLink, const QString &moblinLink, const QString &irlProLink);
	void scheduleAppQrCodes() const;

	obs_source_t *m_source = nullptr;
	uint32_t m_width{1280};
	uint32_t m_height{720};
	bool m_activated{false};
	bool m_running{false};
	std::string m_streamId;
	std::string m_streamUrl;
	StreamProtocol m_protocol{};

	std::vector<PortForwarder *> m_portForwarders;
	SRT_FrameReceiver *m_frameReceiver{nullptr};
	mutable std::mutex m_frameReceiverMutex;
	StreamHandler *m_streamHandler{nullptr};
	uint8_t m_pendingPortForwards{0};

	std::map<AVCodecID, std::vector<DecoderInfo>> m_decoderInfos;
	std::map<AVCodecID, const AVCodec *> m_selectedCodecs;

	static std::mutex s_instancesMutex;
	static std::set<PluginSource *> s_instances;
	static std::atomic<bool> s_shutdownPrepared;
};
