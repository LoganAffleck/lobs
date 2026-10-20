#pragma once

#include <QObject>
#include <QList>

class QRasterWindow;

// Screen eyedropper: freezes every screen into a full-screen window with a magnifier; a click
// picks the pixel under the pointer (picked), Esc or right-click cancels (canceled).
class Eyedropper : public QObject {
	Q_OBJECT
public:
	using QObject::QObject;
	void start();
	bool active() const { return !m_windows.isEmpty(); }

signals:
	void picked(const QString &hex);
	void canceled();

private:
	friend class EyedropperWindow;
	void finish(const QString &hex);
	QList<QRasterWindow *> m_windows;
};
