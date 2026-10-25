#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QResizeEvent;
class QShowEvent;

class AppQrRow : public QWidget {
public:
	explicit AppQrRow(QWidget *parent = nullptr);

	void setLinks(const QString &larixLink, const QString &moblinLink, const QString &irlProLink);

	QSize minimumSizeHint() const override;
	QSize sizeHint() const override;

protected:
	void resizeEvent(QResizeEvent *event) override;
	void showEvent(QShowEvent *event) override;

private:
	void applySquareSize();
	void updatePixmap(int index);

	static constexpr int c_count = 3;
	static constexpr int c_maxSide = 280;
	static constexpr int c_titleHeight = 28;
	static constexpr int c_spacing = 16;

	QLabel *m_previews[c_count]{};
	QString m_links[c_count];
	int m_side = -1;
};
