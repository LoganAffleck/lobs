#include "window_chrome.h"

#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

#import <AppKit/AppKit.h>

#include <algorithm>
#include <functional>

static NSWindow *nsWindow(QWindow *w)
{
	return w ? [(__bridge NSView *)reinterpret_cast<void *>(w->winId()) window] : nil;
}

void WindowChrome::attach(QQuickWindow *window)
{
	m_window = window;
	m_hwnd = reinterpret_cast<void *>(window->winId());
	// Main.qml asks for an expanded client area: the content runs under a transparent title
	// bar. Empty parts of the top bar move the window (eventFilter), as the title bar would.
	if (NSWindow *w = nsWindow(window)) {
		w.titleVisibility = NSWindowTitleHidden;
		w.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
	}
	window->installEventFilter(this);
}

void WindowChrome::minimize()
{
	if (m_window)
		m_window->showMinimized();
}

void WindowChrome::toggleMaximize()
{
	if (NSWindow *w = nsWindow(m_window))
		[w zoom:nil];
}

bool WindowChrome::isControlAt(const QPointF &scenePos) const
{
	std::function<bool(QQuickItem *)> hit = [&](QQuickItem *it) -> bool {
		if (!it->isVisible() || it->opacity() <= 0)
			return false;
		QList<QQuickItem *> kids = it->childItems();
		std::stable_sort(kids.begin(), kids.end(), [](QQuickItem *a, QQuickItem *b) { return a->z() < b->z(); });
		for (int k = int(kids.size()) - 1; k >= 0; --k)
			if (hit(kids[k]))
				return true;
		if (it == m_captionItem || it->inherits("QQuickText"))
			return false;
		return it->acceptedMouseButtons() != Qt::NoButton && it->contains(it->mapFromScene(scenePos));
	};
	return m_captionItem && hit(m_captionItem);
}

bool WindowChrome::eventFilter(QObject *watched, QEvent *event)
{
	if (watched != m_window || (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick))
		return false;
	auto *me = static_cast<QMouseEvent *>(event);
	const QPointF p = me->scenePosition();
	if (me->button() != Qt::LeftButton || p.y() >= m_caption || isControlAt(p))
		return false;
	if (event->type() == QEvent::MouseButtonDblClick) {
		// What the user picked in System Settings > Desktop & Dock for a title bar double-click.
		NSString *action = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleActionOnDoubleClick"];
		if ([action isEqualToString:@"Minimize"])
			minimize();
		else if (![action isEqualToString:@"None"])
			toggleMaximize();
	} else {
		m_window->startSystemMove();
	}
	return true;
}

bool WindowChrome::nativeEventFilter(const QByteArray &, void *, qintptr *)
{
	return false;
}
