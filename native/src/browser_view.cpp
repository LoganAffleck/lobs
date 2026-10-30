#include "browser_view.h"

#include <obs.h>
#include <obs-interaction.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QQuickWindow>
#include <QUuid>
#include <algorithm>
#include <cmath>

static bool g_engineReady = false;
void BrowserView::setEngineReady(bool ready) { g_engineReady = ready; }

static obs_data_t *toObsData(const QJsonObject &o)
{
	return obs_data_create_from_json(QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
}

static uint32_t modifiersOf(Qt::KeyboardModifiers m, Qt::MouseButtons b = {})
{
	uint32_t out = 0;
	if (m & Qt::ShiftModifier) out |= INTERACT_SHIFT_KEY;
	if (m & Qt::ControlModifier) out |= INTERACT_CONTROL_KEY;
	if (m & Qt::AltModifier) out |= INTERACT_ALT_KEY;
	if (m & Qt::KeypadModifier) out |= INTERACT_IS_KEY_PAD;
	if (b & Qt::LeftButton) out |= INTERACT_MOUSE_LEFT;
	if (b & Qt::MiddleButton) out |= INTERACT_MOUSE_MIDDLE;
	if (b & Qt::RightButton) out |= INTERACT_MOUSE_RIGHT;
	return out;
}

BrowserView::BrowserView(QQuickItem *parent) : QQuickPaintedItem(parent)
{
	setAcceptedMouseButtons(Qt::AllButtons);
	setAcceptHoverEvents(true);
	setFlag(ItemAcceptsInputMethod, true);
	setActiveFocusOnTab(true);
	setOpaquePainting(true);
	m_timer.setInterval(33); // ~30 fps is plenty for chat and dashboards
	connect(&m_timer, &QTimer::timeout, this, &BrowserView::grab);
	m_resize.setSingleShot(true);
	m_resize.setInterval(150);
	connect(&m_resize, &QTimer::timeout, this, [this] {
		if (!m_src)
			return ensureSource();
		const QSize s = pageSize();
		if (s == m_srcSize || s.isEmpty())
			return;
		obs_data_t *d = toObsData({{"width", s.width()}, {"height", s.height()}});
		obs_source_update(m_src, d);
		obs_data_release(d);
		m_srcSize = s;
	});
}

BrowserView::~BrowserView() { releaseSource(); }

QSize BrowserView::pageSize() const
{
	const double dpr = window() ? window()->devicePixelRatio() : 1.0;
	return QSize(int(width() * dpr), int(height() * dpr));
}

void BrowserView::setUrl(const QString &u)
{
	if (u == m_url)
		return;
	m_url = u;
	emit urlChanged();
	if (m_src) {
		obs_data_t *d = toObsData({{"url", m_url}});
		obs_source_update(m_src, d);
		obs_data_release(d);
	} else {
		ensureSource();
	}
}

// Zoom is CSS zoom on the page, so text re-lays out crisply instead of being scaled. (This
// also replaces the browser source's default CSS, which hides scrollbars.) The page renders at
// device pixels, so the display's scaling is part of it: at 150% Windows scaling, 100% here is
// CSS zoom 1.5, and the page is the size a browser would show it.
QString BrowserView::pageCss() const
{
	const double dpr = window() ? window()->devicePixelRatio() : 1.0;
	return QString("html { zoom: %1; }").arg(m_zoom * dpr, 0, 'f', 2);
}

void BrowserView::setZoom(double z)
{
	z = std::clamp(std::round(z * 10) / 10, 0.5, 2.5);
	if (std::abs(z - m_zoom) < 0.001)
		return;
	m_zoom = z;
	emit zoomChanged();
	if (m_src) {
		obs_data_t *d = toObsData({{"css", pageCss()}});
		obs_source_update(m_src, d);
		obs_data_release(d);
	}
}

bool BrowserView::zoomKey(QKeyEvent *e)
{
	if (!(e->modifiers() & Qt::ControlModifier))
		return false;
	switch (e->key()) {
	case Qt::Key_Plus:
	case Qt::Key_Equal:
		setZoom(m_zoom + 0.1);
		return true;
	case Qt::Key_Minus:
	case Qt::Key_Underscore:
		setZoom(m_zoom - 0.1);
		return true;
	case Qt::Key_0:
		setZoom(1.0);
		return true;
	}
	return false;
}

void BrowserView::setLive(bool on)
{
	if (on == m_live)
		return;
	m_live = on;
	emit liveChanged();
	if (on)
		ensureSource();
	else
		m_timer.stop();
	if (m_src && on)
		m_timer.start();
}

void BrowserView::ensureSource()
{
	if (m_src || !g_engineReady || m_url.isEmpty() || !m_live)
		return;
	const QSize s = pageSize();
	if (s.width() < 16 || s.height() < 16)
		return;
	// Not part of any scene, so viewers never see or hear it.
	obs_data_t *d = toObsData({{"url", m_url}, {"width", s.width()}, {"height", s.height()}, {"fps_custom", true}, {"fps", 30},
				   {"shutdown", false}, {"restart_when_active", false}, {"reroute_audio", true}, {"webpage_control_level", 1}, {"css", pageCss()}});
	const QByteArray name = ("lobs_panel_" + QUuid::createUuid().toString(QUuid::Id128).left(8)).toUtf8();
	m_src = obs_source_create_private("browser_source", name.constData(), d);
	obs_data_release(d);
	if (!m_src)
		return;
	obs_source_inc_showing(m_src);
	m_srcSize = s;
	m_timer.start();
}

void BrowserView::releaseSource()
{
	m_timer.stop();
	if (m_src) {
		obs_source_dec_showing(m_src);
		obs_source_release(m_src);
		m_src = nullptr;
	}
}

void BrowserView::reload()
{
	if (!m_src)
		return;
	obs_properties_t *props = obs_source_properties(m_src);
	if (obs_property_t *p = props ? obs_properties_get(props, "refreshnocache") : nullptr)
		obs_property_button_clicked(p, m_src);
	obs_properties_destroy(props);
}

void BrowserView::grab()
{
	if (!m_src)
		return;
	const uint32_t w = obs_source_get_width(m_src), h = obs_source_get_height(m_src);
	if (!w || !h)
		return;
	QImage img;
	obs_enter_graphics();
	gs_texrender_t *tr = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	if (gs_texrender_begin(tr, w, h)) {
		vec4 clear;
		vec4_set(&clear, 0.09f, 0.09f, 0.09f, 1.0f);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, float(w), 0.0f, float(h), -100.0f, 100.0f);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		obs_source_video_render(m_src);
		gs_blend_state_pop();
		gs_texrender_end(tr);
		gs_stagesurf_t *stage = gs_stagesurface_create(w, h, GS_RGBA);
		gs_stage_texture(stage, gs_texrender_get_texture(tr));
		uint8_t *data = nullptr;
		uint32_t linesize = 0;
		if (gs_stagesurface_map(stage, &data, &linesize)) {
			img = QImage(data, int(w), int(h), int(linesize), QImage::Format_RGBA8888).copy();
			gs_stagesurface_unmap(stage);
		}
		gs_stagesurface_destroy(stage);
	}
	gs_texrender_destroy(tr);
	obs_leave_graphics();
	if (img.isNull())
		return;
	m_frame = img;
	if (!m_ready) {
		m_ready = true;
		emit readyChanged();
	}
	update();
}

void BrowserView::paint(QPainter *p)
{
	p->fillRect(boundingRect(), QColor(23, 23, 23));
	if (!m_frame.isNull())
		p->drawImage(boundingRect(), m_frame);
}

void BrowserView::geometryChange(const QRectF &n, const QRectF &o)
{
	QQuickPaintedItem::geometryChange(n, o);
	if (n.size() != o.size())
		m_resize.start();
}

QPoint BrowserView::toPage(QPointF p) const
{
	const double sx = m_srcSize.width() / std::max(1.0, width()), sy = m_srcSize.height() / std::max(1.0, height());
	return QPoint(int(p.x() * sx), int(p.y() * sy));
}

void BrowserView::sendClick(QMouseEvent *e, bool up, int count)
{
	if (!m_src)
		return;
	const QPoint pt = toPage(e->position());
	obs_mouse_event ev{modifiersOf(e->modifiers(), e->buttons()), int32_t(pt.x()), int32_t(pt.y())};
	const obs_mouse_button_type b = e->button() == Qt::RightButton ? MOUSE_RIGHT : e->button() == Qt::MiddleButton ? MOUSE_MIDDLE : MOUSE_LEFT;
	obs_source_send_mouse_click(m_src, &ev, int32_t(b), up, uint32_t(count));
}

void BrowserView::mousePressEvent(QMouseEvent *e)
{
	forceActiveFocus(Qt::MouseFocusReason);
	sendClick(e, false, 1);
	e->accept();
}
void BrowserView::mouseDoubleClickEvent(QMouseEvent *e) { sendClick(e, false, 2); e->accept(); }
void BrowserView::mouseReleaseEvent(QMouseEvent *e) { sendClick(e, true, 1); e->accept(); }

void BrowserView::mouseMoveEvent(QMouseEvent *e)
{
	if (!m_src)
		return;
	const QPoint pt = toPage(e->position());
	obs_mouse_event ev{modifiersOf(e->modifiers(), e->buttons()), int32_t(pt.x()), int32_t(pt.y())};
	obs_source_send_mouse_move(m_src, &ev, false);
}

void BrowserView::hoverMoveEvent(QHoverEvent *e)
{
	if (!m_src)
		return;
	const QPoint pt = toPage(e->position());
	obs_mouse_event ev{modifiersOf(e->modifiers()), int32_t(pt.x()), int32_t(pt.y())};
	obs_source_send_mouse_move(m_src, &ev, false);
}

void BrowserView::hoverLeaveEvent(QHoverEvent *)
{
	if (!m_src)
		return;
	obs_mouse_event ev{0, 0, 0};
	obs_source_send_mouse_move(m_src, &ev, true);
}

void BrowserView::wheelEvent(QWheelEvent *e)
{
	if (e->modifiers() & Qt::ControlModifier) { // zoom, like a browser
		setZoom(m_zoom + (e->angleDelta().y() > 0 ? 0.1 : -0.1));
		return e->accept();
	}
	if (!m_src)
		return;
	const QPoint pt = toPage(e->position());
	obs_mouse_event ev{modifiersOf(e->modifiers(), e->buttons()), int32_t(pt.x()), int32_t(pt.y())};
	obs_source_send_mouse_wheel(m_src, &ev, e->angleDelta().x(), e->angleDelta().y());
	e->accept();
}

void BrowserView::sendKey(QKeyEvent *e, bool up)
{
	if (!m_src)
		return;
	const QByteArray text = e->text().toUtf8();
	obs_key_event ev{};
	ev.modifiers = modifiersOf(e->modifiers());
	ev.text = const_cast<char *>(text.constData());
	ev.native_modifiers = e->nativeModifiers();
	ev.native_scancode = e->nativeScanCode();
	ev.native_vkey = e->nativeVirtualKey();
	obs_source_send_key_click(m_src, &ev, up);
}

void BrowserView::keyPressEvent(QKeyEvent *e)
{
	if (!zoomKey(e))
		sendKey(e, false);
	e->accept();
}
void BrowserView::keyReleaseEvent(QKeyEvent *e)
{
	if (!(e->modifiers() & Qt::ControlModifier) || !QList<int>{Qt::Key_Plus, Qt::Key_Equal, Qt::Key_Minus, Qt::Key_Underscore, Qt::Key_0}.contains(e->key()))
		sendKey(e, true);
	e->accept();
}

void BrowserView::focusInEvent(QFocusEvent *e)
{
	if (m_src)
		obs_source_send_focus(m_src, true);
	QQuickPaintedItem::focusInEvent(e);
}

void BrowserView::focusOutEvent(QFocusEvent *e)
{
	if (m_src)
		obs_source_send_focus(m_src, false);
	QQuickPaintedItem::focusOutEvent(e);
}
