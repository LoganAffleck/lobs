#include "window_chrome.h"

#include <QCoreApplication>
#include <QQuickItem>
#include <QQuickWindow>

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <algorithm>
#include <functional>

void WindowChrome::attach(QQuickWindow *window)
{
	m_window = window;
	HWND hwnd = reinterpret_cast<HWND>(window->winId());
	m_hwnd = hwnd;
	// Qt treats the window as frameless (Qt.FramelessWindowHint in Main.qml), so its geometry
	// and popup positions match the client area. The frame styles come back here so Windows
	// still snaps, animates minimize/maximize and draws the shadow.
	const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
	SetWindowLongPtrW(hwnd, GWL_STYLE, (style & ~WS_POPUP) | WS_OVERLAPPED | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
	const MARGINS shadow{0, 0, 0, 1};
	DwmExtendFrameIntoClientArea(hwnd, &shadow);
	SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	QCoreApplication::instance()->installNativeEventFilter(this);
}

void WindowChrome::minimize()
{
	if (m_hwnd)
		ShowWindow(static_cast<HWND>(m_hwnd), SW_MINIMIZE);
}

void WindowChrome::toggleMaximize()
{
	if (m_hwnd)
		ShowWindow(static_cast<HWND>(m_hwnd), IsZoomed(static_cast<HWND>(m_hwnd)) ? SW_RESTORE : SW_MAXIMIZE);
}

// Topmost item under the point that takes clicks (a button, field, list): the title bar
// leaves those to the app and drags the window everywhere else.
bool WindowChrome::isControlAt(const QPointF &scenePos) const
{
	std::function<bool(QQuickItem *)> hit = [&](QQuickItem *it) -> bool {
		if (!it->isVisible() || it->opacity() <= 0)
			return false;
		QList<QQuickItem *> kids = it->childItems();
		// Paint order: by z, then later siblings on top. Test the topmost first.
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

QString WindowChrome::keys(const QString &text)
{
	return text;
}

bool WindowChrome::nativeEventFilter(const QByteArray &, void *message, qintptr *result)
{
	MSG *msg = static_cast<MSG *>(message);
	if (!m_hwnd || msg->hwnd != static_cast<HWND>(m_hwnd))
		return false;
	HWND hwnd = msg->hwnd;
	switch (msg->message) {
	case WM_NCCALCSIZE:
		if (msg->wParam) { // the whole window is client area: no native title bar or borders
			*result = 0;
			return true;
		}
		return false;
	case WM_NCACTIVATE: // don't let Windows repaint a frame we don't have
		*result = DefWindowProcW(hwnd, msg->message, msg->wParam, -1);
		return true;
	case WM_GETMINMAXINFO: {
		// Maximized, a window with a resize frame spills past the screen by the frame's width.
		// Keep it exactly on the work area (taskbar stays visible).
		auto *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);
		MONITORINFO mi{sizeof(mi)};
		if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
			mmi->ptMaxPosition = {mi.rcWork.left - mi.rcMonitor.left, mi.rcWork.top - mi.rcMonitor.top};
			mmi->ptMaxSize = {mi.rcWork.right - mi.rcWork.left, mi.rcWork.bottom - mi.rcWork.top};
		}
		return false; // Qt adds its own minimum size
	}
	case WM_NCHITTEST: {
		const POINT pt{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
		RECT wr;
		GetWindowRect(hwnd, &wr);
		const int x = pt.x - wr.left, y = pt.y - wr.top, w = wr.right - wr.left, h = wr.bottom - wr.top;
		if (!IsZoomed(hwnd)) {
			const UINT dpi = GetDpiForWindow(hwnd);
			const int b = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
			const bool l = x < b, r = x >= w - b, t = y < b, bo = y >= h - b;
			if (t && l) return *result = HTTOPLEFT, true;
			if (t && r) return *result = HTTOPRIGHT, true;
			if (bo && l) return *result = HTBOTTOMLEFT, true;
			if (bo && r) return *result = HTBOTTOMRIGHT, true;
			if (l) return *result = HTLEFT, true;
			if (r) return *result = HTRIGHT, true;
			if (t) return *result = HTTOP, true;
			if (bo) return *result = HTBOTTOM, true;
		}
		const qreal dpr = m_window->devicePixelRatio();
		const QPointF p(x / dpr, y / dpr);
		*result = p.y() < m_caption && !isControlAt(p) ? HTCAPTION : HTCLIENT;
		return true;
	}
	default:
		return false;
	}
}
