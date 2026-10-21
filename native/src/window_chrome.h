#pragma once
// The main window draws its own title bar (the app's top bar) while Windows keeps the real
// frame behavior: drag to move, double-click to maximize, Aero Snap, resize edges, shadow.
#include <QAbstractNativeEventFilter>
#include <QObject>

class QQuickWindow;
class QQuickItem;

class WindowChrome : public QObject, public QAbstractNativeEventFilter
{
	Q_OBJECT
	// Height of the app's top bar in device-independent pixels: empty parts of it drag the window.
	Q_PROPERTY(int captionHeight MEMBER m_caption NOTIFY captionHeightChanged)
	// The top bar item; only its controls are hit-tested (this runs on every mouse move).
	Q_PROPERTY(QQuickItem *captionItem MEMBER m_captionItem NOTIFY captionHeightChanged)

public:
	explicit WindowChrome(QObject *parent = nullptr) : QObject(parent) {}
	void attach(QQuickWindow *window);
	bool nativeEventFilter(const QByteArray &type, void *message, qintptr *result) override;

	Q_INVOKABLE void minimize();
	Q_INVOKABLE void toggleMaximize();

signals:
	void captionHeightChanged();

private:
	bool isControlAt(const QPointF &scenePos) const;

	QQuickWindow *m_window = nullptr;
	void *m_hwnd = nullptr;
	int m_caption = 44;
	QQuickItem *m_captionItem = nullptr;
};
