#pragma once

#define CPPHTTPLIB_OPENSSL_SUPPORT

#include <mutex>
#include <thread>
#include <httplib/httplib.h>
#include <PortForwardUtils.h>

class API_Endpoint final {
public:
	~API_Endpoint() = default;

	static API_Endpoint &getInstance();
	bool start();
	void stop();

	void setAccessKey(const std::string &accessKey);
	void setEnabled(bool enabled);
	uint16_t getPort();
	QString getIp() const;

private slots:

private:
	void registerEndpoints() const;
	bool authorize(const httplib::Request &req, httplib::Response &res) const;

	std::thread m_httpThread;
	std::unique_ptr<httplib::SSLServer> m_server;
	mutable std::mutex m_authMutex;
	std::string m_accessKey{};
	std::unique_ptr<PortForwarder> m_portForwarder;
	bool m_enabled{true};
};
