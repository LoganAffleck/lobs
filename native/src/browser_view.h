#pragma once

#include <QImage>
#include <QQuickPaintedItem>
#include <QTimer>

struct obs_source;

// An interactive web page in the UI (Twitch chat, stream info...). The page runs in the
// engine's own browser (obs-browser / CEF), offscreen: frames are read back and painted
// here, and mouse, wheel, focus and keys are sent back to the page. Logins persist in the
// engine's browser profile, so you sign in once.
class BrowserView : public QQuickPaintedItem {
	Q_OBJECT
	Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY urlChanged)
	Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged) // false: pause (hidden panels)
	Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
	// Page zoom (1 = 100%). Ctrl + / Ctrl - / Ctrl 0 and Ctrl + wheel change it.
	Q_PROPERTY(double zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
public:
	explicit BrowserView(QQuickItem *parent = nullptr);
	~BrowserView() override;
	static void setEngineReady(bool ready); // set once the engine has started
	static void releaseAll(); // before the engine shuts down: no page outlives CEF

	QString url() const { return m_url; }
	void setUrl(const QString &u);
	bool live() const { return m_live; }
	void setLive(bool on);
	bool ready() const { return m_ready; }
	double zoom() const { return m_zoom; }
	void setZoom(double z);
	Q_INVOKABLE void reload();

	void paint(QPainter *p) override;

signals:
	void urlChanged();
	void liveChanged();
	void readyChanged();
	void zoomChanged();

protected:
	void geometryChange(const QRectF &n, const QRectF &o) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void hoverMoveEvent(QHoverEvent *e) override;
	void hoverLeaveEvent(QHoverEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void keyReleaseEvent(QKeyEvent *e) override;
	void focusInEvent(QFocusEvent *e) override;
	void focusOutEvent(QFocusEvent *e) override;

private:
	void ensureSource();
	void releaseSource();
	void grab();
	void sendKey(QKeyEvent *e, bool up);
	void sendClick(QMouseEvent *e, bool up, int count);
	QPoint toPage(QPointF p) const;
	QSize pageSize() const;

	QString m_url;
	bool m_live = true, m_ready = false;
	double m_zoom = 1.0;
	QString pageCss() const;
	bool zoomKey(QKeyEvent *e);
	obs_source *m_src = nullptr;
	QSize m_srcSize;
	QImage m_frame;
	QTimer m_timer, m_resize;
};
