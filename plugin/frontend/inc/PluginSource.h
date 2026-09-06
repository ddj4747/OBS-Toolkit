#pragma once

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QString>
#include <plugin-support.h>
#include <MediamtxManager.h>
#include <SRT_FrameReceiver.h>
#include <GoIRL_Process.h>
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

	static bool readSettings(obs_data_t *settings, Protocol &protocol, std::string &streamId);
	void startReceiver();
	void stopReceiver();
	void startFrameReceiver();

	void onMediamtxStarted();
	void onMediamtxStopped();
	void onMediamtxError(MediamtxManager::ServerError error);
	void onMediamtxInputAdded(const QString &streamId, const QString &publishUrl);
	void onMediamtxInputRemoved(const QString &streamId);
	void onMediamtxInputError(const QString &streamId, const QString &error);

	void onGoIRLStarted();
	void onGoIRLStopped();
	void onGoIRLError(GoIRL_Process::ServerError error);

	obs_source_t *m_source = nullptr;
	uint32_t m_width{1280};
	uint32_t m_height{720};
	bool m_activated{false};
	bool m_running{false};
	std::string m_streamId;
	std::string m_streamEndpoint;
	Protocol m_protocol{};
	Protocol m_lastProtocol{};

	SRT_FrameReceiver *m_frameReceiver{nullptr};
	GoIRL_Process *m_goirlProcess{nullptr};
	static MediamtxManager *s_mediamtxManager;
};
