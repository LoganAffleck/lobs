#include "window_chrome.h"

#include <QMouseEvent>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>

#import <AppKit/AppKit.h>

#include <algorithm>
#include <cmath>
#include <functional>

static NSWindow *nsWindow(QWindow *w)
{
	return w ? [(__bridge NSView *)reinterpret_cast<void *>(w->winId()) window] : nil;
}

// macOS puts the window buttons in its own 28-point title bar, near the top of the window; the
// top bar is taller, so the title bar strip is stretched to its height and the buttons centered
// in it, as other apps with tall toolbars do. AppKit lays them out again on resize, so this runs
// after each one.
static void centerWindowButtons(NSWindow *w, CGFloat barHeight)
{
	NSButton *close = [w standardWindowButton:NSWindowCloseButton];
	NSView *container = close.superview.superview; // the title bar's container view
	if (!close || !container || (w.styleMask & NSWindowStyleMaskFullScreen))
		return;
	NSRect frame = container.frame;
	frame.size.height = barHeight;
	frame.origin.y = NSHeight(w.frame) - barHeight;
	container.frame = frame;
	for (NSWindowButton b : {NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton}) {
		NSButton *button = [w standardWindowButton:b];
		NSPoint origin = button.frame.origin;
		origin.y = std::round((NSHeight(button.superview.frame) - NSHeight(button.frame)) / 2);
		[button setFrameOrigin:origin];
	}
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
		QPointer<WindowChrome> self(this);
		__weak NSWindow *weak = w;
		for (NSNotificationName n : {NSWindowDidResizeNotification, NSWindowDidExitFullScreenNotification, NSWindowDidBecomeKeyNotification})
			[[NSNotificationCenter defaultCenter] addObserverForName:n object:w queue:nil usingBlock:^(NSNotification *) {
				if (self && weak)
					centerWindowButtons(weak, self->m_caption);
			}];
		centerWindowButtons(w, m_caption);
		connect(this, &WindowChrome::captionHeightChanged, this, [this, w] { centerWindowButtons(w, m_caption); });
	}
	window->installEventFilter(this);
	connect(window, &QWindow::windowStateChanged, this, &WindowChrome::leadingInsetChanged);
}

int WindowChrome::leadingInset() const
{
	return m_window && (m_window->windowStates() & Qt::WindowFullScreen) ? 0 : 72;
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

// Whether the top bar itself is what's under the point, not something drawn over it (Settings,
// a popup, a dialog's dimmer), which keeps its clicks.
bool WindowChrome::onCaption(const QPointF &scenePos) const
{
	std::function<QQuickItem *(QQuickItem *)> top = [&](QQuickItem *it) -> QQuickItem * {
		if (!it->isVisible() || it->opacity() <= 0)
			return nullptr;
		QList<QQuickItem *> kids = it->childItems();
		std::stable_sort(kids.begin(), kids.end(), [](QQuickItem *a, QQuickItem *b) { return a->z() < b->z(); });
		for (int k = int(kids.size()) - 1; k >= 0; --k)
			if (QQuickItem *hit = top(kids[k]))
				return hit;
		const bool takesInput = it->acceptedMouseButtons() != Qt::NoButton || it == m_captionItem;
		return takesInput && it->contains(it->mapFromScene(scenePos)) ? it : nullptr;
	};
	for (QQuickItem *hit = m_window ? top(m_window->contentItem()->parentItem() ? m_window->contentItem()->parentItem() : m_window->contentItem()) : nullptr;
	     hit; hit = hit->parentItem())
		if (hit == m_captionItem)
			return true;
	return false;
}

bool WindowChrome::eventFilter(QObject *watched, QEvent *event)
{
	if (watched != m_window || (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick))
		return false;
	auto *me = static_cast<QMouseEvent *>(event);
	const QPointF p = me->scenePosition();
	if (me->button() != Qt::LeftButton || p.y() >= m_caption || isControlAt(p) || !onCaption(p))
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

QString WindowChrome::keys(const QString &text)
{
	static const QRegularExpression ctrlShift(R"(Ctrl\s*[+ ]\s*Shift\s*[+ ]\s*)"), shiftCtrl(R"(Shift\s*[+ ]\s*Ctrl\s*[+ ]\s*)"),
		ctrl(R"(Ctrl\s*[+ ]\s*(?=\S))"), alt(R"(Alt\s*[+ ]\s*(?=\S))");
	QString t = text;
	t.replace(ctrlShift, QStringLiteral("⇧⌘")).replace(shiftCtrl, QStringLiteral("⇧⌘")).replace(ctrl, QStringLiteral("⌘")).replace(alt, QStringLiteral("⌥"));
	return t.replace(QStringLiteral("Ctrl"), QStringLiteral("⌘")).replace(QRegularExpression(R"(\bAlt\b)"), QStringLiteral("⌥"));
}

bool WindowChrome::nativeEventFilter(const QByteArray &, void *, qintptr *)
{
	return false;
}
