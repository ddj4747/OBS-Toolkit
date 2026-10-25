#pragma once

#include <QIcon>
#include <QString>
#include <callback/calldata.h>
#include <functional>
#include <string>
#include <qrcodegen/qrcodegen.hpp>

struct obs_source;

#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

namespace obs_helpers {
QIcon getIconFromSource(const obs_source *source);
QIcon getIconFromPath(const QString &path);
NO_DISCARD uint64_t connectSourceEditSignals(std::function<void(calldata_t *cd)> callback);
void disconnectSourceEditSignals(uint64_t id);
QPixmap getQrFromURL(const QString &url, qrcodegen::QrCode::Ecc errCorrection);
QString makeLarixLink(const QString &name, const QString &url, const QString &streamId, int latencyMs,
		      const QString &scheme = QStringLiteral("larix"));

inline QString fromStdString(const std::string &str) {
	return QString::fromUtf8(str.data(), static_cast<qsizetype>(str.size()));
}

inline std::string toStdString(const QString &str) {
	const QByteArray utf8 = str.toUtf8();
	return {utf8.constData(), static_cast<size_t>(utf8.size())};
}
} // namespace obs_helpers
