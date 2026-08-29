#include <PortForwardUtils.h>

#include <QtConcurrent/QtConcurrent>
#include <iostream>
#include <plugin-support.h>

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

PortForwarder::PortForwarder(const uint16_t portNumber, const Protocol protocol)
	: m_port(std::to_string(portNumber)),
	  m_protocol(protocol == Protocol::TCP ? "TCP" : "UDP"),
	  m_timer(new QTimer(this)) {}

PortForwarder::~PortForwarder() {
	m_timer->stop();
	m_forwardFuture.waitForFinished();
	m_leaseRenewFuture.waitForFinished();

	if (!m_forwarded.load()) {
		return;
	}

	(void)tryClosePortUPnP();
	FreeUPNPUrls(&m_urls);
}

void PortForwarder::forward() {
	m_forwardFuture = QtConcurrent::run([this]() {
		if (m_forwarded.exchange(true)) {
			return;
		}

		if (!tryForwardPortUPnP()) {
			QMetaObject::invokeMethod(qApp, [this]() { onPortForwardFinished(false); });
			m_forwarded.store(false);
			return;
		}

		QMetaObject::invokeMethod(
			this,
			[this]() {
				m_timer->setInterval(LEASE_RENEW_INTERVAL);
				connect(
					m_timer, &QTimer::timeout, this,
					[this]() {
						if (!m_leaseRenewFuture.isFinished()) {
							return;
						}
						m_leaseRenewFuture =
							QtConcurrent::run([this]() { (void)tryRenewPortLeaseUPnP(); });
					},
					Qt::UniqueConnection);
				m_timer->start();

				emit onPortForwardFinished(true);
			},
			Qt::QueuedConnection);
	});
}

bool PortForwarder::tryForwardPortUPnP() {
	int error = 0;

	UPNPDev *devlist = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &error);
	if (!devlist) {
		obs_log(LOG_INFO, "No UPnP device found or discovery error: %i", error);
		return false;
	}

	char lanAddress[64] = {0};
	const int status = UPNP_GetValidIGD(devlist, &m_urls, &m_data, lanAddress, sizeof(lanAddress));
	if (status != 1) {
		obs_log(LOG_INFO, "No valid Internet Gateway Device found.");
		if (status != 0) {
			FreeUPNPUrls(&m_urls);
		}
		freeUPNPDevlist(devlist);
		return false;
	}

	m_lanAddress = lanAddress;

	static const std::string name = std::format("OBS_PLUGIN_{}_{}", PLUGIN_NAME, PLUGIN_VERSION);
	static const std::string leaseDuration = std::to_string(LEASE_DURATION.count());

	const int result = UPNP_AddPortMapping(m_urls.controlURL, m_data.first.servicetype, m_port.c_str(),
					       m_port.c_str(), m_lanAddress.c_str(), name.c_str(), m_protocol.c_str(),
					       nullptr, leaseDuration.c_str());

	freeUPNPDevlist(devlist);
	if (result != UPNPCOMMAND_SUCCESS) {
		FreeUPNPUrls(&m_urls);
		return false;
	}

	return true;
}

bool PortForwarder::tryClosePortUPnP() const {
	if (!m_forwarded.load()) {
		return true;
	}

	const int result = UPNP_DeletePortMapping(m_urls.controlURL, m_data.first.servicetype, m_port.c_str(),
						  m_protocol.c_str(), nullptr);

	return (result == UPNPCOMMAND_SUCCESS);
}

bool PortForwarder::tryRenewPortLeaseUPnP() const {
	static const std::string name = std::format("OBS_PLUGIN_{}_{}", PLUGIN_NAME, PLUGIN_VERSION);
	static const std::string leaseDuration = std::to_string(LEASE_DURATION.count());

	const int result = UPNP_AddPortMapping(m_urls.controlURL, m_data.first.servicetype, m_port.c_str(),
					       m_port.c_str(), m_lanAddress.c_str(), name.c_str(), m_protocol.c_str(),
					       nullptr, leaseDuration.c_str());

	return (result == UPNPCOMMAND_SUCCESS);
}
