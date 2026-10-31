#include <MediaMtxConfig.h>
#include <QSaveFile>

bool MediaMtxConfig::write(const QString &path, const RelayConfig &config, QString &error) {
	const QString contents =
		QStringLiteral("api: yes\napiAddress: 127.0.0.1:%1\n"
			       "srt: yes\nsrtAddress: 0.0.0.0:%2\n"
			       "rtsp: yes\nrtspAddress: :%3\nrtpAddress: :%4\nrtcpAddress: :%5\n"
			       "rtmp: yes\nrtmpAddress: :%6\nwebrtc: yes\nwebrtcAddress: :%7\n"
			       "moq: yes\nmoqHTTP2Address: :%8\nmoqHTTP3Address: :%8\nmoqQUICAddress: :%9\n")
			.arg(apiPort)
			.arg(config.srtPort)
			.arg(config.rtspPort)
			.arg(config.rtpPort)
			.arg(config.rtcpPort)
			.arg(config.rtmpPort)
			.arg(config.webRtcPort)
			.arg(config.moqPort)
			.arg(config.moqQuicPort);

	QSaveFile file(path);

	if (!file.open(QIODevice::WriteOnly)) {
		error = file.errorString();
		return false;
	}

	const QByteArray data = contents.toUtf8();

	if (file.write(data) != data.size() || !file.commit()) {
		error = file.errorString();
		return false;
	}

	error.clear();

	return true;
}
