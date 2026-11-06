#include "preview_host.h"
#include "app.h"
#include "engine.h"
#include "shellmeta.h"

#include <obs.h>

#include <QFocusEvent>
#include <QHash>
#include <QHoverEvent>
#include <QImageReader>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QRunnable>
#include <QSGRendererInterface>
#include <QSGSimpleTextureNode>
#include <QUrl>
#include <algorithm>
#include <atomic>
#include <functional>

#ifdef Q_OS_WIN
#include <QtQuick/qsgtexture_platform.h>
#include <d3d11.h>
#include <dxgi.h>
#endif

static bool scenePreviewEnabled() { return qEnvironmentVariableIntValue("LOBS_NATIVE_PREVIEW") != 1; }

bool canvasKey(App *app, QKeyEvent *e)
{
	if (app->textKey(e))
		return true;
	const bool ctrl = e->modifiers() & Qt::ControlModifier, shift = e->modifiers() & Qt::ShiftModifier;
	const int step = shift ? 10 : 1;
	switch (e->key()) {
	case Qt::Key_Left: return app->nudge(-step, 0), true;
	case Qt::Key_Right: return app->nudge(step, 0), true;
	case Qt::Key_Up: return app->nudge(0, -step), true;
	case Qt::Key_Down: return app->nudge(0, step), true;
	case Qt::Key_Delete:
	case Qt::Key_Backspace: return app->deleteSelected(), true;
	case Qt::Key_Z:
		if (ctrl) return (shift ? app->redo() : app->undo()), true;
		break;
	case Qt::Key_Y:
		if (ctrl) return app->redo(), true;
		break;
	case Qt::Key_D:
		if (ctrl) return app->duplicateSelected(), true;
		break;
	case Qt::Key_G:
		if (ctrl) return (shift ? app->ungroupSelected() : app->groupSelected()), true;
		break;
	case Qt::Key_BracketRight:
	case Qt::Key_BraceRight:
		if (ctrl) return app->moveSelected(shift ? -1000 : -1), true;
		break;
	case Qt::Key_BracketLeft:
	case Qt::Key_BraceLeft:
		if (ctrl) return app->moveSelected(shift ? 1000 : 1), true;
		break;
	case Qt::Key_Escape:
		if (app->cropMode())
			return app->cancelCrop(), true;
		return app->setTool("select"), true;
	case Qt::Key_Return:
	case Qt::Key_Enter:
		if (app->cropMode())
			return app->setCropMode(false), true;
		break;
	}
	if (!ctrl && !(e->modifiers() & Qt::AltModifier)) {
		static const QHash<int, const char *> keys = {{Qt::Key_V, "select"}, {Qt::Key_T, "text"}, {Qt::Key_R, "rect"}, {Qt::Key_O, "ellipse"},
							      {Qt::Key_A, "arrow"}, {Qt::Key_P, "pencil"}, {Qt::Key_L, "laser"}};
		if (const char *t = keys.value(e->key()))
			return app->setTool(t), true;
	}
	return false;
}

class PreviewWindow : public QWindow {
public:
	PreviewWindow(QWindow *parent, App *app) : QWindow(parent), m_app(app) { setFlags(Qt::FramelessWindowHint); }

protected:
	void mousePressEvent(QMouseEvent *e) override
	{
		if (e->button() == Qt::LeftButton)
			m_app->canvasPress(e->position(), e->modifiers(), false);
		else if (e->button() == Qt::RightButton)
			m_app->requestCanvasMenu(e->position());
		requestActivate();
	}
	void mouseDoubleClickEvent(QMouseEvent *e) override
	{
		if (e->button() == Qt::LeftButton)
			m_app->canvasPress(e->position(), e->modifiers(), true);
	}
	void mouseMoveEvent(QMouseEvent *e) override
	{
		if (e->buttons() & Qt::LeftButton)
			m_app->canvasMove(e->position(), e->modifiers());
		else
			setCursor(m_app->cursorAt(e->position()));
	}
	void mouseReleaseEvent(QMouseEvent *) override { m_app->canvasRelease(); }
	void focusOutEvent(QFocusEvent *e) override
	{
		m_app->endTextEdit();
		QWindow::focusOutEvent(e);
	}
	void keyPressEvent(QKeyEvent *e) override
	{
		if (!canvasKey(m_app, e))
			QWindow::keyPressEvent(e);
	}

private:
	App *m_app;
};

ThumbProvider::ThumbProvider(Engine *e) : QQuickImageProvider(QQuickImageProvider::Image), m_engine(e) {}

QImage ThumbProvider::requestImage(const QString &id, QSize *size, const QSize &req)
{
	const QString key = id.section('?', 0, 0);
	const int w = req.width() > 0 ? req.width() : 320;
	const QImage img = key == "__transition" ? m_engine->renderTransitionPreview(w)
			 : key.startsWith("src:") ? m_engine->renderSourceThumbnail(key.mid(4), w) : m_engine->renderThumbnail(key, w);
	if (size)
		*size = img.size();
	return img;
}

AssetThumbProvider::AssetThumbProvider()
	: QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
{
}

QImage AssetThumbProvider::requestImage(const QString &id, QSize *size, const QSize &req)
{
	const QString path = QUrl::fromPercentEncoding(id.section('?', 0, 0).toUtf8());
	const int want = (std::max)(64, req.width() > 0 ? req.width() : 256);
	QImage img = shellThumbnail(path, want);
	if (img.isNull()) {
		QImageReader r(path);
		r.setAutoTransform(true);
		const QSize s = r.size();
		if (s.isValid() && s.width() > want)
			r.setScaledSize(s.scaled(want, want, Qt::KeepAspectRatio));
		img = r.read();
	}
	if (size)
		*size = img.size();
	return img;
}

QUrl LobsViewFactory::tabbarFilename() const { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockTabBar.qml"); }
QUrl LobsViewFactory::titleBarFilename() const { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockTitleBar.qml"); }
QUrl LobsViewFactory::groupFilename() const { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockGroup.qml"); }
QUrl LobsViewFactory::floatingWindowFilename() const { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockFloatingWindow.qml"); }
QUrl LobsViewFactory::separatorFilename() const { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockSeparator.qml"); }
QUrl LobsViewFactory::dockwidgetFilename() const { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockWidgetBg.qml"); }

#ifdef Q_OS_WIN
class PreviewItem : public QQuickItem {
public:
	PreviewItem(QQuickItem *parent, Engine *engine, App *app, std::function<void()> onFailed)
		: QQuickItem(parent), m_engine(engine), m_app(app), m_onFailed(std::move(onFailed))
	{
		setFlag(ItemHasContents, true);
		setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
		setAcceptHoverEvents(true);
		setActiveFocusOnTab(false);
		setObjectName("previewItem");
		m_engine->startSharedPreview([this] {
			if (!m_pending.exchange(true))
				QMetaObject::invokeMethod(this, [this] { m_pending = false; update(); }, Qt::QueuedConnection);
		});
	}
	~PreviewItem() override
	{
		m_engine->stopSharedPreview();
		dropTextures();
	}
	void setSize(QSizeF size) { QQuickItem::setSize(size); }

protected:
	void geometryChange(const QRectF &n, const QRectF &o) override
	{
		QQuickItem::geometryChange(n, o);
		syncSize();
	}
	void itemChange(ItemChange c, const ItemChangeData &d) override
	{
		QQuickItem::itemChange(c, d);
		if (c == ItemDevicePixelRatioHasChanged || c == ItemSceneChange)
			syncSize();
	}
	QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override
	{
		auto *node = static_cast<QSGSimpleTextureNode *>(old);
		if (m_failed)
			return delete node, nullptr;
		const int gen = m_engine->sharedPreviewGeneration();
		if (gen != m_gen && !openTextures()) {
			m_failed = true;
			QMetaObject::invokeMethod(this, [this] { if (m_onFailed) m_onFailed(); }, Qt::QueuedConnection);
			return delete node, nullptr;
		}
		const int p = m_engine->takeSharedPreviewFrame();
		if (p < 0 || p >= Engine::kSharedFrames || !m_textures[p])
			return node;
		if (!node) {
			node = new QSGSimpleTextureNode;
			node->setOwnsTexture(false);
			node->setFiltering(QSGTexture::Nearest);
		}
		node->setTexture(m_textures[p]);
		m_shown = m_textures[p];
		const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
		node->setRect(QRectF(0, 0, m_texSize.width() / dpr, m_texSize.height() / dpr));
		releaseTextures(m_oldTextures, m_oldD3d);
		return node;
	}
	void releaseResources() override { dropTextures(); }

	void mousePressEvent(QMouseEvent *e) override
	{
		forceActiveFocus(Qt::MouseFocusReason);
		if (e->button() == Qt::LeftButton)
			m_app->canvasPress(e->position(), e->modifiers(), false);
		else if (e->button() == Qt::RightButton)
			m_app->requestCanvasMenu(e->position());
		e->accept();
	}
	void mouseDoubleClickEvent(QMouseEvent *e) override
	{
		if (e->button() == Qt::LeftButton)
			m_app->canvasPress(e->position(), e->modifiers(), true);
		e->accept();
	}
	void mouseMoveEvent(QMouseEvent *e) override
	{
		if (e->buttons() & Qt::LeftButton)
			m_app->canvasMove(e->position(), e->modifiers());
	}
	void mouseReleaseEvent(QMouseEvent *e) override
	{
		if (e->button() == Qt::LeftButton)
			m_app->canvasRelease();
	}
	void hoverMoveEvent(QHoverEvent *e) override { setCursor(m_app->cursorAt(e->position())); }
	void focusOutEvent(QFocusEvent *e) override
	{
		m_app->endTextEdit();
		QQuickItem::focusOutEvent(e);
	}
	void keyPressEvent(QKeyEvent *e) override
	{
		if (!canvasKey(m_app, e))
			e->ignore();
	}

private:
	void syncSize()
	{
		if (!window() || width() < 1 || height() < 1)
			return;
		const qreal dpr = window()->effectiveDevicePixelRatio();
		const int pw = int(width() * dpr), ph = int(height() * dpr);
		m_engine->setPreviewDpr(dpr);
		m_engine->setSharedPreviewSize(pw, ph);
		m_app->setPreviewSize(QSizeF(pw / dpr, ph / dpr), dpr);
	}
	bool openTextures()
	{
		const auto holds = [this](QSGTexture *const *set) { return m_shown && std::find(set, set + Engine::kSharedFrames, m_shown) != set + Engine::kSharedFrames; };
		if (holds(m_textures)) {
			releaseTextures(m_oldTextures, m_oldD3d);
			for (int i = 0; i < Engine::kSharedFrames; i++)
				m_oldTextures[i] = m_textures[i], m_oldD3d[i] = m_d3d[i], m_textures[i] = nullptr, m_d3d[i] = nullptr;
		} else {
			releaseTextures(m_textures, m_d3d);
			if (!holds(m_oldTextures))
				releaseTextures(m_oldTextures, m_oldD3d), m_shown = nullptr;
		}
		QSGRendererInterface *ri = window() ? window()->rendererInterface() : nullptr;
		if (!ri || ri->graphicsApi() != QSGRendererInterface::Direct3D11)
			return false;
		auto *dev = static_cast<ID3D11Device *>(ri->getResource(window(), QSGRendererInterface::DeviceResource));
		auto *obsDev = static_cast<ID3D11Device *>(m_engine->graphicsDevice());
		if (!dev || !obsDev || adapterLuid(dev) != adapterLuid(obsDev))
			return false;
		const Engine::SharedFrames f = m_engine->sharedPreviewFrames();
		m_gen = f.generation;
		if (!f.handles[0])
			return f.generation == 0;
		for (int i = 0; i < Engine::kSharedFrames; i++) {
			ID3D11Texture2D *tex = nullptr;
			if (FAILED(dev->OpenSharedResource(reinterpret_cast<HANDLE>(uintptr_t(f.handles[i])), __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&tex))) || !tex)
				return false;
			m_d3d[i] = tex;
			m_textures[i] = QNativeInterface::QSGD3D11Texture::fromNative(tex, window(), QSize(f.width, f.height));
			if (!m_textures[i])
				return false;
		}
		m_texSize = QSize(f.width, f.height);
		return true;
	}
	static quint64 adapterLuid(ID3D11Device *d)
	{
		IDXGIDevice *dx = nullptr;
		IDXGIAdapter *ad = nullptr;
		DXGI_ADAPTER_DESC desc{};
		if (SUCCEEDED(d->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dx))) && SUCCEEDED(dx->GetAdapter(&ad)))
			ad->GetDesc(&desc);
		if (ad) ad->Release();
		if (dx) dx->Release();
		return (quint64(quint32(desc.AdapterLuid.HighPart)) << 32) | desc.AdapterLuid.LowPart;
	}
	void dropTextures()
	{
		constexpr int n = Engine::kSharedFrames;
		struct Free : QRunnable {
			QSGTexture *t[2 * n] = {}; ID3D11Texture2D *d[2 * n] = {};
			void run() override { for (int i = 0; i < 2 * n; i++) { delete t[i]; if (d[i]) d[i]->Release(); } }
		};
		auto *job = new Free;
		for (int i = 0; i < n; i++) {
			job->t[i] = m_textures[i], job->d[i] = m_d3d[i], m_textures[i] = nullptr, m_d3d[i] = nullptr;
			job->t[n + i] = m_oldTextures[i], job->d[n + i] = m_oldD3d[i], m_oldTextures[i] = nullptr, m_oldD3d[i] = nullptr;
		}
		m_gen = -1;
		m_shown = nullptr;
		if (window())
			window()->scheduleRenderJob(job, QQuickWindow::NoStage);
		else
			job->run(), delete job;
	}
	static void releaseTextures(QSGTexture **t, ID3D11Texture2D **d)
	{
		for (int i = 0; i < Engine::kSharedFrames; i++) {
			delete t[i];
			t[i] = nullptr;
			if (d[i])
				d[i]->Release();
			d[i] = nullptr;
		}
	}

	Engine *m_engine;
	App *m_app;
	std::function<void()> m_onFailed;
	std::atomic<bool> m_pending{false};
	bool m_failed = false;
	int m_gen = -1;
	QSGTexture *m_textures[Engine::kSharedFrames] = {}, *m_oldTextures[Engine::kSharedFrames] = {};
	QSGTexture *m_shown = nullptr;
	ID3D11Texture2D *m_d3d[Engine::kSharedFrames] = {}, *m_oldD3d[Engine::kSharedFrames] = {};
	QSize m_texSize;
};
#endif

PreviewHost::PreviewHost(Engine *engine, App *app) : m_engine(engine), m_app(app) {}
PreviewHost::~PreviewHost() { delete m_native; }

void PreviewHost::attach(QQuickItem *item)
{
	if (m_native || m_inScene || !item || !item->window())
		return;
	m_item = item;
#ifdef Q_OS_WIN
	if (scenePreviewEnabled()) {
		m_inScene = new PreviewItem(item, m_engine, m_app, [this] {
			blog(LOG_WARNING, "[lobs] preview: shared textures unavailable, using a native window");
			m_inScene->deleteLater();
			m_inScene = nullptr;
			m_app->setPreviewInScene(false);
			attachNative();
		});
		auto fit = [this] { if (m_inScene && m_item) m_inScene->setSize(m_item->size()); };
		connect(item, &QQuickItem::widthChanged, m_inScene, fit);
		connect(item, &QQuickItem::heightChanged, m_inScene, fit);
		fit();
		m_app->setPreviewInScene(true);
		blog(LOG_INFO, "[lobs] preview: drawn in the UI scene (shared textures)");
		return;
	}
#endif
	attachNative();
}

void PreviewHost::attachNative()
{
	QQuickItem *item = m_item;
	if (m_native || !item || !item->window())
		return;
	QQuickWindow *top = item->window();
	m_native = new PreviewWindow(top, m_app);
	m_native->create();
	connect(top, &QQuickWindow::afterAnimating, this, &PreviewHost::sync);
	sync();
	m_native->show();
	connect(m_app, &App::previewVisibilityRequested, this, [this](bool on) { m_hidden = !on; sync(); });
	const qreal dpr = m_native->devicePixelRatio();
	m_engine->setPreviewDpr(dpr);
	m_engine->attachPreview(reinterpret_cast<void *>(m_native->winId()), int(m_native->width() * dpr), int(m_native->height() * dpr));
}

void PreviewHost::sync()
{
	if (!m_item || !m_native)
		return;
	const bool show = !m_hidden && m_item->isVisible() && m_item->width() > 4 && m_item->height() > 4;
	if (show != m_native->isVisible())
		m_native->setVisible(show);
	if (!show)
		return;
	const QRect r(m_item->mapToScene(QPointF(0, 0)).toPoint(), QSize(int(m_item->width()), int(m_item->height())));
	if (r == m_rect)
		return;
	m_rect = r;
	m_native->setGeometry(r);
	const qreal dpr = m_native->devicePixelRatio();
	const int pw = int(r.width() * dpr), ph = int(r.height() * dpr);
	m_app->setPreviewSize(QSizeF(pw / dpr, ph / dpr), dpr);
	m_engine->setPreviewDpr(dpr);
	m_engine->resizePreview(pw, ph);
}
