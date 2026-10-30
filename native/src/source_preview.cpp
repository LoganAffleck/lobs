#include "source_preview.h"

#include <QSet>
#include "engine.h"

#include <obs.h>

#include <QPainter>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>

static Engine *g_engine = nullptr;
static QSet<SourcePreview *> g_previews; // for releaseAll
void SourcePreview::setEngine(Engine *e) { g_engine = e; }

// As BrowserView::releaseAll: nothing of the engine's outlives its shutdown.
void SourcePreview::releaseAll()
{
	g_engine = nullptr;
	for (SourcePreview *p : std::as_const(g_previews))
		p->release();
}

static bool isAudio(const QString &kind) { return kind == "mic" || kind == "desktop_audio"; }

static void onPeak(void *data, const float *, const float peak[MAX_AUDIO_CHANNELS], const float *)
{
	float m = -60.0f;
	for (int c = 0; c < 2; c++)
		if (std::isfinite(peak[c]))
			m = std::max(m, peak[c]);
	static_cast<std::atomic<float> *>(data)->store(m);
}

SourcePreview::SourcePreview(QQuickItem *parent) : QQuickPaintedItem(parent)
{
	g_previews.insert(this);
	setOpaquePainting(true);
	m_timer.setInterval(66); // ~15 fps: enough to see it's the right camera or page
	connect(&m_timer, &QTimer::timeout, this, &SourcePreview::tick);
	m_restart.setSingleShot(true);
	m_restart.setInterval(250); // typing a URL or flipping settings settles before the input is rebuilt
	connect(&m_restart, &QTimer::timeout, this, &SourcePreview::restart);
}

SourcePreview::~SourcePreview() {
	g_previews.remove(this); release(); }

void SourcePreview::setKind(const QString &k)
{
	if (k == m_kind)
		return;
	m_kind = k;
	emit kindChanged();
	release();
	m_restart.start();
}

void SourcePreview::setSettings(const QVariantMap &s)
{
	if (s == m_settings)
		return;
	m_settings = s;
	emit settingsChanged();
	m_restart.start();
}

void SourcePreview::setSourceId(const QString &id)
{
	if (id == m_sourceId)
		return;
	m_sourceId = id;
	emit sourceIdChanged();
	release(); // let go of the device right away, before anything else opens it
	m_restart.start();
}

void SourcePreview::setLive(bool on)
{
	if (on == m_live)
		return;
	m_live = on;
	emit liveChanged();
	if (on)
		m_restart.start();
	else
		release();
}

void SourcePreview::release()
{
	m_restart.stop();
	m_timer.stop();
	if (m_meter) {
		obs_volmeter_remove_callback(m_meter, onPeak, &m_peak);
		obs_volmeter_destroy(m_meter);
		m_meter = nullptr;
	}
	if (m_src) {
		obs_source_dec_showing(m_src);
		obs_source_release(m_src);
		m_src = nullptr;
	}
	m_frame = {};
	m_peak = -60.0f;
	if (m_level != -60) {
		m_level = -60;
		emit levelChanged();
	}
	if (m_ready) {
		m_ready = false;
		emit readyChanged();
	}
	update();
}

void SourcePreview::restart()
{
	release();
	if (!m_live || !g_engine || m_kind.isEmpty())
		return;
	if (!m_sourceId.isEmpty()) { // its frames come from the source that has the device
		m_timer.start();
		return;
	}
	if (m_kind == "browser" && m_settings.value(m_settings.value("local").toBool() ? "file" : "url").toString().trimmed().isEmpty())
		return; // nothing to load yet
	m_src = g_engine->createPreviewInput(m_kind, m_settings);
	if (!m_src)
		return;
	obs_source_inc_showing(m_src);
	if (isAudio(m_kind)) {
		m_meter = obs_volmeter_create(OBS_FADER_LOG);
		obs_volmeter_attach_source(m_meter, m_src);
		obs_volmeter_add_callback(m_meter, onPeak, &m_peak);
	}
	m_timer.start();
}

void SourcePreview::reload()
{
	if (!m_src)
		return;
	obs_properties_t *props = obs_source_properties(m_src);
	if (obs_property_t *p = props ? obs_properties_get(props, "refreshnocache") : nullptr)
		obs_property_button_clicked(p, m_src);
	obs_properties_destroy(props);
}

void SourcePreview::tick()
{
	if (!m_sourceId.isEmpty() && g_engine) {
		const double dpr = window() ? window()->devicePixelRatio() : 1.0;
		const QImage img = g_engine->renderSourceThumbnail(m_sourceId, int(std::max(width(), 160.0) * dpr));
		if (img.isNull())
			return;
		m_frame = img;
		if (!m_ready) {
			m_ready = true;
			emit readyChanged();
		}
		update();
		return;
	}
	if (!m_src)
		return;
	if (m_meter) {
		// Falls back slowly, like the mixer's meters, so short sounds are easy to see.
		const double now = m_peak.load();
		const double next = now >= m_level ? now : std::max(now, m_level - 2.0);
		if (std::abs(next - m_level) > 0.05) {
			m_level = next;
			emit levelChanged();
		}
		if (!m_ready) {
			m_ready = true;
			emit readyChanged();
		}
		return;
	}
	const QImage img = grabFrame();
	if (img.isNull())
		return;
	m_frame = img;
	if (!m_ready) {
		m_ready = true;
		emit readyChanged();
	}
	update();
}

QImage SourcePreview::grabFrame() const
{
	const uint32_t sw = obs_source_get_width(m_src), sh = obs_source_get_height(m_src);
	if (!sw || !sh)
		return {};
	// Read back at about the size it's shown, not the source's own (a 4K display is 30 MB a frame).
	const double dpr = window() ? window()->devicePixelRatio() : 1.0;
	const double k = std::min(1.0, std::max(width(), 160.0) * dpr / sw);
	const uint32_t w = std::max<uint32_t>(1, uint32_t(sw * k)), h = std::max<uint32_t>(1, uint32_t(sh * k));
	QImage img;
	obs_enter_graphics();
	gs_texrender_t *tr = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	if (gs_texrender_begin(tr, w, h)) {
		vec4 clear;
		vec4_set(&clear, 0, 0, 0, 0);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, float(sw), 0.0f, float(sh), -100.0f, 100.0f);
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
			img = QImage(data, int(w), int(h), int(linesize), QImage::Format_RGBA8888_Premultiplied).copy();
			gs_stagesurface_unmap(stage);
		}
		gs_stagesurface_destroy(stage);
	}
	gs_texrender_destroy(tr);
	obs_leave_graphics();
	return img;
}

void SourcePreview::paint(QPainter *p)
{
	const QRectF r = boundingRect();
	// A checkerboard shows where a page (or capture) is transparent.
	p->fillRect(r, QColor(23, 23, 23));
	if (m_frame.isNull())
		return;
	const QSizeF fit = QSizeF(m_frame.size()).scaled(r.size(), Qt::KeepAspectRatio);
	const QRectF to((r.width() - fit.width()) / 2, (r.height() - fit.height()) / 2, fit.width(), fit.height());
	QPixmap checks(16, 16);
	checks.fill(QColor(40, 40, 40));
	QPainter cp(&checks);
	cp.fillRect(0, 0, 8, 8, QColor(52, 52, 52));
	cp.fillRect(8, 8, 8, 8, QColor(52, 52, 52));
	cp.end();
	p->fillRect(to, QBrush(checks));
	p->setRenderHint(QPainter::SmoothPixmapTransform);
	p->drawImage(to, m_frame);
}
