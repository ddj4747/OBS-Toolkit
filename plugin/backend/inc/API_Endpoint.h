#pragma once

#define CPPHTTPLIB_OPENSSL_SUPPORT

#include <thread>
#include <httplib/httplib.h>

class API_Endpoint final {
public:
	~API_Endpoint() = default;

	static API_Endpoint &getInstance();
	bool start();
	void stop();

	void setAccessKey(const std::string &accessKey);

private:
	void registerEndpoints() const;
	bool authorize(const httplib::Request &req, httplib::Response &res) const;

	std::thread m_httpThread;
	std::unique_ptr<httplib::SSLServer> m_server;
	std::string m_accessKey{};
};
