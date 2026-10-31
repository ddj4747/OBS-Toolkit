#include <RelayConfig.h>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <cmath>

const std::array<RelayConfig::PortSetting, 10> &RelayConfig::portSettings() {
	static const std::array<PortSetting, 10> ports{{
		{"API_Port", &RelayConfig::apiPort},
		{"SRTLA_Port", &RelayConfig::srtlaPort},
		{"SRT_Port", &RelayConfig::srtPort},
		{"RTSP_Port", &RelayConfig::rtspPort},
		{"RTP_Port", &RelayConfig::rtpPort},
		{"RTCP_Port", &RelayConfig::rtcpPort},
		{"RTMP_Port", &RelayConfig::rtmpPort},
		{"WebRTC_Port", &RelayConfig::webRtcPort},
		{"MoQ_Port", &RelayConfig::moqPort},
		{"MoQ_QUIC_Port", &RelayConfig::moqQuicPort},
	}};

	return ports;
}

bool RelayConfig::load(const QString &path, RelayConfig &config, QString &error) {
	QFile file(path);

	if (!file.exists()) {
		const RelayConfig defaults;
		QJsonObject object;

		for (const PortSetting &port : portSettings())
			object.insert(QLatin1String(port.key), defaults.*(port.member));

		if (file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
			const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);

			if (file.write(data) != data.size() || !file.flush()) {
				error = file.errorString();
				file.close();
				file.remove();
				return false;
			}

			file.close();
		} else if (!file.exists()) {
			error = file.errorString();
			return false;
		}
	}

	if (!file.open(QIODevice::ReadOnly)) {
		error = file.errorString();
		return false;
	}

	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);

	if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
		error = parseError.error == QJsonParseError::NoError ? QStringLiteral("Expected a JSON object")
								     : parseError.errorString();
		return false;
	}

	RelayConfig result;
	const QJsonObject object = document.object();

	for (const PortSetting &port : portSettings()) {
		const QJsonValue value = object.value(QLatin1String(port.key));

		if (value.isUndefined())
			continue;

		const double number = value.toDouble(-1);

		if (!value.isDouble() || number < 1 || number > 65535 || std::floor(number) != number) {
			error = QStringLiteral("Invalid port: ") + QLatin1String(port.key);
			return false;
		}

		result.*(port.member) = static_cast<uint16_t>(number);
	}

	config = result;
	error.clear();

	return true;
}
