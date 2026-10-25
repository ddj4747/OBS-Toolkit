#include <AppQrRow.h>

#include <obs-helpers.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <algorithm>

AppQrRow::AppQrRow(QWidget *parent) : QWidget(parent) {
	setObjectName(QStringLiteral("pluginSourceQrRow"));
	setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

	auto *rowLayout = new QHBoxLayout(this);
	rowLayout->setContentsMargins(0, 0, 0, 0);
	rowLayout->setSpacing(c_spacing);
	rowLayout->setSizeConstraint(QLayout::SetNoConstraint);

	const char *titles[] = {"Larix", "Moblin", "IRL Pro"};
	const char *names[] = {"pluginSourceQrLarix", "pluginSourceQrMoblin", "pluginSourceQrIrlPro"};
	for (int index = 0; index < c_count; ++index) {
		auto *cell = new QWidget(this);
		cell->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		auto *cellLayout = new QVBoxLayout(cell);
		cellLayout->setContentsMargins(0, 0, 0, 0);
		cellLayout->setSpacing(6);

		auto *title = new QLabel(QString::fromUtf8(titles[index]), cell);
		title->setAlignment(Qt::AlignCenter);
		m_previews[index] = new QLabel(cell);
		m_previews[index]->setObjectName(QLatin1String(names[index]));
		m_previews[index]->setAlignment(Qt::AlignCenter);
		m_previews[index]->setScaledContents(false);
		m_previews[index]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		cellLayout->addWidget(title);
		cellLayout->addWidget(m_previews[index], 0, Qt::AlignHCenter);
		rowLayout->addWidget(cell, 1);
	}
}

void AppQrRow::setLinks(const QString &larixLink, const QString &moblinLink, const QString &irlProLink) {
	m_links[0] = larixLink;
	m_links[1] = moblinLink;
	m_links[2] = irlProLink;
	for (int index = 0; index < c_count; ++index) {
		updatePixmap(index);
	}
}

QSize AppQrRow::minimumSizeHint() const {
	return {0, m_side > 0 ? m_side + c_titleHeight : c_titleHeight};
}

QSize AppQrRow::sizeHint() const {
	return {c_count * c_maxSide, m_side > 0 ? m_side + c_titleHeight : c_titleHeight};
}

void AppQrRow::resizeEvent(QResizeEvent *event) {
	QWidget::resizeEvent(event);
	applySquareSize();
}

void AppQrRow::showEvent(QShowEvent *event) {
	QWidget::showEvent(event);
	applySquareSize();
}

void AppQrRow::applySquareSize() {
	auto *rowLayout = static_cast<QHBoxLayout *>(layout());
	const int inner = std::max(0, contentsRect().width() - rowLayout->spacing() * (c_count - 1));
	const int side = std::clamp(inner / c_count, 1, c_maxSide);
	if (side == m_side) {
		return;
	}

	m_side = side;
	for (int index = 0; index < c_count; ++index) {
		m_previews[index]->setFixedSize(side, side);
		updatePixmap(index);
	}

	const int height = side + c_titleHeight;
	setMinimumHeight(height);
	setMaximumHeight(height);
}

void AppQrRow::updatePixmap(const int index) {
	QLabel *preview = m_previews[index];
	const QString &link = m_links[index];
	if (m_side <= 0) {
		return;
	}
	if (link.isEmpty()) {
		preview->setPixmap(QPixmap());
		preview->setText(QStringLiteral("No link yet"));
		return;
	}

	const QPixmap qr = obs_helpers::getQrFromURL(link, qrcodegen::QrCode::Ecc::LOW);
	if (qr.isNull()) {
		preview->setPixmap(QPixmap());
		preview->setText(QStringLiteral("Could not create QR code"));
		return;
	}

	const int modules = std::max(1, qr.width());
	const int factor = std::max(1, m_side / modules);
	const int pixels = modules * factor;
	preview->setText(QString());
	preview->setPixmap(qr.scaled(pixels, pixels, Qt::KeepAspectRatio, Qt::FastTransformation));
}
