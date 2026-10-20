#include "projector.h"
#include "engine.h"

#include <obs.h>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QScreen>

Projector::Projector(Engine *engine, const QString &sceneId, const QString &title, QScreen *fullscreenOn)
	: m_engine(engine)
{
	setTitle(title);
	setMinimumSize(QSize(160, 90));
	m_canvasW = engine->canvasW();
	m_canvasH = engine->canvasH();
	if (!sceneId.isEmpty()) {
		m_scene = engine->sceneSource(sceneId);
		if (m_scene) {
			m_sceneId = sceneId;
			engine->holdScene(sceneId, true);
			obs_source_get_ref(m_scene);
			obs_source_inc_showing(m_scene);
		}
	}
	if (fullscreenOn) {
		setFlags(Qt::Window | Qt::FramelessWindowHint);
		setScreen(fullscreenOn);
		setGeometry(fullscreenOn->geometry());
		setCursor(Qt::BlankCursor);
		showFullScreen();
	} else {
		resize(960, 540);
		show();
	}
}

Projector::~Projector()
{
	if (m_display) {
		obs_display_remove_draw_callback(m_display, draw, this);
		obs_display_destroy(m_display);
	}
	if (m_scene) {
		m_engine->holdScene(m_sceneId, false);
		obs_source_dec_showing(m_scene);
		obs_source_release(m_scene);
	}
}

void Projector::ensureDisplay()
{
	if (m_display || !isExposed())
		return;
	const qreal dpr = devicePixelRatio();
	gs_init_data init = {};
	init.window.hwnd = reinterpret_cast<void *>(winId());
	init.cx = uint32_t(width() * dpr);
	init.cy = uint32_t(height() * dpr);
	init.format = GS_BGRA;
	init.zsformat = GS_ZS_NONE;
	m_display = obs_display_create(&init, 0xFF000000);
	obs_display_add_draw_callback(m_display, draw, this);
}

void Projector::exposeEvent(QExposeEvent *) { ensureDisplay(); }

void Projector::resizeEvent(QResizeEvent *)
{
	if (m_display) {
		const qreal dpr = devicePixelRatio();
		obs_display_resize(m_display, uint32_t(width() * dpr), uint32_t(height() * dpr));
	}
}

// Fits the canvas in the window, letterboxed, centered.
void Projector::draw(void *data, uint32_t cx, uint32_t cy)
{
	auto *p = static_cast<Projector *>(data);
	const float W = float(p->m_canvasW), H = float(p->m_canvasH);
	const float s = std::min(float(cx) / W, float(cy) / H);
	const int vw = int(W * s), vh = int(H * s), vx = (int(cx) - vw) / 2, vy = (int(cy) - vh) / 2;
	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, W, 0.0f, H, -100.0f, 100.0f);
	gs_set_viewport(vx, vy, vw, vh);
	if (p->m_scene)
		obs_source_video_render(p->m_scene);
	else
		obs_render_main_texture();
	gs_projection_pop();
	gs_viewport_pop();
}

void Projector::keyPressEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_Escape)
		close();
	else
		QWindow::keyPressEvent(e);
}

void Projector::mouseDoubleClickEvent(QMouseEvent *)
{
	if (windowState() & Qt::WindowFullScreen) {
		setFlags(Qt::Window);
		unsetCursor();
		showNormal();
		resize(960, 540);
	} else {
		setCursor(Qt::BlankCursor);
		showFullScreen();
	}
}

bool Projector::event(QEvent *e)
{
	if (e->type() == QEvent::Close) {
		deleteLater(); // a projector lives until it's closed
	}
	return QWindow::event(e);
}
