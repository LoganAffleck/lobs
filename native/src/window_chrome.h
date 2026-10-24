#pragma once
// The main window draws its own title bar (the app's top bar) while Windows keeps the real
// frame behavior: drag to move, double-click to maximize, Aero Snap, resize edges, shadow.
// On macOS the top bar sits under a transparent native title bar instead, which keeps the
// traffic-light buttons, full screen and window tiling (window_chrome_mac.mm).
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
	// The system draws the window buttons (macOS), so the top bar hides its own and leaves
	// leadingInset pixels free at its left edge for them (none in full screen, where they hide).
	Q_PROPERTY(bool nativeControls READ nativeControls CONSTANT)
	Q_PROPERTY(int leadingInset READ leadingInset NOTIFY leadingInsetChanged)
	// macOS: File, Edit and Window live in the system menu bar (Main.qml), not the top bar.
	Q_PROPERTY(bool nativeMenuBar READ nativeControls CONSTANT)

public:
	explicit WindowChrome(QObject *parent = nullptr) : QObject(parent) {}
	void attach(QQuickWindow *window);
	bool nativeEventFilter(const QByteArray &type, void *message, qintptr *result) override;
#ifdef Q_OS_MACOS
	bool eventFilter(QObject *watched, QEvent *event) override;
	bool nativeControls() const { return true; }
	int leadingInset() const;
#else
	bool nativeControls() const { return false; }
	int leadingInset() const { return 0; }
#endif

	Q_INVOKABLE void minimize();
	// Shortcut names in UI text as this platform writes them: "Undo  Ctrl Shift Z" stays as is on
	// Windows and reads "Undo  ⇧⌘Z" on macOS (where Qt's Ctrl is Command).
	Q_INVOKABLE static QString keys(const QString &text);
	Q_INVOKABLE void toggleMaximize();

signals:
	void captionHeightChanged();
	void leadingInsetChanged();

private:
	bool isControlAt(const QPointF &scenePos) const;
#ifdef Q_OS_MACOS
	bool onCaption(const QPointF &scenePos) const;
#endif

	QQuickWindow *m_window = nullptr;
	void *m_hwnd = nullptr;
	int m_caption = 44;
	QQuickItem *m_captionItem = nullptr;
};
