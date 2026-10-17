#include <API_Endpoint.h>
#include <obs-helpers.h>
#include <CertificateManager.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <nlohmann/json.hpp>
#include <PluginSource.h>

#include <QCoreApplication>
#include <QThread>
#include <QUuid>

#include <cstring>
#include <functional>

namespace {
constexpr const char *API_SERVER_HOST = "0.0.0.0";
constexpr uint16_t API_SERVER_PORT = 5643;

void runOnUiThread(const std::function<void()> &function) {
	QCoreApplication *app = QCoreApplication::instance();
	if (!app || QThread::currentThread() == app->thread()) {
		function();
		return;
	}

	QMetaObject::invokeMethod(app, function, Qt::BlockingQueuedConnection);
}
} // namespace

API_Endpoint &API_Endpoint::getInstance() {
	static API_Endpoint instance = API_Endpoint();
	return instance;
}

bool API_Endpoint::start() {
	const char *dataPathRaw = obs_get_module_data_path(obs_current_module());
	if (!dataPathRaw) {
		obs_log(LOG_ERROR, "Plugin data folder wasn't found.");
		return false;
	}

	const std::filesystem::path dataPath(dataPathRaw);
	const std::filesystem::path keyPath(dataPath / "key.pem");
	const std::filesystem::path certPath(dataPath / "cert.pem");

	if (!CertificateManager::isCertificateValid(certPath) || !CertificateManager::isPrivateKeyValid(keyPath)) {
		if (!CertificateManager::generateCertificate(keyPath, certPath)) {
			return false;
		}
	}

	const QUuid uuid = QUuid::createUuid();
	m_accessKey = obs_helpers::toStdString(uuid.toString());

	m_server = std::make_unique<httplib::SSLServer>(certPath.string().c_str(), keyPath.string().c_str());
	if (!m_server->is_valid()) {
		m_server.reset();
		return false;
	}

	registerEndpoints();
	if (!m_server->bind_to_port(API_SERVER_HOST, API_SERVER_PORT)) {
		obs_log(LOG_ERROR, "API endpoint failed to bind to %s:%d", API_SERVER_HOST, API_SERVER_PORT);
		m_server.reset();
		return false;
	}

	m_httpThread = std::thread([this] { m_server->listen_after_bind(); });
	return true;
}

void API_Endpoint::stop() {
	if (m_server) {
		m_server->stop();
	}
	if (m_httpThread.joinable()) {
		m_httpThread.join();
	}
}

bool API_Endpoint::authorize(const httplib::Request &req, httplib::Response &res) const {
	if (req.get_header_value("Authorization") != m_accessKey) {
		res.status = 401;
		return false;
	}

	return true;
}

void API_Endpoint::setAccessKey(const std::string &accessKey) {
	m_accessKey = accessKey;
}

void API_Endpoint::registerEndpoints() const {
	m_server->Get("/getBitrates", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}

		const std::vector<std::pair<std::string, uint64_t>> bitrates = PluginSource::getBitrates();

		nlohmann::json responseJson = nlohmann::json::array();
		for (const auto &[name, bitrate] : bitrates) {
			responseJson.push_back(nlohmann::json::object());
			responseJson.back()["sourceName"] = name;
			responseJson.back()["bitrate"] = bitrate;
		}

		res.body = responseJson.dump();
		res.status = 200;
	});

	m_server->Get("/getSceneList", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}

		struct SceneEntry {
			std::string name;
			bool current{false};
		};

		std::vector<SceneEntry> scenes;
		runOnUiThread([&scenes] {
			obs_frontend_source_list sceneList = {};
			obs_frontend_get_scenes(&sceneList);
			obs_source_t *currentScene = obs_frontend_get_current_scene();

			for (size_t i = 0; i < sceneList.sources.num; i++) {
				const obs_source_t *source = sceneList.sources.array[i];
				if (!source) {
					continue;
				}

				const char *name = obs_source_get_name(source);
				scenes.push_back({name ? name : "", source == currentScene});
			}

			obs_source_release(currentScene);
			obs_frontend_source_list_free(&sceneList);
		});

		nlohmann::json responseJson = nlohmann::json::array();
		for (const auto &[name, current] : scenes) {
			responseJson.push_back(nlohmann::json::object());
			responseJson.back()["currentScene"] = current;
			responseJson.back()["sceneName"] = name;
		}

		res.body = responseJson.dump();
		res.status = 200;
	});

	m_server->Post("/setActiveScene", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}
		if (!req.has_param("sceneName")) {
			res.status = 400;
			return;
		}

		const std::string sceneName = req.get_param_value("sceneName");
		obs_source_t *source = obs_get_source_by_name(sceneName.c_str());
		if (!source) {
			res.status = 400;
			return;
		}

		runOnUiThread([source] { obs_frontend_set_current_scene(source); });
		obs_source_release(source);
		res.status = 200;
	});

	m_server->Post("/startStream", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}

		runOnUiThread([] { obs_frontend_streaming_start(); });
		res.status = 200;
	});

	m_server->Post("/stopStream", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}

		runOnUiThread([] { obs_frontend_streaming_stop(); });
		res.status = 200;
	});

	m_server->Get("/streamState", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}

		bool streaming = false;
		runOnUiThread([&streaming] { streaming = obs_frontend_streaming_active(); });

		nlohmann::json responseJson = nlohmann::json::object();
		responseJson["status"] = streaming;
		res.body = responseJson.dump();
		res.status = 200;
	});

	m_server->Get("/getPluginSources", [this](const httplib::Request &req, httplib::Response &res) {
		if (!authorize(req, res)) {
			return;
		}

		struct SourceEntry {
			std::string name;
			obs_source_t *source{nullptr};
		};

		std::vector<SourceEntry> sources;

		obs_enum_sources(
			[](void *data, obs_source_t *source) -> bool {
				auto &sourcesData = *static_cast<std::vector<SourceEntry> *>(data);
				if (obs_source_removed(source)) {
					return true;
				}

				const char *id = obs_source_get_id(source);
				if (!id || strcmp(id, PluginSource::id()) != 0) {
					return true;
				}

				obs_source_t *referenced = obs_source_get_ref(source);
				if (!referenced) {
					return true;
				}

				const char *name = obs_source_get_name(referenced);
				sourcesData.push_back({name ? name : "", referenced});
				return true;
			},
			&sources);

		nlohmann::json responseJson = nlohmann::json::array();
		for (const auto &[name, source] : sources) {
			const PluginSource *pluginSource = PluginSource::fromSource(source);
			responseJson.push_back(nlohmann::json::object());
			responseJson.back()["sourceName"] = name;
			responseJson.back()["streamActive"] = pluginSource && pluginSource->active();
			obs_source_release(source);
		}

		res.body = responseJson.dump();
		res.status = 200;
	});
}
