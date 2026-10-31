#include <RelayConfig.h>
#include <API_Endpoint.h>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

int main(int argc, char **argv) {
	const QCoreApplication app(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("obs-relay"));

	QCommandLineParser parser;
	parser.setApplicationDescription(QStringLiteral("OBS Toolkit relay server"));
	parser.addHelpOption();

	const QCommandLineOption generateApiKey(
		QStringLiteral("generate-api-key"),
		QStringLiteral("Generate and save a new API key, then print it to standard output."));

	parser.addOption(generateApiKey);
	parser.process(app);

	const QString configDirectory =
		qEnvironmentVariable("RELAY_CONFIG_DIR", QCoreApplication::applicationDirPath());
	QString key;
	QString error;
	bool generated = false;

	if (!API_Endpoint::prepareAccessKey(configDirectory, parser.isSet(generateApiKey), key, error, generated)) {
		QTextStream(stderr) << "Failed to prepare relay API key: " << error << Qt::endl;
		return 1;
	}

	if (generated) {
		if (parser.isSet(generateApiKey)) {
			QTextStream(stdout) << key << Qt::endl;
		} else {
			QTextStream(stdout) << "Relay API key: " << key << Qt::endl;
		}
	}

	if (parser.isSet(generateApiKey)) {
		return 0;
	}

	RelayConfig config;

	const QString configPath = QDir(configDirectory).filePath(QStringLiteral("relay-config.json"));
	if (!RelayConfig::load(configPath, config, error)) {
		QTextStream(stderr) << "Failed to load relay configuration: " << error << Qt::endl;
		return 1;
	}

	API_Endpoint &api = API_Endpoint::getInstance();
#ifdef _WIN32
	const std::filesystem::path apiDirectory(configDirectory.toStdWString());
#else
	const std::filesystem::path apiDirectory(configDirectory.toStdString());
#endif

	if (!api.start(apiDirectory, config.apiPort)) {
		QTextStream(stderr) << "Failed to start relay API server" << Qt::endl;
		return 1;
	}

	const int result = QCoreApplication::exec();
	api.stop();

	return result;
}
