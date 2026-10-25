#pragma once

#include <cstdint>
#include <mutex>
#include <set>

#include <QFuture>
#include <QTimer>

#include <miniupnpc/miniupnpc.h>

class PortForwarder final : public QObject {
	Q_OBJECT

public:
	enum class Protocol : bool { TCP, UDP };

	PortForwarder() = delete;
	PortForwarder(const PortForwarder &) = delete;
	PortForwarder &operator=(const PortForwarder &) = delete;

	explicit PortForwarder(uint16_t portNumber, Protocol protocol);
	~PortForwarder() override;

	void forward();
	void close();
	bool isForwarded() const;
	uint16_t port() const;
	Protocol protocol() const;
	std::optional<std::string> publicAddress() const;

	static uint16_t getAvailablePort();
	static uint16_t reserveUdpPort(uint16_t preferred = 0);
	static void releaseUdpPort(uint16_t port);

signals:
	void onPortForwardFinished(bool success);

private:
	bool tryForwardPortUPnP();
	bool tryClosePortUPnP() const;
	bool tryRenewPortLeaseUPnP() const;

	static constexpr std::chrono::seconds LEASE_RENEW_INTERVAL = std::chrono::seconds(60);
	static constexpr std::chrono::seconds LEASE_DURATION = std::chrono::seconds(90);

	static std::mutex s_reservedPortsMutex;
	static std::set<uint16_t> s_reservedPorts;
	static bool udpPortFree(uint16_t port);

	std::string m_port;
	std::string m_protocol;
	std::atomic<bool> m_forwarded{};

	QFuture<void> m_forwardFuture;
	QFuture<void> m_leaseRenewFuture;
	QTimer *m_timer;

	// UPnP
	UPNPUrls m_urls{};
	IGDdatas m_data{};
	std::string m_lanAddress;
	std::string m_publicAddress;
};
