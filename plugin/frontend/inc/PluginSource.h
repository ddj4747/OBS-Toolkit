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
#include <set>
#include <string>

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

class PluginSource {
public:
	static void registerType();
	static obs_source_t *create(const QString &name);
	static PluginSource *fromSource(obs_source_t *source);
	static const char *id();
	static void prepareForShutdown();

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
	static void OnGetDefaults(obs_data_t *settings);
	static void OnUpdate(void *data, obs_data_t *settings);
	static void OnActivate(void *data);
	static void OnDeactivate(void *data);

	NO_DISCARD uint32_t width() const;
	NO_DISCARD uint32_t height() const;

	static bool readSettings(obs_data_t *settings, PluginSource *source);

	void startReceiver();
	void stopReceiver();
	void startFrameReceiver();
	void stopFrameReceiver();
	void destroyPortForwarder();
	void destroyStreamHandler();
	void onPortForwardFinished(PortForwarder *portForwarder, bool success);
	void startStreamHandler();
	void prepareInstanceForShutdown();

	void onStreamReady(const QString &streamId, const QString &publishUrl);
	void onStreamAvailable(const QString &streamId);
	void onStreamUnavailable(const QString &streamId);
	void onStreamStopped(const QString &streamId);
	void onStreamError(const QString &streamId, const QString &error);
	void updateProperties() const;

	obs_source_t *m_source = nullptr;
	uint32_t m_width{1280};
	uint32_t m_height{720};
	bool m_activated{false};
	bool m_running{false};
	std::string m_streamId;
	std::string m_streamUrl;
	Protocol m_protocol{};

	PortForwarder *m_portForwarder{nullptr};
	PortForwarder *m_secondaryPortForwarder{nullptr};
	SRT_FrameReceiver *m_frameReceiver{nullptr};
	StreamHandler *m_streamHandler{nullptr};
	uint8_t m_pendingPortForwards{0};

	std::map<AVCodecID, std::vector<DecoderInfo>> m_decoderInfos;
	std::map<AVCodecID, const AVCodec *> m_selectedCodecs;

	static std::mutex s_instancesMutex;
	static std::set<PluginSource *> s_instances;
	static std::atomic<bool> s_shutdownPrepared;
};
