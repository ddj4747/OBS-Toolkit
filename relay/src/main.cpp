#include <RelayConfig.h>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QRandomGenerator>
#include <QTextStream>
#include <array>

int main(int argc, char **argv) {
	QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("obs-relay"));
	QCommandLineParser parser;
	parser.setApplicationDescription(QStringLiteral("OBS Toolkit relay server"));
	parser.addHelpOption();
	const QCommandLineOption generateApiKey(
		QStringLiteral("generate-api-key"),
		QStringLiteral("Generate a random API key and print it to standard output."));
	parser.addOption(generateApiKey);
	parser.process(app);

	if (parser.isSet(generateApiKey)) {
		std::array<quint32, 8> randomWords;
		QRandomGenerator::system()->fillRange(randomWords.data(), randomWords.size());
		QByteArray key(reinterpret_cast<const char *>(randomWords.data()), sizeof(randomWords));
		QTextStream(stdout) << key.toHex() << Qt::endl;
		return 0;
	}

	RelayConfig config;
	QString error;
	const QString configDirectory =
		qEnvironmentVariable("RELAY_CONFIG_DIR", QCoreApplication::applicationDirPath());
	const QString configPath = QDir(configDirectory).filePath(QStringLiteral("relay-config.json"));
	if (!RelayConfig::load(configPath, config, error)) {
		QTextStream(stderr) << "Failed to load relay configuration: " << error << Qt::endl;
		return 1;
	}
	return 0;
}
