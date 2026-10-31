#include <MediaMtxConfig.h>
#include <QSaveFile>

bool MediaMtxConfig::write(const QString &path, QString &error) {
	const QString contents =
		QStringLiteral("api: yes\napiAddress: 127.0.0.1:%1\n"
			       "srt: yes\nsrtAddress: 0.0.0.0:%2\n"
			       "rtsp: yes\nrtspAddress: :%3\nrtpAddress: :%4\nrtcpAddress: :%5\n"
			       "rtmp: yes\nrtmpAddress: :%6\nwebrtc: yes\nwebrtcAddress: :%7\n"
			       "moq: yes\nmoqHTTP2Address: :%8\nmoqHTTP3Address: :%8\nmoqQUICAddress: :%9\n")
			.arg(apiPort)
			.arg(RELAY_SRT_PORT)
			.arg(RELAY_RTSP_PORT)
			.arg(RELAY_RTP_PORT)
			.arg(RELAY_RTCP_PORT)
			.arg(RELAY_RTMP_PORT)
			.arg(RELAY_WEBRTC_PORT)
			.arg(RELAY_MOQ_PORT)
			.arg(RELAY_MOQ_QUIC_PORT);

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
