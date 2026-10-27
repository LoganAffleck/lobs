// LOBS.exe / LOBS.app — native shell. QML draws the UI; a native child window over the "preview"
// item is where libobs renders the edited scene, and where canvas mouse input lands.
#include "window_chrome.h"
#include "captions.h"
#include "crash_report.h"
#include "dock_tabs.h"
#include "app_updater.h"
#include "app.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/qtquick/Platform.h>
#include <kddockwidgets/qtquick/ViewFactory.h>
#include "icons.h"
#include <QQmlPropertyMap>
#include <QIcon>
#include <QSvgRenderer>
#include "engine.h"
#include "engine_manager.h"
#include "hotkeys.h"
#include "permissions.h"
#include "platform.h"
#include "shellmeta.h"
#include "shapes.h"
#include "browser_view.h"
#include "source_preview.h"
#include "startup_mode.h"
#include "rows_model.h"
#include <QElapsedTimer>
#include <QImageReader>
#include <QUrl>

#include <obs.h>
#include <obs-hotkey.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QFontDatabase>
#include <QQuickImageProvider>
#include <QPalette>
#include <QColor>
#include <QQuickWindow>
#include <QRunnable>
#include <QSGSimpleTextureNode>
#include <QSGRendererInterface>
#ifdef Q_OS_WIN
#include <QtQuick/qsgtexture_platform.h>
#include <d3d11.h>
#include <dxgi.h>
#endif
#include <QStandardPaths>
#include <QTextStream>
#include <QThreadPool>
#include <QTimer>
#include <QWindow>
#include <QRasterWindow>
#include <QPainter>
#include <QScreen>

#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#ifdef LOBS_TESTS
#include "test_runner.h"
#endif

#ifdef Q_OS_WIN
QString installEngineZip(const QString &zip, const QString &version, QString *note);
#endif

static QFile *g_log = nullptr;
static QMutex g_logLock; // libobs logs from its own threads; QFile isn't thread-safe (LOBS-040)

static QElapsedTimer g_uptime; // log lines carry ms since launch, to see where startup goes

static void logHandler(int level, const char *fmt, va_list args, void *)
{
	char buf[4096];
	vsnprintf(buf, sizeof(buf), fmt, args);
	if (g_log) {
		static const char *names[] = {"error", "warning", "info", "debug"};
		const int idx = level <= LOG_ERROR ? 0 : level <= LOG_WARNING ? 1 : level <= LOG_INFO ? 2 : 3;
		// One write per line under a lock. A QTextStream per line connected to and disconnected
		// from the shared QFile on every call, which corrupted the heap when threads logged at once.
		const QByteArray line = "[" + QByteArray::number(g_uptime.elapsed()) + "ms] " + names[idx] + ": " + buf + "\n";
		QMutexLocker lock(&g_logLock);
		if (g_log) {
			g_log->write(line);
			g_log->flush();
		}
	}
}

// The preview in the UI's scene by default; LOBS_NATIVE_PREVIEW=1 asks for the native window.
static bool scenePreviewEnabled() { return qEnvironmentVariableIntValue("LOBS_NATIVE_PREVIEW") != 1; }

// The canvas's keys: nudge, delete, undo, tools... (the native preview window and PreviewItem).
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
	case Qt::Key_BracketRight: // forward (Shift: to the front)
	case Qt::Key_BraceRight:
		if (ctrl) return app->moveSelected(shift ? -1000 : -1), true;
		break;
	case Qt::Key_BracketLeft: // backward (Shift: to the back)
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

/// Native window libobs draws into; forwards input to the App's canvas logic.
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

/// image://thumbs/<sceneId>?<tick> — a small live frame of a scene, rendered by libobs.
class ThumbProvider : public QQuickImageProvider {
public:
	explicit ThumbProvider(Engine *e) : QQuickImageProvider(QQuickImageProvider::Image), m_engine(e) {}
	QImage requestImage(const QString &id, QSize *size, const QSize &req) override
	{
		const QString key = id.section('?', 0, 0);
		const int w = req.width() > 0 ? req.width() : 320;
		// "src:<id>" is one input (Sources panel); anything else is a scene.
		const QImage img = key == "__transition" ? m_engine->renderTransitionPreview(w)
				 : key.startsWith("src:") ? m_engine->renderSourceThumbnail(key.mid(4), w) : m_engine->renderThumbnail(key, w);
		if (size)
			*size = img.size();
		return img;
	}

private:
	Engine *m_engine;
};

// "image://assetthumb/<percent-encoded path>?<mtime>": the Windows shell's thumbnail (images,
// videos), falling back to decoding images directly.
class AssetThumbProvider : public QQuickImageProvider {
public:
	AssetThumbProvider() : QQuickImageProvider(QQuickImageProvider::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading) {}
	QImage requestImage(const QString &id, QSize *size, const QSize &req) override
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
};

/// Dock styling: LOBS's Fluid Functionalism versions of KDDockWidgets' tab bar, title bar,
/// panel group, floating window and separator.
class LobsViewFactory : public KDDockWidgets::QtQuick::ViewFactory {
public:
	QUrl tabbarFilename() const override { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockTabBar.qml"); }
	QUrl titleBarFilename() const override { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockTitleBar.qml"); }
	QUrl groupFilename() const override { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockGroup.qml"); }
	QUrl floatingWindowFilename() const override { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockFloatingWindow.qml"); }
	QUrl separatorFilename() const override { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockSeparator.qml"); }
	QUrl dockwidgetFilename() const override { return QUrl("qrc:/qt/qml/LOBS/qml/dock/DockWidgetBg.qml"); }
};

#ifdef Q_OS_WIN
/// The preview as an item in the UI's own scene: the engine draws each frame into textures shared
/// with Qt's D3D11 device (Engine::startSharedPreview) and this shows the newest. Being part of
/// the scene, menus, dialogs and tooltips draw over the live video; nothing has to hide it.
/// Takes the canvas's mouse and keys, as the native preview window does.
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
		// New frames arrive on the graphics thread; one update per UI frame is enough.
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
	// Both at once: a new size changes the preview scale, which re-lays out the box and resizes
	// this again from inside; separate setWidth/setHeight would then restore the stale height.
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
			return node; // nothing drawn at this size yet: keep the last frame (its texture is kept too)
		if (!node) {
			node = new QSGSimpleTextureNode;
			node->setOwnsTexture(false);
			node->setFiltering(QSGTexture::Nearest); // drawn at exactly this size: 1:1 pixels
		}
		node->setTexture(m_textures[p]);
		m_shown = m_textures[p];
		const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
		node->setRect(QRectF(0, 0, m_texSize.width() / dpr, m_texSize.height() / dpr));
		releaseTextures(m_oldTextures, m_oldD3d); // a frame at the new size is up: the old ones can go
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
		// Last: the new scale re-lays out the box, which can resize this (and sync again) from
		// inside; anything after it would overwrite that newer size with this one.
		m_app->setPreviewSize(QSizeF(pw / dpr, ph / dpr), dpr);
	}
	// Render thread: the engine's textures, opened on Qt's own device (same GPU, or it fails).
	bool openTextures()
	{
		// The set holding the texture on screen is never freed here: it becomes (or stays) the old
		// set, freed once a frame from the new set replaces it. Sizes can change several times
		// before one does (startup, a window drag); freeing the shown one crashed in the renderer.
		const auto holds = [this](QSGTexture *const *set) { return m_shown && std::find(set, set + Engine::kSharedFrames, m_shown) != set + Engine::kSharedFrames; };
		if (holds(m_textures)) {
			releaseTextures(m_oldTextures, m_oldD3d);
			for (int i = 0; i < Engine::kSharedFrames; i++)
				m_oldTextures[i] = m_textures[i], m_oldD3d[i] = m_d3d[i], m_textures[i] = nullptr, m_d3d[i] = nullptr;
		} else {
			releaseTextures(m_textures, m_d3d); // not on screen (the shown one is in the old set, or none is)
			if (!holds(m_oldTextures))
				releaseTextures(m_oldTextures, m_oldD3d), m_shown = nullptr;
		}
		QSGRendererInterface *ri = window() ? window()->rendererInterface() : nullptr;
		if (!ri || ri->graphicsApi() != QSGRendererInterface::Direct3D11)
			return false;
		auto *dev = static_cast<ID3D11Device *>(ri->getResource(window(), QSGRendererInterface::DeviceResource));
		auto *obsDev = static_cast<ID3D11Device *>(m_engine->graphicsDevice());
		if (!dev || !obsDev || adapterLuid(dev) != adapterLuid(obsDev))
			return false; // UI and engine on different GPUs: the native window it is
		const Engine::SharedFrames f = m_engine->sharedPreviewFrames();
		m_gen = f.generation;
		if (!f.handles[0])
			return f.generation == 0; // not made yet: try again on the next frame
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
	// Off the render thread (the window goes away, or this item does): freed on it, later.
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
	static void releaseTextures(QSGTexture **t, ID3D11Texture2D **d) // render thread
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
	QSGTexture *m_shown = nullptr; // what the node draws (render thread): its set must outlive it
	ID3D11Texture2D *m_d3d[Engine::kSharedFrames] = {}, *m_oldD3d[Engine::kSharedFrames] = {};
	QSize m_texSize;
};
#endif

/// Keeps the native preview window glued to the canvas item. The canvas sits inside the dock
/// layout, so it can move without its own x/y changing; we re-check its scene rect every frame.
class PreviewHost : public QObject {
	Q_OBJECT
public:
	PreviewHost(Engine *engine, App *app) : m_engine(engine), m_app(app) {}
	~PreviewHost() override { delete m_native; }

	Q_INVOKABLE void attach(QQuickItem *item)
	{
		if (m_native || m_inScene || !item || !item->window())
			return;
		m_item = item;
#ifdef Q_OS_WIN
		// The preview drawn in the UI's scene, so nothing hides it; the native window is the
		// fallback (another GPU, no D3D11), and LOBS_NATIVE_PREVIEW=1 asks for it.
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

private:
	void attachNative()
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

private:
	void sync()
	{
		if (!m_item || !m_native)
			return;
		const bool show = !m_hidden && m_item->isVisible() && m_item->width() > 4 && m_item->height() > 4;
		if (show != m_native->isVisible())
			m_native->setVisible(show);
		if (!show)
			return; // too small to draw: leave the native window and the engine at their last size
		const QRect r(m_item->mapToScene(QPointF(0, 0)).toPoint(), QSize(int(m_item->width()), int(m_item->height())));
		if (r == m_rect)
			return;
		m_rect = r;
		m_native->setGeometry(r);
		// Engine and canvas mapping work from the same physical size, so their scale math agrees.
		const qreal dpr = m_native->devicePixelRatio();
		const int pw = int(r.width() * dpr), ph = int(r.height() * dpr);
		m_app->setPreviewSize(QSizeF(pw / dpr, ph / dpr), dpr);
		m_engine->setPreviewDpr(dpr);
		m_engine->resizePreview(pw, ph);
	}

	Engine *m_engine;
	App *m_app;
	QQuickItem *m_item = nullptr;
	PreviewWindow *m_native = nullptr;
#ifdef Q_OS_WIN
	PreviewItem *m_inScene = nullptr;
#else
	QQuickItem *m_inScene = nullptr;
#endif
	QRect m_rect;
	bool m_hidden = false;
};

// Builds a small show on the running engine and checks each step, then records a clip.
// Writes {ok, engine, checks:[{name, ok, detail}]}; exit code 0 only if everything passed.
static int runEngineSmokeTest(QGuiApplication &app, Engine &engine, const QString &docPath, const QString &reportPath)
{
	auto *checks = new QJsonArray;
	const auto check = [checks](const QString &name, bool ok, const QString &detail = {}) {
		*checks << QJsonObject{{"name", name}, {"ok", ok}, {"detail", detail}};
		return ok;
	};
	const auto finish = [&app, checks, reportPath](int code) {
		bool all = true;
		for (const QJsonValue c : *checks)
			all &= c.toObject()["ok"].toBool();
		QFile f(reportPath);
		f.open(QIODevice::WriteOnly | QIODevice::Truncate);
		f.write(QJsonDocument(QJsonObject{{"ok", all && code == 0}, {"engine", QString(obs_get_version_string())}, {"checks", *checks}}).toJson());
		f.close();
		app.exit(all && code == 0 ? 0 : 1);
	};
	if (!check("engine starts", engine.started(), engine.status()))
		return finish(1), 1;

	// Recordings go to a temp folder, never the user's Videos.
	AppSettings s = engine.settings();
	s.record.folder = QDir::tempPath() + "/lobs-smoketest";
	s.record.quality = "stream";
	QDir(s.record.folder).removeRecursively();
	QDir().mkpath(s.record.folder);
	engine.setSettings(s);

	const QString html = QDir::tempPath() + "/lobs-smoketest.html";
	QFile h(html);
	h.open(QIODevice::WriteOnly | QIODevice::Truncate);
	h.write("<html><body style='margin:0'><div style='width:300px;height:300px;background:#0000ff'></div></body></html>");
	h.close();

	auto *controller = new App(&engine, docPath, &app);
	QTimer::singleShot(500, &app, [=, &engine]() {
		controller->newDocument();
		controller->addSource("color", "Red", "#ff0000", true);
		controller->fit("fit");
		controller->addFilter("color");
		controller->addSource("text", "Text", "LOBS", true);
		controller->addSource("browser", "Page", QUrl::fromLocalFile(html).toString(), true);
		controller->addSource("desktop_audio", "Desktop", "", false);
		const lobs::Doc &d = controller->doc();
		for (const QString &id : d.sourceOrder)
			if (d.sources[id].kind == "desktop_audio")
				controller->addAudioFilter(id, "gain");
		controller->take();
		check("scene with color, text, browser and audio sources", d.instances.size() >= 3 && d.sources.size() >= 4);
	});
	QTimer::singleShot(6000, &app, [=, &engine]() {
		const QJsonObject f = engine.inputFilters();
		bool audioFx = false;
		for (const QJsonValue v : f)
			for (const QJsonValue id : v.toArray())
				audioFx |= id.toString().startsWith("gain_filter");
		// macOS captures desktop audio only with Screen Recording allowed (permissions.h).
		if (Permissions::allows("desktop_audio"))
			check("audio effect attached", audioFx);
		else
			check("audio effect attached", true, "skipped: Screen Recording isn't allowed");
		int modules = 0;
		obs_enum_modules([](void *p, obs_module_t *) { ++*static_cast<int *>(p); }, &modules);
		check("engine modules load", modules >= 10, QString("%1 modules").arg(modules));
		const QImage img = engine.renderThumbnail(engine.programScene(), 320);
		const QColor corner = img.isNull() ? QColor() : img.pixelColor(4, img.height() - 4);
		check("frame renders", !img.isNull() && img.width() == 320, img.isNull() ? "no image" : QString("%1x%2").arg(img.width()).arg(img.height()));
		check("colors are right", corner.red() > 150 && corner.green() < 90 && corner.blue() < 90, corner.name());
		const QColor page = img.isNull() ? QColor() : img.pixelColor(10, 10);
		check("browser source renders", page.blue() > 150 && page.red() < 90, page.name());
		// Mirroring: flip the browser layer; its blue square moves from the left to the right.
		for (const lobs::Instance &i : controller->doc().instances)
			if (controller->doc().sources.value(i.sourceId).kind == "browser")
				controller->selectLayer(i.id, false);
		controller->setTransform("flipH", 1);
	});
	QTimer::singleShot(7000, &app, [=, &engine, &app]() {
		const QImage img = engine.renderThumbnail(engine.programScene(), 320);
		const QColor left = img.isNull() ? QColor() : img.pixelColor(10, 10), right = img.isNull() ? QColor() : img.pixelColor(309, 10);
		check("mirroring", right.blue() > 150 && left.blue() < 90, left.name() + " / " + right.name());
		// Transitions: a 1 s fade to a new scene is mid-way after 0.3 s and done after 1.5 s.
		// The show's own transition (transitions live in the show, not the app settings).
		const QString fade = controller->addTransition("fade");
		controller->updateTransition(fade, "durationMs", 1000);
		controller->setCurrentTransition(fade);
		controller->addScene();
		controller->take();
		// Timed from the take, not from the start: the steps before it run at the GPU's pace.
		QTimer::singleShot(300, &app, [=, &app]() {
			obs_source_t *out = obs_get_output_source(0);
			const float t = out ? obs_transition_get_time(out) : -1;
			check("transition plays", out && obs_source_get_type(out) == OBS_SOURCE_TYPE_TRANSITION && t > 0.05f && t < 0.95f, QString("t=%1").arg(t));
			obs_source_release(out);
		});
		QTimer::singleShot(1600, &app, [=, &engine]() {
			obs_source_t *out = obs_get_output_source(0);
			obs_source_t *active = out ? obs_transition_get_active_source(out) : nullptr;
			check("transition lands on the new scene", active && QString(obs_source_get_name(active)) == engine.programScene());
			obs_source_release(active);
			obs_source_release(out);
			engine.recordClip(2);
		});
	});
	QObject::connect(&engine, &Engine::recordingFinished, &app, [=](bool ok, const QString &path, qint64 bytes) {
		check("records a clip", ok && bytes > 10000, QString("%1 bytes").arg(bytes));
		QFile::remove(path);
		QDir(QFileInfo(path).absolutePath()).removeRecursively();
		QFile::remove(html);
		finish(0);
	});
	QTimer::singleShot(60000, &app, [=]() { check("finishes in time", false); finish(1); });
	return app.exec();
}

/// Shown the moment LOBS starts, until the main window draws its first frame: the logo and
/// the step startup is on. A plain raster window, so it needs no QML and shows in milliseconds.
class Splash : public QRasterWindow {
public:
	explicit Splash(const QString &iconDir)
	{
		setFlags(Qt::SplashScreen | Qt::FramelessWindowHint);
		resize(360, 200);
		if (QScreen *s = QGuiApplication::primaryScreen())
			setPosition(s->availableGeometry().center() - QPoint(width() / 2, height() / 2));
		m_logo.load(iconDir + "/lobs-128.png"); // drawn at 64: sharp on high-DPI screens
		show();
		step("Starting");
	}
	void step(const QString &text)
	{
		m_text = text;
		update();
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents); // paint now: the next step blocks
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
		p.fillRect(QRect(QPoint(), size()), QColor("#1e1e1e"));
		p.setPen(QColor("#1fffffff"));
		p.drawRect(QRect(QPoint(), size()).adjusted(0, 0, -1, -1));
		p.drawImage(QRect(width() / 2 - 32, 44, 64, 64), m_logo);
		QFont f = QGuiApplication::font();
		f.setPixelSize(20);
		f.setWeight(QFont::DemiBold);
		p.setFont(f);
		p.setPen(QColor("#f5f5f5"));
		p.drawText(QRect(0, 116, width(), 28), Qt::AlignCenter, "LOBS");
		f.setPixelSize(12);
		f.setWeight(QFont::Normal);
		p.setFont(f);
		p.setPen(QColor("#a3a3a3"));
		p.drawText(QRect(0, 150, width(), 20), Qt::AlignCenter, m_text + QString::fromUtf8("…"));
	}

private:
	QImage m_logo;
	QString m_text;
};

int main(int argc, char *argv[])
{
	g_uptime.start();
#ifndef Q_OS_WIN // on Windows obs.dll can't load yet: the engine folder is picked below
	// The browser source passes these to CEF on macOS, as OBS does (helper processes read them).
	obs_set_cmdline_args(argc, argv);
#endif
#ifdef Q_OS_WIN
	// The preview drawn in the UI scene (PreviewItem) crashes Qt's threaded render loop now and
	// then (in QRhi::beginFrame); on the GUI thread's loop it doesn't, and costs ~0.6 ms a frame
	// there. Set before the app exists.
	if (scenePreviewEnabled() && !qEnvironmentVariableIsSet("QSG_RENDER_LOOP"))
		qputenv("QSG_RENDER_LOOP", "basic");
#endif
	QGuiApplication app(argc, argv);
	app.setOrganizationName("LOBS");
	app.setApplicationName("LOBS");
#ifdef LOBS_TESTS
	const int scenarioAt = int(app.arguments().indexOf("--scenariotest"));
	const bool scenario = scenarioAt > 0 && scenarioAt + 2 < app.arguments().size();
	if (scenario)
		lobs_test::isolateProfile(app.arguments()[scenarioAt + 2]);
#endif

	// --render-svg <in.svg> <out.png> <size>: rasterize an SVG (used to build the app icon).
	if (const int at = int(app.arguments().indexOf("--render-svg")); at > 0 && at + 3 < app.arguments().size()) {
		const int size = app.arguments()[at + 3].toInt();
		QSvgRenderer r(app.arguments()[at + 1]);
		QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
		img.fill(Qt::transparent);
		QPainter p(&img);
		p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
		r.render(&p);
		p.end();
		return img.save(app.arguments()[at + 2]) ? 0 : 1;
	}
#ifdef Q_OS_WIN
	// --install-engine <obs release zip> <version>: unpack an official OBS release as an
	// engine folder (used by the build and the installer), then exit. Runs before libobs
	// is loaded, so it works with no engine installed yet.
	if (const int at = int(app.arguments().indexOf("--install-engine")); at > 0 && at + 2 < app.arguments().size()) {
		QString note;
		const QString ver = app.arguments()[at + 2];
		const QString err = installEngineZip(app.arguments()[at + 1], ver, &note);
		if (err.isEmpty() && engine_store::select().version.isEmpty())
			engine_store::activate(ver, false);
		else if (err.isEmpty() && engine_store::select().version != ver)
			engine_store::activate(ver, false);
		QFile out(QCoreApplication::applicationDirPath() + "/engine/install.log");
		out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
		out.write((err.isEmpty() ? "installed " + ver + (note.isEmpty() ? "" : "\n" + note) : "error: " + err).toUtf8());
		return err.isEmpty() ? 0 : 2;
	}

	// The OBS engine lives in <app>/engine/<version>. obs.dll is delay-loaded, so pointing
	// the DLL search at the chosen engine here decides which libobs this process uses.
	engine_store::Selected sel;
	if (const int ue = int(app.arguments().indexOf("--use-engine")); ue > 0 && ue + 1 < app.arguments().size())
		sel = {app.arguments()[ue + 1], engine_store::enginesDir() + "/" + app.arguments()[ue + 1]};
	else {
		engine_store::adoptBundled(LOBS_ENGINE_VERSION);
		sel = engine_store::select();
	}
	if (!QFile::exists(sel.root + "/bin/64bit/obs.dll"))
		sel.version.clear();
	if (sel.version.isEmpty()) {
		platform::fatalError("LOBS couldn't find its OBS engine. Reinstall LOBS to restore it.");
		return 3;
	}
	// Start the RTWQ work-queue platform for the whole run, as OBS's own main() does. win-wasapi
	// uses RTWQ whenever RTWorkQ.dll is already loaded when a source is created; without our
	// reference, the DLL only gets loaded (and started) later by a Media Foundation user, such as
	// CEF for a browser source, and once that shuts RTWQ down, RTWQ sources never run their work
	// items again, so destroying one at exit waits forever (LOBS-037).
	const HMODULE rtwq = LoadLibraryW(L"RTWorkQ.dll");
	if (rtwq)
		if (auto startup = reinterpret_cast<HRESULT(STDAPICALLTYPE *)()>(GetProcAddress(rtwq, "RtwqStartup")))
			startup();
	const QString engineBin = sel.root + "/bin/64bit";
	SetDllDirectoryW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(engineBin).utf16()));
	// libobs starts its helper programs (recording muxer, encoder probes) from the folder of
	// the running exe, so mirror the engine's helpers next to LOBS.exe; on PATH, the engine's
	// bin lets them (and anything they start) find the engine's DLLs.
	qputenv("PATH", QDir::toNativeSeparators(engineBin).toLocal8Bit() + ";" + qgetenv("PATH"));
	for (const QFileInfo &fi : QDir(engineBin).entryInfoList({"*.exe"}, QDir::Files)) {
		const QString dest = QCoreApplication::applicationDirPath() + "/" + fi.fileName();
		const QFileInfo d(dest);
		if (!d.exists() || d.size() != fi.size() || d.lastModified() != fi.lastModified()) {
			QFile::remove(dest);
			QFile::copy(fi.filePath(), dest); // copy keeps the timestamp, so this runs once per engine
		}
	}
#else
	// macOS: the engine is part of LOBS.app (Contents/Frameworks and Contents/PlugIns), where
	// libobs finds its modules and data on its own.
	const engine_store::Selected sel = engine_store::select();
	if (sel.version.isEmpty()) {
		platform::fatalError("LOBS couldn't find its OBS engine. Reinstall LOBS to restore it.");
		return 3;
	}
#endif
	QQuickStyle::setStyle("Basic");
	// One font family (Inter Variable), bundled next to the app.
	QString fontFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family(); // if Inter is missing
	const int fid = QFontDatabase::addApplicationFont(platform::resourceDir() + "/fonts/InterVariable.ttf");
	if (fid >= 0 && !QFontDatabase::applicationFontFamilies(fid).isEmpty())
		fontFamily = QFontDatabase::applicationFontFamilies(fid).first();
	QFont uiFont(fontFamily);
	uiFont.setPixelSize(12);
	QGuiApplication::setFont(uiFont);
	QPalette pal;
	pal.setColor(QPalette::Window, QColor("#2c2c2c"));
	pal.setColor(QPalette::WindowText, QColor("#f5f5f5"));
	pal.setColor(QPalette::Base, QColor("#171717"));
	pal.setColor(QPalette::AlternateBase, QColor("#333333"));
	pal.setColor(QPalette::Text, QColor("#f5f5f5"));
	pal.setColor(QPalette::Button, QColor("#333333"));
	pal.setColor(QPalette::ButtonText, QColor("#f5f5f5"));
	pal.setColor(QPalette::Highlight, QColor("#6b97ff"));
	pal.setColor(QPalette::HighlightedText, Qt::white);
	pal.setColor(QPalette::Mid, QColor("#414141"));
	pal.setColor(QPalette::Dark, QColor("#101010"));
	pal.setColor(QPalette::Light, QColor("#3a3a3a"));
	pal.setColor(QPalette::Midlight, QColor("#3a3a3a"));
	pal.setColor(QPalette::PlaceholderText, QColor("#a3a3a3"));
	pal.setColor(QPalette::ToolTipBase, QColor("#333333"));
	pal.setColor(QPalette::ToolTipText, QColor("#f5f5f5"));
	QGuiApplication::setPalette(pal);

	// App files (fonts, icons) and what LOBS writes (logs, test output) sit next to LOBS.exe; on
	// macOS in LOBS.app/Contents/Resources and Application Support (see platform.h).
	const QString root = platform::resourceDir(), appOut = platform::outputDir();
#ifdef Q_OS_WIN
	// libobs finds its plugins relative to the working directory (../../obs-plugins/64bit), so
	// that's the engine's bin.
	QDir::setCurrent(engineBin);
#endif

	// Window and taskbar icon (the exe's own icon comes from resources/lobs.rc).
	{
		QIcon icon;
		for (int s : {16, 20, 24, 32, 40, 48, 64, 128, 256})
			icon.addFile(root + QString("/icons/lobs-%1.png").arg(s), QSize(s, s));
		QGuiApplication::setWindowIcon(icon);
	}
	// No splash for headless runs (self-tests, smoke tests): nobody's waiting on them.
	std::unique_ptr<Splash> splash;
	if (!isSelfTestRun(app.arguments()) && !app.arguments().contains("--engine-smoketest"))
		splash = std::make_unique<Splash>(root + "/icons");
	QFile log(appOut + "/lobs.log");
	log.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
	g_log = &log;
	base_set_log_handler(logHandler, nullptr);
	const QString crashDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/crashes";
	crash_report::install(crashDir, appOut + "/lobs.log");

	blog(LOG_INFO, "[lobs] startup: app setup done");
	if (splash)
		splash->step("Starting the video engine");
	Engine engine;
	qmlRegisterType<BrowserView>("LOBS.Web", 1, 0, "BrowserView");
	qmlRegisterType<SourcePreview>("LOBS.Web", 1, 0, "SourcePreview");
	qmlRegisterType<RowsModel>("LOBS.Models", 1, 0, "RowsModel");
	engine.setSettings(AppSettings::load());
	blog(LOG_INFO, "[lobs] OBS engine %s from %s", qPrintable(sel.version), qPrintable(sel.root));
	if (!engine.start(sel.root))
		qWarning("engine failed: %s", qPrintable(engine.status()));
	else {
		engine.resetVideo(); // apply saved output size and FPS
		BrowserView::setEngineReady(true);
		SourcePreview::setEngine(&engine);
	}
	blog(LOG_INFO, "[lobs] startup: engine up");
	if (splash)
		splash->step("Loading your show");
	crash_report::install(crashDir, appOut + "/lobs.log"); // again: the engine's modules may have set their own
	EngineUpdater engineUpdater(sel);

	// Self-tests work on a throwaway document, so they never touch the user's show.
	const bool selfTest = isSelfTestRun(app.arguments());
	const QString docPath = selfTest ? QDir::tempPath() + "/lobs-selftest.lobs.json"
					 : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/current.lobs.json";
	if (selfTest) {
		QFile::remove(docPath);
		QFile::remove(QDir::tempPath() + "/layout-v2.json"); // panels from an earlier test run would carry over
		if (const QString l = qEnvironmentVariable("LOBS_QA_LAYOUT"); !l.isEmpty()) // reproduce a user's panel layout
			QFile::copy(l, QDir::tempPath() + "/layout-v2.json");
		AppSettings::defaultRecordFolderOverride() = QDir::tempPath() + "/lobs-selftest-recordings"; // LOBS-003
	}

	// --engine-smoketest <report.json>: headless check that this engine works with LOBS.
	if (const int st = int(app.arguments().indexOf("--engine-smoketest")); st > 0 && st + 1 < app.arguments().size())
		return runEngineSmokeTest(app, engine, docPath, app.arguments()[st + 1]);
	App controller(&engine, docPath);
	blog(LOG_INFO, "[lobs] startup: show loaded");
	// macOS privacy permissions never hold up startup: what isn't allowed yet isn't created,
	// the permissions bar says so, and a grant applies the show again (see permissions.h).
	Permissions permissions;
	if (qEnvironmentVariableIsSet("LOBS_DEBUG_KEYS"))
		platform::debugKeyEvents();
	const auto syncNeeded = [&] {
		QStringList kinds;
		for (const QString &id : controller.doc().sourceOrder)
			kinds << controller.doc().sources[id].kind;
		permissions.setNeeded(kinds);
	};
	syncNeeded();
	QObject::connect(&controller, &App::docChanged, &permissions, syncNeeded);
	const auto syncHotkeys = [&] {
		bool bound = false;
		for (const QVariant &v : engine.settings().hotkeys)
			bound |= !v.toString().isEmpty();
		permissions.setHotkeysNeeded(bound);
	};
	syncHotkeys();
	QObject::connect(&controller, &App::settingsChanged, &permissions, syncHotkeys);
	QObject::connect(&permissions, &Permissions::granted, &controller, [&] {
		controller.fillMissingDevices();
		engine.apply(controller.doc());
	});
	if (splash)
		splash->step("Opening the workspace");

	KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtQuick);
	auto &dockConfig = KDDockWidgets::Config::self();
	dockConfig.setFlags(KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible | KDDockWidgets::Config::Flag_AlwaysShowTabs |
			    KDDockWidgets::Config::Flag_AllowReorderTabs | KDDockWidgets::Config::Flag_CloseOnlyCurrentTab);
	dockConfig.setSeparatorThickness(6);
	// While dragging, the panel is a faint ghost so the layout and the blue landing preview
	// stay in view, rather than a solid window following the cursor.
	dockConfig.setDraggedWindowOpacity(0.35);
	dockConfig.setTransparencyOnlyOverDropIndicator(false);
	dockConfig.setViewFactory(new LobsViewFactory());

	// Objects QML uses outlive the QML engine (declared first, destroyed after it): panels still
	// read them while the windows close.
	WindowChrome chrome;
	Captioner captions(&engine);
	CrashReport crashReport(crashDir);
	DockTabs dockTabs;
	if (selfTest) // a self-test never waits on the crash prompt (it covered every later test's window, LOBS-041);
		crashReport.dismiss(); // run.ps1 reports crashes, and the dump stays in the folder
	AppUpdater appUpdater(app.arguments().contains("--updated"));
	QQmlApplicationEngine qml;
	KDDockWidgets::QtQuick::Platform::instance()->setQmlEngine(&qml);
	PreviewHost previewHost(&engine, &controller);
	qml.rootContext()->setContextProperty("previewHost", &previewHost);
	qml.rootContext()->setContextProperty("app", &controller);
	qml.rootContext()->setContextProperty("engineUpdater", &engineUpdater);
	// Fluid Functionalism tokens (dark), shared by every QML file as `Pal.<name>`:
	// the 8-step surface ladder, 2 text colors, 12% borders, +6%/+10% hover/selected,
	// 3 springs (fast/moderate/slow, ms), the compact size step and the 5-style type scale.
	// A property map, not a QVariantMap: QML reads one property instead of converting the whole
	// map to a new JS object on every Pal.x access (thousands per list rebuild).
	auto *palMap = new QQmlPropertyMap(&qml);
	{
		const QVariantMap values{
		{"s1", "#171717"}, {"s2", "#1e1e1e"}, {"s3", "#252525"}, {"s4", "#2c2c2c"},
		{"s5", "#333333"}, {"s6", "#3a3a3a"}, {"s7", "#414141"}, {"s8", "#484848"},
		{"fg", "#f5f5f5"}, {"muted", "#a3a3a3"},
		{"border", "#1fffffff"}, {"borderStrong", "#33ffffff"}, {"hover", "#0fffffff"}, {"selected", "#1affffff"},
		{"focus", "#6b97ff"}, {"link", "#7aa7ff"}, {"danger", "#fca5a5"}, {"dangerBg", "#450a0a"}, {"live", "#ef4444"},
		{"fast", 80}, {"moderate", 160}, {"slow", 240},
		{"ctl", 28}, {"radius", 8}, {"font", fontFamily}, {"mono", lobs::monoFont()},
		{"display", 22}, {"title", 14}, {"subtitle", 13}, {"body", 12}, {"caption", 11}};
		for (auto it = values.cbegin(); it != values.cend(); ++it)
			palMap->insert(it.key(), it.value());
	}
	qml.rootContext()->setContextProperty("Pal", palMap);
	qml.rootContext()->setContextProperty("captions", &captions);
	qml.rootContext()->setContextProperty("chrome", &chrome);
	qml.rootContext()->setContextProperty("crashReport", &crashReport);
	qml.rootContext()->setContextProperty("dockTabs", &dockTabs);
	qml.rootContext()->setContextProperty("permissions", &permissions);
	qml.rootContext()->setContextProperty("appUpdater", &appUpdater);
	qml.addImageProvider("thumbs", new ThumbProvider(&engine));
	qml.addImageProvider("assetthumb", new AssetThumbProvider);
	qml.addImageProvider("icon", new IconProvider(root + "/icons"));
	qml.loadFromModule("LOBS", "Main");
	if (qml.rootObjects().isEmpty())
		return 1;
	blog(LOG_INFO, "[lobs] startup: UI built");
	if (auto *mainWindow = qobject_cast<QQuickWindow *>(qml.rootObjects().first())) {
		QObject::connect(mainWindow, &QQuickWindow::frameSwapped, mainWindow, [&splash] {
			blog(LOG_INFO, "[lobs] startup: first frame");
			QTimer::singleShot(0, [&splash] { splash.reset(); });
		}, Qt::SingleShotConnection);
		chrome.attach(mainWindow); // the top bar is the title bar
	}
#ifdef LOBS_TESTS
	if (scenario)
		lobs_test::startScenario(app, controller, engine, qml, app.arguments()[scenarioAt + 1], app.arguments()[scenarioAt + 2]);
#endif
#ifdef LOBS_TESTS
	if (qEnvironmentVariableIsSet("LOBS_CRASH_TEST")) // checks the crash report end to end
		QTimer::singleShot(3000, [] { *static_cast<volatile int *>(nullptr) = 1; });
#endif
	// The UI is up on this engine: it's good (ends a new engine's trial), then look for
	// LOBS updates at most once a day, a little after startup. The OBS engine updates with
	// LOBS (each release ships the engine it was tested with), so it isn't checked on its own.
	QTimer::singleShot(5000, [&]() {
		if (engine.started())
			engine_store::markGood(sel.version);
		if (!selfTest) {
			appUpdater.checkIfDue();
			permissions.askForNeeded(); // with the window up, so the prompt has context
		}
	});
	// System Settings' "Quit & Reopen" quits LOBS but doesn't start it again (Qt finishes quitting on
	// its own time, which macOS takes for a refusal). A quit that comes while LOBS is in the
	// background, right after such a permission was turned on, starts it again; ⌘Q in LOBS doesn't.
	QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&] {
		if (!selfTest && QGuiApplication::applicationState() != Qt::ApplicationActive && permissions.grantedNeedingRelaunch()) {
			blog(LOG_INFO, "[lobs] quit from outside after a permission change: reopening");
			platform::relaunchAfterExit();
		}
	});
	QObject::connect(&permissions, &Permissions::relaunchRequested, &app, [&] {
		platform::relaunchAfterExit();
		if (auto *w = qobject_cast<QQuickWindow *>(qml.rootObjects().value(0)))
			w->close(); // as on quit: the show and layout are saved
		else
			QCoreApplication::quit();
	});
	QObject::connect(&appUpdater, &AppUpdater::quitForInstall, &app, [&] {
		if (auto *w = qobject_cast<QQuickWindow *>(qml.rootObjects().value(0)))
			w->close();
		else
			QCoreApplication::quit();
	});
	// Self-test of the whole update path against a local feed: check, download, verify, install
	// (LOBS_UPDATE_AUTO=check: only check).
	if (qEnvironmentVariableIsSet("LOBS_UPDATE_FEED") && qEnvironmentVariableIsSet("LOBS_UPDATE_AUTO") && !appUpdater.justUpdated()) {
		QObject::connect(&appUpdater, &AppUpdater::changed, &app, [&] {
			if (qEnvironmentVariable("LOBS_UPDATE_AUTO") == "check") // stop at the notice, to look at it
				return;
			if (appUpdater.state() == "available" && appUpdater.message().isEmpty())
				appUpdater.download();
			else if (appUpdater.state() == "ready")
				QTimer::singleShot(1000, &appUpdater, &AppUpdater::installAndRestart);
		});
		QTimer::singleShot(3000, &appUpdater, &AppUpdater::check);
	}

	// --autotest: exercise the editor through the controller, record, write results, quit.
	if (app.arguments().contains("--autotest")) {
		QTimer::singleShot(3000, [&]() {
			controller.newDocument();
			controller.addSource("color", "Backdrop", "#203040", true);
			controller.addSource("camera", "Facecam", "", true);
			controller.fit("fit");
			controller.addFilter("color");
			controller.addScene();
			controller.placeSource(controller.doc().sourceOrder.value(1));
			controller.setCrop("l", 400);
			controller.take();
			engine.recordClip(5);
		});
		QObject::connect(&engine, &Engine::recordingFinished, [&](bool ok, const QString &path, qint64 bytes) {
			const lobs::Doc &d = controller.doc();
			QJsonObject r{{"ok", ok}, {"path", path}, {"bytes", bytes}, {"stats", engine.stats()}, {"scenes", d.scenes.size()},
				      {"sources", d.sources.size()}, {"instances", d.instances.size()}, {"canUndo", controller.canUndo()}};
			QFile f(appOut + "/autotest.json");
			f.open(QIODevice::WriteOnly | QIODevice::Truncate);
			f.write(QJsonDocument(r).toJson());
			f.close();
			QTimer::singleShot(1000, &app, &QGuiApplication::quit);
		});
	}

	// --snapshot <file.png>: save the program scene at full canvas size, then quit.
	if (const int at = int(app.arguments().indexOf("--snapshot")); at > 0 && at + 1 < app.arguments().size()) {
		const QString out = app.arguments()[at + 1];
		QTimer::singleShot(4000, [&, out]() {
			engine.renderThumbnail(engine.programScene(), engine.canvasW()).save(out);
			QTimer::singleShot(500, &app, &QGuiApplication::quit);
		});
	}

	// --browsertest <url> <file.png>: show a browser source full-canvas, snapshot program, quit.
	if (const int at = int(app.arguments().indexOf("--browsertest")); at > 0 && at + 2 < app.arguments().size()) {
		const QString url = app.arguments()[at + 1], out = app.arguments()[at + 2];
		QTimer::singleShot(3000, [&, url]() {
			controller.newDocument();
			controller.addSource("browser", "Page", url, true);
			controller.fit("fit");
			controller.take();
		});
		QTimer::singleShot(9000, [&, out]() {
			engine.renderThumbnail(engine.programScene(), engine.canvasW()).save(out);
			QTimer::singleShot(500, &app, &QGuiApplication::quit);
		});
	}

	// --importtest <obs.json> <out dir>: import an OBS collection, then save the resulting
	// document and a snapshot of every scene, and quit.
	if (const int at = int(app.arguments().indexOf("--importtest")); at > 0 && at + 2 < app.arguments().size()) {
		const QString in = app.arguments()[at + 1], outDir = app.arguments()[at + 2];
		QTimer::singleShot(3000, [&, in, outDir]() {
			controller.importObs(in);
			QDir().mkpath(outDir);
			QFile n(outDir + "/notes.txt");
			n.open(QIODevice::WriteOnly | QIODevice::Truncate);
			n.write(controller.toast().toUtf8());
		});
		QTimer::singleShot(12000, [&]() { engine.setAllScenesShowing(true); }); // captures only run while showing
		QTimer::singleShot(15000, [&, outDir]() {
			QDir().mkpath(outDir);
			QFile f(outDir + "/imported.lobs.json");
			f.open(QIODevice::WriteOnly | QIODevice::Truncate);
			f.write(QJsonDocument(controller.doc().toJson()).toJson());
			f.close();
			QFile ff(outDir + "/filters.json");
			ff.open(QIODevice::WriteOnly | QIODevice::Truncate);
			ff.write(QJsonDocument(engine.inputFilters()).toJson());
			ff.close();
			int n = 0;
			for (const QString &sid : controller.doc().sceneOrder)
				engine.renderThumbnail(sid, 960).save(QString("%1/scene%2.png").arg(outDir).arg(++n));
			QTimer::singleShot(500, &app, &QGuiApplication::quit);
		});
	}

	// --enginetest check|install <zip> <version> <out.json>: exercise the engine updater
	// (a check against GitHub, or installing a local OBS zip as an update), write its state, quit.
	if (const int at = int(app.arguments().indexOf("--enginetest")); at > 0 && at + 2 < app.arguments().size()) {
		const QStringList a = app.arguments().mid(at + 1);
		const QString out = a.last();
		if (a[0] == "install" && a.size() >= 4)
			QTimer::singleShot(1000, [&, a]() { engineUpdater.installLocal(a[1], a[2]); });
		else
			QTimer::singleShot(1000, [&]() { engineUpdater.check(); });
		QTimer::singleShot(a[0] == "install" ? 150000 : 10000, [&, out]() {
			QFile f(out);
			f.open(QIODevice::WriteOnly | QIODevice::Truncate);
			f.write(QJsonDocument(QJsonObject{{"state", engineUpdater.state()}, {"message", engineUpdater.message()}, {"installed", engineUpdater.installedVersion()},
							  {"latest", engineUpdater.latest()}, {"previous", engineUpdater.previous()}}).toJson());
			f.close();
			QTimer::singleShot(200, &app, &QGuiApplication::quit);
		});
	}

	// --featuretest <report.json>: replay buffer, a hotkey through libobs, push-to-talk, the
	// virtual camera and live stats. Settings stay in memory and files go to a temp folder.
	if (const int ft = int(app.arguments().indexOf("--featuretest")); ft > 0 && ft + 1 < app.arguments().size()) {
		const QString report = app.arguments()[ft + 1];
		auto *checks = new QJsonArray;
		auto check = [checks](const QString &name, bool ok, const QString &detail = {}) { *checks << QJsonObject{{"name", name}, {"ok", ok}, {"detail", detail}}; };
		AppSettings ts = engine.settings();
		ts.record.folder = QDir::tempPath() + "/lobs-featuretest";
		ts.record.replaySeconds = 5;
		QDir(ts.record.folder).removeRecursively();
		QDir().mkpath(ts.record.folder);
		engine.setSettings(ts);
		auto *hk = new Hotkeys([checks](const QString &a, bool pressed) {
			if (pressed)
				*checks << QJsonObject{{"name", "hotkey fires its action"}, {"ok", a == "stream"}, {"detail", a}};
		}, &app);
		QTimer::singleShot(2000, &app, [=, &engine, &controller]() {
			controller.newDocument();
			controller.addSource("color", "Bg", "#336699", true);
			controller.addSource("desktop_audio", "Desktop", "", false);
			check("replay buffer starts", engine.startReplay(), engine.lastError());
			hk->apply({{"stream", "Ctrl+Shift+F9"}}, {"stream"});
			obs_key_combination_t combo{INTERACT_CONTROL_KEY | INTERACT_SHIFT_KEY, OBS_KEY_F9};
			obs_hotkey_inject_event(combo, true);
			obs_hotkey_inject_event(combo, false);
			QString desk;
			for (const QString &id : controller.doc().sourceOrder)
				if (controller.doc().sources[id].kind == "desktop_audio")
					desk = id;
			engine.setPushToTalk({desk});
			obs_source_t *in = obs_get_source_by_name(desk.toUtf8().constData());
			const bool mutedIdle = in && obs_source_muted(in);
			engine.pushToTalk(desk, true);
			const bool liveHeld = in && !obs_source_muted(in);
			engine.pushToTalk(desk, false);
			check("push-to-talk mutes until held", mutedIdle && liveHeld && in && obs_source_muted(in));
			obs_source_release(in);
			engine.setPushToTalk({});
			// Advanced audio reaches libobs: mono flag, balance, sync offset.
			controller.setAudio(desk, "mono", true, true);
			controller.setAudio(desk, "balance", 0.25, true);
			controller.setAudio(desk, "sync", 120, true);
			if (obs_source_t *a = obs_get_source_by_name(desk.toUtf8().constData())) {
				const bool mono = obs_source_get_flags(a) & OBS_SOURCE_FLAG_FORCE_MONO;
				const float bal = obs_source_get_balance_value(a);
				const int64_t sync = obs_source_get_sync_offset(a);
				check("mono, balance and sync offset apply", mono && std::abs(bal - 0.25f) < 0.001f && sync == 120000000,
				      QString("mono=%1 balance=%2 sync=%3ms").arg(mono).arg(bal).arg(sync / 1000000));
				obs_source_release(a);
			}
			if (engine.vcamAvailable()) {
				const bool on = engine.startVirtualCam();
				check("virtual camera starts", on && engine.vcamActive(), engine.lastError());
				engine.stopVirtualCam();
			} else {
				check("virtual camera driver registered", false, "not registered on this PC");
			}
		});
		QTimer::singleShot(9000, &app, [=, &engine]() {
			const QVariantMap st = engine.liveStats();
			check("live stats", st.contains("cpu") && st.value("fps").toDouble() > 1 && st.contains("diskFreeGb"),
			      QString("%1 fps, cpu %2%").arg(st.value("fps").toDouble(), 0, 'f', 0).arg(st.value("cpu").toDouble(), 0, 'f', 1));
			engine.saveReplay();
		});
		QObject::connect(&engine, &Engine::replaySaved, &app, [=, &engine, &app](const QString &path) {
			const QFileInfo fi(path);
			check("replay saves a file", fi.exists() && fi.size() > 10000, QString("%1 bytes").arg(fi.size()));
			engine.stopReplay();
			bool all = true;
			for (const QJsonValue c : *checks)
				all &= c.toObject()["ok"].toBool();
			QFile f(report);
			f.open(QIODevice::WriteOnly | QIODevice::Truncate);
			f.write(QJsonDocument(QJsonObject{{"ok", all}, {"checks", *checks}}).toJson());
			f.close();
			QTimer::singleShot(1500, &app, [&app]() {
				QDir(QDir::tempPath() + "/lobs-featuretest").removeRecursively();
				app.quit();
			});
		});
	}

	// --assettest <report.json>: Asset Library on a throwaway project (temp folder): import a PNG
	// (twice: no duplicate), place it, delete the layer (its source goes, the file stays), trash it.
	if (const int at = int(app.arguments().indexOf("--assettest")); at > 0 && at + 1 < app.arguments().size()) {
		const QString report = app.arguments()[at + 1];
		QTimer::singleShot(2000, &app, [&, report]() {
			QJsonArray checks;
			auto check = [&](const QString &name, bool ok, const QString &detail = {}) { checks << QJsonObject{{"name", name}, {"ok", ok}, {"detail", detail}}; };
			controller.newDocument();
			const QString png = QDir::tempPath() + "/lobs-asset-test.png";
			QImage img(400, 200, QImage::Format_ARGB32);
			img.fill(QColor("#ff00ff"));
			img.save(png);
			check("assets folder is next to the show", controller.assetsFolder().startsWith(QDir::cleanPath(QDir::tempPath())), controller.assetsFolder());
			const int before = int(controller.assets().size());
			controller.importAssets({QUrl::fromLocalFile(png)});
			controller.importAssets({QUrl::fromLocalFile(png)});
			const QVariantList list = controller.assets();
			check("import copies the file once", list.size() == before + 1, QString("%1 -> %2").arg(before).arg(list.size()));
			QString copied;
			for (const QVariant &v : list)
				if (v.toMap()["name"].toString().startsWith("lobs-asset-test"))
					copied = v.toMap()["path"].toString();
			check("copy lives in the assets folder", !copied.isEmpty() && QFile::exists(copied), copied);
			controller.placeAsset(copied);
			const lobs::Doc &d = controller.doc();
			bool placed = false;
			for (const lobs::Instance &i : d.instances)
				placed |= d.sources.value(i.sourceId).settings.value("file").toString() == copied && d.transform(i).w == 400;
			check("placing makes a layer at the file's size", placed);
			bool hidden = true;
			for (const QVariant &v : controller.pool())
				hidden &= !QStringList{"image", "media", "text", "color"}.contains(v.toMap()["kind"].toString());
			check("Sources panel lists inputs only", hidden);
			controller.deleteSelected();
			bool gone = true;
			for (const lobs::Source &s : controller.doc().sources)
				gone &= s.settings.value("file").toString() != copied;
			check("last layer deleted: its source goes, the file stays", gone && QFile::exists(copied));
			controller.trashAsset(copied);
			check("trash moves it out of assets", !QFile::exists(copied));
			QFile::remove(png);
			bool all = true;
			for (const QJsonValue c : checks)
				all &= c.toObject()["ok"].toBool();
			QFile f(report);
			f.open(QIODevice::WriteOnly | QIODevice::Truncate);
			f.write(QJsonDocument(QJsonObject{{"ok", all}, {"checks", checks}}).toJson());
			f.close();
			QTimer::singleShot(300, &app, &QGuiApplication::quit);
		});
	}

	// --foldertest <report.json>: Asset Library folders on a throwaway project (temp folder).
	if (const int ft2 = int(app.arguments().indexOf("--foldertest")); ft2 > 0 && ft2 + 1 < app.arguments().size()) {
		const QString report = app.arguments()[ft2 + 1];
		QTimer::singleShot(2000, &app, [&, report]() {
			QJsonArray checks;
			auto check = [&](const QString &name, bool ok, const QString &detail = {}) { checks << QJsonObject{{"name", name}, {"ok", ok}, {"detail", detail}}; };
			controller.newDocument();
			const QString rootDir = controller.assetsFolder();
			QDir(rootDir + "/LOBS folder test").removeRecursively();
			QDir(rootDir + "/Renamed folder").removeRecursively();
			QDir(rootDir + "/Moved").removeRecursively();
			// make a folder, go in, import a PNG there
			const QString made = controller.createAssetFolder();
			check("create folder", QFileInfo(made).isDir(), made);
			controller.renameAsset(made, "LOBS folder test");
			const QString folder = rootDir + "/LOBS folder test";
			check("rename folder", QFileInfo(folder).isDir());
			controller.setFolderColor(folder, "#3b82f6");
			bool colored = false, markerShown = false;
			for (const QVariant &v : controller.assets())
				colored |= v.toMap()["path"].toString() == folder && v.toMap()["color"].toString() == "#3b82f6";
			controller.openAssetFolder(folder);
			for (const QVariant &v : controller.assets())
				markerShown |= v.toMap()["name"].toString() == ".lobsfolder";
			controller.assetUp();
			check("folder color is saved and the marker stays hidden", colored && !markerShown);
			controller.openAssetFolder(folder);
			check("navigate into folder", controller.assetPath() == "LOBS folder test" && controller.assetCrumbs().size() == 2, controller.assetPath());
			const QString png = QDir::tempPath() + "/lobs-folder-test.png";
			QImage img(320, 180, QImage::Format_ARGB32);
			img.fill(QColor("#00ffaa"));
			img.save(png);
			controller.importAssets({QUrl::fromLocalFile(png)});
			const QString inFolder = folder + "/lobs-folder-test.png";
			check("import lands in the current folder", QFile::exists(inFolder));
			controller.placeAsset(inFolder);
			auto fileOfLayer = [&]() {
				for (const lobs::Instance &i : controller.doc().instances) {
					const QString f = controller.doc().sources.value(i.sourceId).settings.value("file").toString();
					if (f.contains("lobs-folder-test") || f.contains("renamed-asset"))
						return f;
				}
				return QString();
			};
			// rename the file: the layer follows
			controller.renameAsset(inFolder, "renamed-asset");
			check("renaming a file keeps the extension and repoints the layer", fileOfLayer() == folder + "/renamed-asset.png", fileOfLayer());
			// up, then move the whole folder into another folder: the layer follows, undo history too
			controller.assetUp();
			check("navigate up", controller.assetPath().isEmpty());
			// search from the top finds files inside subfolders, and says where they are
			const QVariantList hits = controller.searchAssets("renamed-asset");
			check("search reaches subfolders", hits.size() == 1 && hits[0].toMap()["where"].toString() == "LOBS folder test",
			      hits.isEmpty() ? QString("no hits") : hits[0].toMap()["where"].toString());
			const QString other = rootDir + "/Moved";
			QDir().mkpath(other);
			controller.moveAssets({folder}, other);
			const QString expected = other + "/LOBS folder test/renamed-asset.png";
			check("moving a folder repoints its files", fileOfLayer() == expected && QFile::exists(expected), fileOfLayer());
			controller.undo(); // removes the layer (the last command was placing it)...
			controller.redo(); // ...and back: the restored state must use the new path too
			check("undo history uses the new location", fileOfLayer() == expected, fileOfLayer());
			// a folder in use can't be trashed; once unused it can
			controller.trashAssets({other});
			check("folder in use is kept", QFileInfo(other).isDir());
			controller.selectLayer(controller.doc().instances.isEmpty() ? QString() : controller.doc().instances.begin().key(), false);
			for (const lobs::Instance &i : controller.doc().instances)
				controller.selectLayer(i.id, false);
			controller.deleteSelected();
			controller.trashAssets({other});
			check("unused folder goes to the Recycle Bin", !QFileInfo(other).exists());
			QFile::remove(png);
			bool all = true;
			for (const QJsonValue c : checks)
				all &= c.toObject()["ok"].toBool();
			QFile f(report);
			f.open(QIODevice::WriteOnly | QIODevice::Truncate);
			f.write(QJsonDocument(QJsonObject{{"ok", all}, {"checks", checks}}).toJson());
			f.close();
			QTimer::singleShot(300, &app, &QGuiApplication::quit);
		});
	}

	// --texttest <report.json>: a text layer's font, style and color reach the renderer.
	if (const int tt = int(app.arguments().indexOf("--texttest")); tt > 0 && tt + 1 < app.arguments().size()) {
		const QString report = app.arguments()[tt + 1];
		QTimer::singleShot(2000, &app, [&]() {
			controller.newDocument();
			controller.addGraphic("text");
			QString sid, iid;
			for (const lobs::Instance &i : controller.doc().instances)
				if (controller.doc().sources.value(i.sourceId).kind == "text")
					sid = i.sourceId, iid = i.id;
			controller.setSourceSetting(sid, "text", "LOBS");
			controller.setSourceSetting(sid, "font", "Times New Roman");
			controller.setSourceSetting(sid, "italic", true);
			controller.setSourceSetting(sid, "textColor", "#ff0000");
			controller.take();
			controller.setProperty("lobsTestInst", iid);
		});
		QTimer::singleShot(4500, &app, [&, report]() {
			QJsonArray checks;
			auto check = [&](const QString &name, bool ok, const QString &detail = {}) { checks << QJsonObject{{"name", name}, {"ok", ok}, {"detail", detail}}; };
			const QString iid = controller.property("lobsTestInst").toString();
			if (obs_source_t *t = obs_get_source_by_name((iid + ":text").toUtf8().constData())) {
#ifdef Q_OS_WIN
				obs_data_t *st = obs_source_get_settings(t);
				obs_data_t *font = obs_data_get_obj(st, "font");
				const QString face = obs_data_get_string(font, "face"), style = obs_data_get_string(font, "style");
				const long long flags = obs_data_get_int(font, "flags"), color = obs_data_get_int(st, "color");
				check("font face", face == "Times New Roman", face);
				check("bold + italic style", style == "Bold Italic" && flags == 3, style + QString(" flags=%1").arg(flags));
				check("text color (BGR)", (color & 0xFFFFFF) == 0x0000FF, QString::number(color & 0xFFFFFF, 16));
				obs_data_release(font);
				obs_data_release(st);
#else // LOBS draws text itself (renderText): no OBS text settings to read, the pixels say it all
				check("text drawn by LOBS", QByteArray(obs_source_get_id(t)) == "lobs_frame", obs_source_get_id(t));
#endif
				obs_source_release(t);
			} else {
				check("text renderer exists", false);
			}
			const QImage img = engine.renderThumbnail(engine.programScene(), 480);
			int red = 0;
			for (int y = 0; y < img.height(); y += 2)
				for (int x = 0; x < img.width(); x += 2) {
					const QColor c = img.pixelColor(x, y);
					red += c.red() > 180 && c.green() < 80 && c.blue() < 80;
				}
			check("red text renders", red > 20, QString("%1 red samples").arg(red));
			bool all = true;
			for (const QJsonValue c : checks)
				all &= c.toObject()["ok"].toBool();
			QFile f(report);
			f.open(QIODevice::WriteOnly | QIODevice::Truncate);
			f.write(QJsonDocument(QJsonObject{{"ok", all}, {"checks", checks}}).toJson());
			f.close();
			QTimer::singleShot(300, &app, &QGuiApplication::quit);
		});
	}

	// --captiontest <speech.wav> <report.json>: live captions on a media source playing the clip
	// (CC-01 without the QA harness; the model comes from LOBS_WHISPER_MODEL or the data folder).
	if (const int ct = int(app.arguments().indexOf("--captiontest")); ct > 0 && ct + 2 < app.arguments().size()) {
		const QString wav = app.arguments()[ct + 1], report = app.arguments()[ct + 2];
		QTimer::singleShot(2000, &app, [&, wav, report]() {
			controller.newDocument();
			controller.addSource("media", "Speech", wav, true);
			const QString sid = controller.doc().sourceOrder.last();
			controller.take(); // on program, so the clip plays
			controller.mediaCommand(sid, "restart");
			const bool started = captions.start(sid);
			auto *poll = new QTimer(&app);
			auto *clock = new QElapsedTimer;
			clock->start();
			QObject::connect(poll, &QTimer::timeout, &app, [&, poll, clock, started, report]() {
				QString text;
				for (const QVariant &l : captions.lines())
					text += l.toMap().value("text").toString() + " ";
				if (!text.contains("water", Qt::CaseInsensitive) && clock->elapsed() < 60000)
					return;
				poll->stop();
				captions.stop();
				bool all = true; // a word from each phrase: nothing lost at the seams
				for (const char *w : {"stream", "today", "haunted", "follow", "water"})
					all &= text.contains(QLatin1String(w), Qt::CaseInsensitive);
				QFile f(report);
				f.open(QIODevice::WriteOnly | QIODevice::Truncate);
				f.write(QJsonDocument(QJsonObject{{"ok", started && all}, {"started", started}, {"model", captions.modelState()},
								  {"seconds", clock->elapsed() / 1000.0}, {"status", captions.status()}, {"text", text.trimmed()}}).toJson());
				f.close();
				QTimer::singleShot(300, &app, &QGuiApplication::quit);
			});
			poll->start(250);
		});
	}

	// --crossplatformtest <report.json>: a show's devices saved on the other platform (Windows ids on
	// a Mac, Mac ids on Windows) give way to this machine's, a window waits to be picked, and a
	// kind this platform can't capture gets no stand-in. On the throwaway show; nothing is saved.
	if (const int cp = int(app.arguments().indexOf("--crossplatformtest")); cp > 0 && cp + 1 < app.arguments().size()) {
		const QString report = app.arguments()[cp + 1];
		QTimer::singleShot(2000, &app, [&, report]() {
			controller.newDocument();
#ifdef Q_OS_WIN
			const QVariantMap foreign{{"camera", "0FD4B3A2-236E-492B-8CE5-255DD288CE50"}, {"display", "37D8832A-2D66-02CA-B9F7-8F30A301B230"}, {"window", "12345"}};
#else
			const QVariantMap foreign{{"camera", "@device:pnp:\\\\?\\usb#vid_046d&pid_085e#abc#{65e8773d}\\global"},
						  {"display", "\\\\?\\DISPLAY#DELA0A3#5&2a2b3c4d&0&UID4353#{e6f07b5f}"},
						  {"window", "OBS 32.0:Qt663QWindowIcon:obs64.exe"},
						  {"mic", "{0.0.1.00000000}.{4b2c7a1e-1111-2222-3333-444455556666}"}};
#endif
			QJsonArray checks;
			auto check = [&](const QString &name, bool ok, const QString &detail = {}) { checks << QJsonObject{{"name", name}, {"ok", ok}, {"detail", detail}}; };
			QHash<QString, QString> ids; // kind -> source id
			for (auto it = foreign.cbegin(); it != foreign.cend(); ++it) {
				controller.addSource(it.key(), "", "", true);
				const QString sid = controller.doc().sourceOrder.last();
				controller.setSourceSetting(sid, "device", it.value());
				ids[it.key()] = sid;
			}
			// A device that's this machine's own stays as it is (an unplugged one would too).
			controller.addSource("camera", "Local camera", "", true);
			const QString local = controller.doc().sourceOrder.last(), localDev = controller.doc().sources[local].settings.value("device").toString();
			controller.fillMissingDevices(); // what loading a show runs
			const lobs::Doc &d = controller.doc();
			for (auto it = ids.cbegin(); it != ids.cend(); ++it) {
				const QString dev = d.sources[it.value()].settings.value("device").toString();
				if (it.key() == "window")
					check("window: other-platform id cleared, waits for a pick", dev.isEmpty(), dev);
				else
					check(it.key() + ": other-platform id replaced", dev != foreign[it.key()].toString(), dev.isEmpty() ? "(none on this machine)" : dev);
			}
			check("this machine's camera kept", d.sources[local].settings.value("device").toString() == localDev, localDev);
#ifndef Q_OS_WIN
			controller.addSource("game", "Game", "", true);
			const QString game = controller.doc().sourceOrder.last();
			obs_source_t *g = obs_get_source_by_name(game.toUtf8().constData());
			check("game capture: no stand-in source on macOS", !g, g ? obs_source_get_id(g) : "none");
			obs_source_release(g);
#else
			controller.addSource("game", "Game", "", true);
			const QString game = controller.doc().sourceOrder.last();
			controller.setSourceSetting(game, "transparent", true);
			obs_source_t *g = obs_get_source_by_name(game.toUtf8().constData());
			obs_data_t *gst = g ? obs_source_get_settings(g) : nullptr;
			check("game capture: Allow transparency reaches OBS", gst && obs_data_get_bool(gst, "allow_transparency"), g ? "" : "no source");
			obs_data_release(gst);
			obs_source_release(g);
#endif
			bool all = true;
			for (const QJsonValue c : checks)
				all &= c.toObject()["ok"].toBool();
			QFile f(report);
			f.open(QIODevice::WriteOnly | QIODevice::Truncate);
			f.write(QJsonDocument(QJsonObject{{"ok", all}, {"checks", checks}}).toJson());
			f.close();
			QTimer::singleShot(300, &app, &QGuiApplication::quit);
		});
	}

	// --tooltest <report.json>: graphic tools through the real canvas handlers, on a throwaway show.
	if (const int tl = int(app.arguments().indexOf("--tooltest")); tl > 0 && tl + 1 < app.arguments().size()) {
		const QString report = app.arguments()[tl + 1];
		auto *checks = new QJsonArray;
		auto check = [checks](const QString &name, bool ok, const QString &detail = {}) { *checks << QJsonObject{{"name", name}, {"ok", ok}, {"detail", detail}}; };
		auto drag = [&](const QString &tool, QVector<QPointF> pts, Qt::KeyboardModifiers mods = {}) {
			controller.setTool(tool);
			controller.canvasPress(controller.fromCanvas(pts.first()), mods, false);
			for (int i = 1; i < pts.size(); ++i)
				controller.canvasMove(controller.fromCanvas(pts[i]), mods);
			controller.canvasRelease();
		};
		auto layerOf = [&](const QString &shapeOrKind) -> const lobs::Instance * {
			const auto &insts = controller.doc().instances;
			for (auto it = insts.cbegin(); it != insts.cend(); ++it) {
				const lobs::Source &s = controller.doc().sources.value(it->sourceId);
				if (s.kind == shapeOrKind || (s.kind == "shape" && s.settings.value("shape").toString() == shapeOrKind))
					return &*it;
			}
			return nullptr;
		};
		QTimer::singleShot(2500, &app, [=, &controller, &engine, &app]() {
			controller.newDocument();
			controller.addSource("color", "Bg", "#000000", true);
			controller.fit("fit");
			controller.take();
			controller.setToolFill("#ff0000");
			drag("rect", {{100, 100}, {300, 200}, {500, 300}});
			const lobs::Instance *r = layerOf("rect");
			const lobs::Transform rt = r ? controller.doc().transform(*r) : lobs::Transform{};
			check("rectangle from a drag", r && rt.x == 100 && rt.y == 100 && rt.w == 400 && rt.h == 200, QString("%1,%2 %3x%4").arg(rt.x).arg(rt.y).arg(rt.w).arg(rt.h));
			check("tool returns to select after a shape", controller.tool() == "select", controller.tool());
			QVector<QPointF> stroke;
			for (int i = 0; i <= 40; ++i)
				stroke << QPointF(800 + i * 10, 600 + 60 * std::sin(i / 4.0));
			drag("pencil", stroke);
			const lobs::Instance *pn = layerOf("path");
			{
				const QVariantMap pst = controller.doc().sources.value(pn ? pn->sourceId : QString()).settings;
				const QVariantList pl = pst.value("points").toList();
				QString first3;
				for (int i = 0; i < std::min<qsizetype>(3, pl.size()); ++i)
					first3 += QString("[%1,%2] ").arg(pl[i].toList().value(0).toDouble(), 0, 'f', 3).arg(pl[i].toList().value(1).toDouble(), 0, 'f', 3);
				const QImage direct = renderShape(pst, 400, 140);
				int lit = 0;
				for (int y = 0; y < direct.height(); ++y)
					for (int x = 0; x < direct.width(); ++x)
						lit += qAlpha(direct.pixel(x, y)) > 128;
				check("pencil renders directly", lit > 500, QString("%1 px, pts: %2").arg(lit).arg(first3));
			}
			check("pencil stroke becomes a layer", pn && controller.doc().sources.value(pn->sourceId).settings.value("points").toList().size() > 20);
			check("pencil stays on", controller.tool() == "pencil");
			drag("arrow", {{1300, 200}, {1700, 400}});
			check("arrow layer", layerOf("arrow") != nullptr);
			drag("text", {{200, 800}});
			check("text from a click", layerOf("text") != nullptr);
			check("click starts typing", controller.textEditing());
			for (const char *k : {"H", "i"}) {
				QKeyEvent ke(QEvent::KeyPress, 0, Qt::NoModifier, QString::fromLatin1(k));
				controller.textKey(&ke);
			}
			{
				const lobs::Instance *tl = layerOf("text");
				const lobs::Transform tt = tl ? controller.doc().transform(*tl) : lobs::Transform{};
				check("typed text lands in the layer", tl && controller.doc().sources.value(tl->sourceId).settings.value("text").toString() == "Hi");
				check("text box grows with typing", tt.w > 20, QString("%1 x %2").arg(tt.w).arg(tt.h));
			}
			controller.endTextEdit();
			const int before = int(controller.doc().instances.size());
			drag("text", {{900, 900}, {900, 1000}});
			controller.endTextEdit();
			check("empty text box is dropped", int(controller.doc().instances.size()) == before);
			controller.setSwatches(controller.swatches() << "#123456");
			const lobs::Doc round = lobs::Doc::fromJson(controller.doc().toJson());
			check("swatches save in the show", round.swatches.contains("#123456") && round.swatches.size() == 13, round.swatches.join(","));
			if (const lobs::Instance *rl = layerOf("rect")) {
				const QString rid = rl->id;
				controller.selectLayer(rid, false);
				const lobs::Transform before = controller.doc().transform(*rl);
				controller.scrubTransform("rot", 20, false);
				controller.scrubTransform("rot", 45, true);
				const lobs::Transform after = controller.doc().transform(controller.doc().instances[rid]);
				check("rotation scrubs", after.rot == 45 && after.x == before.x && after.y == before.y, QString::number(after.rot));
			}
			{
				// Layers: drag to reorder, into a group, and to make a group.
				auto top = [&]() { return controller.doc().scenes.value(controller.editSceneId()).layers; };
				const QStringList l0 = top();
				const QString a = l0.value(0), b = l0.value(1), c3 = l0.value(2);
				controller.dropLayer(a, b, "below");
				check("drag reorders layers", top().indexOf(a) == top().indexOf(b) + 1);
				controller.selectLayer(a, false);
				controller.moveSelected(-1000);
				check("Ctrl Shift ] brings to front", top().value(0) == a);
				controller.dropLayer(a, b, "group");
				const QString g = top().value(top().indexOf(b) >= 0 ? -1 : 0);
				const QStringList after = top();
				QString gid;
				for (const QString &id : after)
					if (controller.doc().instances[id].kind == "group" && controller.doc().instances[id].children.contains(a))
						gid = id;
				check("hold to make a group", !gid.isEmpty() && controller.doc().instances[gid].children.size() == 2);
				controller.dropLayer(c3, gid, "into");
				check("drop into a group", !gid.isEmpty() && controller.doc().instances[gid].children.value(0) == c3);
				controller.dropLayer(c3, gid, "below");
				controller.dropLayer(a, gid, "below");
				controller.dropLayer(b, gid, "below");
				check("emptied group goes away", !controller.doc().instances.contains(gid));
				Q_UNUSED(g);
			}
			// Projectors open, draw and close cleanly (program in a window, the scene too).
			controller.openProjector("", -1);
			controller.openProjector(controller.editSceneId(), -1);
			QTimer::singleShot(600, &app, [=, &controller]() {
				controller.closeProjectors();
				check("projectors open and close", true);
			});
			{
				// The background can be hidden but not deleted.
				const QString backTo = controller.editSceneId();
				controller.addScene();
				const QString bgScene = controller.editSceneId();
				const QString bg = controller.doc().scenes.value(bgScene).layers.value(0);
				controller.selectLayer(bg, false);
				controller.deleteSelected();
				check("background can't be deleted", controller.doc().instances.contains(bg));
				controller.moveLayer(bg, -1);
				controller.addSource("color", "Above bg", "#ff00ff", true);
				const QString top = controller.doc().scenes.value(bgScene).layers.value(0);
				controller.selectLayer(top, false);
				controller.moveSelected(1000);
				check("background stays at the bottom", controller.doc().scenes.value(bgScene).layers.last() == bg);
				controller.deleteScene(bgScene);
				controller.editScene(backTo);
			}
			controller.addSource("color", "Crop test", "#00ff00", true);
			if (const QString top = controller.doc().scenes.value(controller.editSceneId()).layers.value(0); !top.isEmpty()) {
				const QString rid = top; // the layer just added is on top
				controller.selectLayer(rid, false);
				const lobs::Transform t0 = controller.doc().transform(controller.doc().instances[rid]);
				controller.cropToAspect(1);
				const lobs::Instance &ri = controller.doc().instances[rid];
				const lobs::Transform t1 = controller.doc().transform(ri);
				const lobs::Crop c1 = controller.doc().crop(ri);
				check("crop to 1:1 keeps scale", std::abs(t1.w - t1.h) < 1 && std::abs(t1.w - t0.h) < 1 && std::abs(t1.x - (t0.x + (t0.w - t0.h) / 2)) < 1 && c1.l > 0 && c1.l == c1.r,
				      QString("%1,%2 %3x%4 crop %5/%6").arg(t1.x).arg(t1.y).arg(t1.w).arg(t1.h).arg(c1.l).arg(c1.r));
				controller.setCornerRadius(50, true);
				check("corners round", controller.doc().crop(controller.doc().instances[rid]).radius == 50);
				{
					// Moving and resizing a rounded layer stays fast (no new mask per frame).
					QElapsedTimer tm;
					tm.start();
					for (int k = 0; k < 60; ++k)
						controller.scrubTransform("x", 420 + k * 3, k == 59);
					const qint64 moveMs = tm.restart();
					for (int k = 0; k < 60; ++k)
						controller.scrubTransform("w", 1080 - k * 4, k == 59, true);
					const qint64 sizeMs = tm.elapsed();
					check("rounded layer drags smoothly", moveMs < 600 && sizeMs < 1500, QString("60 moves %1 ms, 60 resizes %2 ms").arg(moveMs).arg(sizeMs));
				}
				controller.setCropMode(true);
				check("crop mode on a layer", controller.cropMode());
				controller.setCropMode(false);
				controller.deleteSelected(); // out of the way of the render checks below
			}
			{
				const QString trId = controller.addTransition("swipe");
				check("transition added to the show", controller.transitions().size() == 3 && !trId.isEmpty());
				controller.updateTransition(trId, "durationMs", 1000);
				controller.setCurrentTransition(trId);
				check("transition in use", controller.currentTransition() == trId);
				controller.previewTransition(trId);
				QTimer::singleShot(250, &app, [=, &engine, &app]() {
					const QImage a = engine.renderTransitionPreview(320);
					QTimer::singleShot(300, &app, [=, &engine, &app]() {
						const QImage b = engine.renderTransitionPreview(320);
						b.save(QDir::tempPath() + "/lobs-trpreview.png");
						check("transition preview animates", !a.isNull() && !b.isNull() && a != b, QString("%1 %2").arg(a.pixelColor(160, 90).name(), b.pixelColor(160, 90).name()));
						engine.stopTransitionPreview();
					});
				});
			}
		});
		QTimer::singleShot(3300, &app, [=, &engine, &app]() { // short trail life: draw just before checking
			engine.laserPoint({960, 540}, true);
			for (int i = 1; i < 10; ++i)
				engine.laserPoint({960 + i * 20.0, 540}, false);
			QTimer::singleShot(120, &app, [=]() {
				obs_source_t *laser = obs_get_output_source(MAX_CHANNELS - 1);
				check("laser draws over the output", laser && obs_source_get_width(laser) > 0, QString("%1 px wide").arg(laser ? obs_source_get_width(laser) : 0));
				obs_source_release(laser);
			});
			// A still of a curvy trail, for eyeballing its look.
			QVector<LaserPoint> pts;
			for (int i = 0; i < 60; ++i)
				pts.push_back({QPointF(300 + i * 18.0, 400 + 120 * std::sin(i / 6.0)), qint64(i * 8), i == 0});
			bool any = false;
			renderLaser(pts, 480, 500, 1920, 1080, &any, QColor("#ef4444")).save(QDir::tempPath() + "/lobs-laser.png");
		});
		QTimer::singleShot(3500, &app, [=, &engine]() {
			const QImage img = engine.renderThumbnail(engine.programScene(), 960);
			img.save(QDir::tempPath() + "/lobs-tooltest.png");
			const QColor fill = img.pixelColor(150, 100); // middle of the rectangle (canvas 300,200)
			check("rectangle renders red", fill.red() > 200 && fill.green() < 60, fill.name());
			int ink = 0; // white pencil pixels near its path
			for (int x = 400; x < 600; ++x)
				for (int y = 270; y < 330; ++y)
					ink += img.pixelColor(x, y).lightness() > 200;
			check("pencil stroke renders", ink > 30, QString("%1 px").arg(ink));
		});
		QTimer::singleShot(5500, &app, [=, &app]() {
			obs_source_t *laser = obs_get_output_source(MAX_CHANNELS - 1);
			check("laser fades away", laser && obs_source_get_width(laser) == 0);
			obs_source_release(laser);
			bool all = true;
			for (const QJsonValue c : *checks)
				all &= c.toObject()["ok"].toBool();
			QFile f(report);
			f.open(QIODevice::WriteOnly | QIODevice::Truncate);
			f.write(QJsonDocument(QJsonObject{{"ok", all}, {"checks", *checks}}).toJson());
			f.close();
			QTimer::singleShot(300, &app, &QGuiApplication::quit);
		});
	}

	// --streamtest <rtmp url>: stream + record for 12 s with in-memory settings, write stats, quit.
	if (const int at = int(app.arguments().indexOf("--streamtest")); at > 0 && at + 1 < app.arguments().size()) {
		AppSettings s = engine.settings();
		s.stream.service = "Custom";
		s.stream.customUrl = app.arguments()[at + 1];
		s.stream.key = "lobs-test"; // dummy key for a local test server; never saved
		s.record.quality = "stream";
		engine.setSettings(s);
		QTimer::singleShot(3000, [&]() {
			if (!engine.startStreaming())
				qWarning("stream failed: %s", qPrintable(engine.lastError()));
			engine.startRecording();
		});
		QTimer::singleShot(15000, [&]() {
			const QVariantMap st = engine.streamStats();
			const bool streamed = engine.streaming();
			engine.stopRecording();
			engine.stopStreaming();
			QJsonObject r = QJsonObject::fromVariantMap(st);
			r["wasStreaming"] = streamed;
			r["encoder"] = engine.resolveEncoder();
			r["error"] = engine.lastError();
			r["recording"] = engine.lastRecording();
			QFile f(appOut + "/streamtest.json");
			f.open(QIODevice::WriteOnly | QIODevice::Truncate);
			f.write(QJsonDocument(r).toJson());
			f.close();
			QTimer::singleShot(2000, &app, &QGuiApplication::quit);
		});
	}

	const int rc = app.exec();
	QThreadPool::globalInstance()->waitForDone(); // a background autosave finishes writing
	captions.stop(); // its audio tap is on an engine input
	engine.shutdown();
#ifdef Q_OS_WIN
	if (rtwq) {
		if (auto shutdown = reinterpret_cast<HRESULT(STDAPICALLTYPE *)()>(GetProcAddress(rtwq, "RtwqShutdown")))
			shutdown();
		FreeLibrary(rtwq);
	}
#endif
	{
		QMutexLocker lock(&g_logLock); // an engine thread may still be logging
		g_log = nullptr;
	}
	return rc;
}

#include "main.moc"
