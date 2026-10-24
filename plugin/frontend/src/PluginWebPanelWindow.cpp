#include <PluginWebPanelWindow.h>
#include <API_Endpoint.h>
#include <PluginFrontend.h>
#include <obs-helpers.h>
#include <plugin-support.h>

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QUuid>

#include <algorithm>

PluginWebPanelWindow::PluginWebPanelWindow(QWidget *parent)
	: QWidget(parent, Qt::Window),
	  m_mainLayout(new QVBoxLayout(this)),
	  m_enabledCheckBox(new QCheckBox(QStringLiteral("Enable web panel"), this)),
	  m_accessKeyEdit(new QLineEdit(this)),
	  m_copyButton(new QPushButton(QStringLiteral("Copy"), this)),
	  m_generateButton(new QPushButton(QStringLiteral("Generate new"), this)),
	  m_qrLabel(new QLabel(this)) {

	setWindowTitle(QStringLiteral("%1 Web Panel").arg(PLUGIN_NAME));
	setMinimumWidth(700);
	setMinimumHeight(600);

	m_mainLayout->setContentsMargins(12, 12, 12, 12);
	m_mainLayout->setSpacing(12);

	m_accessKeyEdit->setReadOnly(true);

	m_qrLabel->setAlignment(Qt::AlignCenter);
	m_qrLabel->setFrameShape(QFrame::StyledPanel);
	m_qrLabel->setFrameShadow(QFrame::Sunken);
	m_qrLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	m_qrLabel->setMinimumSize(240, 240);
	m_qrLabel->installEventFilter(this);

	auto *keyLayout = new QHBoxLayout();
	keyLayout->setSpacing(8);
	keyLayout->addWidget(m_accessKeyEdit, 1);
	keyLayout->addWidget(m_copyButton);
	keyLayout->addWidget(m_generateButton);

	m_mainLayout->addWidget(m_enabledCheckBox);
	m_mainLayout->addLayout(keyLayout);
	m_mainLayout->addWidget(m_qrLabel, 1);
	setLayout(m_mainLayout);

	connect(m_copyButton, &QPushButton::clicked, this, &PluginWebPanelWindow::onCopyClicked);
	connect(m_generateButton, &QPushButton::clicked, this, &PluginWebPanelWindow::onGenerateClicked);
	connect(m_enabledCheckBox, &QCheckBox::toggled, this, &PluginWebPanelWindow::onEnabledChanged);

	loadSettings();
}

PluginWebPanelWindow::~PluginWebPanelWindow() = default;

void PluginWebPanelWindow::showEvent(QShowEvent *event) {
	QWidget::showEvent(event);
	refreshQrCode();
}

bool PluginWebPanelWindow::eventFilter(QObject *watched, QEvent *event) {
	if (watched == m_qrLabel && event->type() == QEvent::Resize) {
		updateImagePreview();
	}

	return QWidget::eventFilter(watched, event);
}

void PluginWebPanelWindow::onCopyClicked() const {
	QGuiApplication::clipboard()->setText(m_accessKeyEdit->text());
}

void PluginWebPanelWindow::onGenerateClicked() {
	m_accessKey = generateAccessKey();
	applyAccessKey();
	saveSettings();
	refreshQrCode();
}

void PluginWebPanelWindow::onEnabledChanged() {
	applyEnabled();
	saveSettings();
}

void PluginWebPanelWindow::loadSettings() {
	obs_data_t *settings = PluginFrontend::get()->getSettingsObject();
	obs_data_set_default_bool(settings, "webPanelEnabled", true);

	const bool enabled = obs_data_get_bool(settings, "webPanelEnabled");
	m_accessKey = QString::fromUtf8(obs_data_get_string(settings, "webPanelAccessKey"));
	const bool generated = m_accessKey.isEmpty();
	if (generated) {
		m_accessKey = generateAccessKey();
	}

	{
		const QSignalBlocker blocker(m_enabledCheckBox);
		m_enabledCheckBox->setChecked(enabled);
	}

	applyAccessKey();
	applyEnabled();
	refreshQrCode();

	if (generated) {
		saveSettings();
	}
}

void PluginWebPanelWindow::saveSettings() const {
	obs_data_t *settings = PluginFrontend::get()->getSettingsObject();
	obs_data_set_bool(settings, "webPanelEnabled", m_enabledCheckBox->isChecked());
	obs_data_set_string(settings, "webPanelAccessKey", m_accessKey.toUtf8().constData());
	PluginFrontend::get()->saveSettingsObject();
}

void PluginWebPanelWindow::applyAccessKey() const {
	API_Endpoint::getInstance().setAccessKey(obs_helpers::toStdString(m_accessKey));
}

void PluginWebPanelWindow::applyEnabled() const {
	API_Endpoint::getInstance().setEnabled(m_enabledCheckBox->isChecked());
}

void PluginWebPanelWindow::applyNewUrl() const {
	const QString ip = API_Endpoint::getInstance().getIp();
	const QString port = QString::number(API_Endpoint::getInstance().getPort());
	m_accessKeyEdit->setText(
		QStringLiteral("https://place-holder.com?accessKey=%1,ip=%2,port=%3").arg(m_accessKey, ip, port));
}

void PluginWebPanelWindow::refreshQrCode() {
	applyNewUrl();
	m_qrCode = obs_helpers::getQrFromURL(m_accessKeyEdit->text(), qrcodegen::QrCode::Ecc::HIGH);
	updateImagePreview();
}

void PluginWebPanelWindow::updateImagePreview() {
	if (m_updatingImagePreview || m_qrCode.isNull()) {
		return;
	}

	const QSize target = m_qrLabel->contentsRect().size();
	if (target.width() <= 0 || target.height() <= 0) {
		return;
	}

	m_updatingImagePreview = true;

	const int modules = m_qrCode.width();
	const int side = std::min(target.width(), target.height());
	const int factor = std::max(1, side / std::max(1, modules));
	m_qrLabel->setText(QString());
	m_qrLabel->setPixmap(
		m_qrCode.scaled(modules * factor, modules * factor, Qt::KeepAspectRatio, Qt::FastTransformation));

	m_updatingImagePreview = false;
}

QString PluginWebPanelWindow::generateAccessKey() {
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
