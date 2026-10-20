#include "eyedropper.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRasterWindow>
#include <QScreen>

// One frozen screen. The screenshot is taken before the window shows, so it's what you saw.
class EyedropperWindow : public QRasterWindow {
public:
	EyedropperWindow(Eyedropper *owner, QScreen *screen) : m_owner(owner)
	{
		m_shot = screen->grabWindow(0).toImage(); // physical pixels
		setScreen(screen);
		setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
		setGeometry(screen->geometry());
		setCursor(Qt::CrossCursor);
	}

protected:
	QColor colorAt(QPointF p) const
	{
		const double sx = double(m_shot.width()) / std::max(1, width()), sy = double(m_shot.height()) / std::max(1, height());
		const QPoint px(int(p.x() * sx), int(p.y() * sy));
		return m_shot.rect().contains(px) ? m_shot.pixelColor(px) : QColor();
	}
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.drawImage(QRect(0, 0, width(), height()), m_shot);
		if (!m_hasMouse)
			return;
		// Magnifier: 11x11 pixels around the pointer, the center one outlined, plus the hex.
		const double sx = double(m_shot.width()) / std::max(1, width()), sy = double(m_shot.height()) / std::max(1, height());
		const QPoint c(int(m_mouse.x() * sx), int(m_mouse.y() * sy));
		const int n = 11, cell = 12, size = n * cell;
		QPointF at = m_mouse + QPointF(24, 24);
		if (at.x() + size > width()) at.setX(m_mouse.x() - 24 - size);
		if (at.y() + size + 30 > height()) at.setY(m_mouse.y() - 24 - size - 30);
		const QRectF box(at, QSizeF(size, size));
		p.setRenderHint(QPainter::Antialiasing);
		QPainterPath clip;
		clip.addRoundedRect(box, 10, 10);
		p.save();
		p.setClipPath(clip);
		p.setRenderHint(QPainter::SmoothPixmapTransform, false);
		const QRect src(c.x() - n / 2, c.y() - n / 2, n, n);
		p.fillRect(box, Qt::black);
		p.drawImage(box, m_shot.copy(src));
		p.restore();
		p.setPen(QPen(QColor(255, 255, 255, 230), 2));
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(box, 10, 10);
		const QRectF mid(box.x() + (n / 2) * cell, box.y() + (n / 2) * cell, cell, cell);
		p.setPen(QPen(Qt::white, 2));
		p.drawRect(mid);
		p.setPen(QPen(Qt::black, 1));
		p.drawRect(mid.adjusted(-2, -2, 2, 2));
		const QColor col = colorAt(m_mouse);
		const QRectF label(box.x(), box.bottom() + 6, size, 24);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(23, 23, 23, 235));
		p.drawRoundedRect(label, 6, 6);
		p.setBrush(col);
		p.drawRoundedRect(QRectF(label.x() + 6, label.y() + 5, 14, 14), 3, 3);
		QFont f("Consolas");
		f.setPixelSize(13);
		p.setFont(f);
		p.setPen(Qt::white);
		p.drawText(label.adjusted(28, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, col.name().toUpper());
	}
	void mouseMoveEvent(QMouseEvent *e) override
	{
		m_mouse = e->position();
		m_hasMouse = true;
		update();
	}
	void mousePressEvent(QMouseEvent *e) override
	{
		if (e->button() == Qt::LeftButton) {
			const QColor c = colorAt(e->position());
			m_owner->finish(c.isValid() ? c.name() : QString());
		} else {
			m_owner->finish({});
		}
	}
	void keyPressEvent(QKeyEvent *e) override
	{
		if (e->key() == Qt::Key_Escape)
			m_owner->finish({});
	}

private:
	Eyedropper *m_owner;
	QImage m_shot;
	QPointF m_mouse;
	bool m_hasMouse = false;
};

void Eyedropper::start()
{
	if (active())
		return;
	for (QScreen *s : QGuiApplication::screens())
		m_windows << new EyedropperWindow(this, s);
	for (QRasterWindow *w : m_windows) {
		w->show();
		w->raise();
	}
	if (!m_windows.isEmpty())
		m_windows.first()->requestActivate();
}

void Eyedropper::finish(const QString &hex)
{
	const QList<QRasterWindow *> ws = m_windows;
	m_windows.clear();
	for (QRasterWindow *w : ws)
		w->deleteLater();
	if (hex.isEmpty())
		emit canceled();
	else
		emit picked(hex);
}
