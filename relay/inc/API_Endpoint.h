#pragma once

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib/httplib.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <thread>
#include <QString>

class API_Endpoint final {
public:
	~API_Endpoint();
	API_Endpoint(const API_Endpoint &) = delete;
	API_Endpoint &operator=(const API_Endpoint &) = delete;

	static API_Endpoint &getInstance();
	static bool prepareAccessKey(const QString &configDirectory, bool regenerate, QString &key, QString &error,
				     bool &generated);
	bool start(const std::filesystem::path &configDirectory, uint16_t port);
	void stop();

private:
	API_Endpoint() = default;
	static bool readAccessKey(const QString &path, QString &key, QString &error);
	void registerEndpoints() const;
	bool authorize(const httplib::Request &req, httplib::Response &res) const;

	std::unique_ptr<httplib::SSLServer> m_server;
	std::thread m_httpThread;
	QString m_keyPath;
};
