// QA scenario runner: see test_runner.h. Scenarios are JavaScript with two globals:
//   app - the App controller, exactly as QML sees it (invokables + properties)
//   t   - TestApi below: waits, doc/engine/UI inspection, input synthesis, results
// plus the helpers in kPrelude (story, check, eq, near, time, stats...).
#include "startup_mode.h"
#include "test_runner.h"

#include "app.h"
#include "engine.h"
#include "settings.h"

#include <obs.h>

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtTest/QTest>

#include <atomic>
#include <chrono>
#include <thread>

#define NOMINMAX
#include <windows.h>
#include <psapi.h>

namespace lobs_test {

static const char *kPrelude = R"JS(
function story(id, title, fn) {
	t.beginStory(id, title);
	try { fn(); }
	catch (e) { t.check("no exception", false, String(e) + (e && e.stack ? "\n" + e.stack : "")); }
	t.endStory();
}
function detailOf(d) { return d === undefined ? "" : (typeof d === "string" ? d : JSON.stringify(d)); }
function check(name, cond, detail) { t.check(name, !!cond, detailOf(detail)); return !!cond; }
function eq(name, got, want) {
	const ok = JSON.stringify(got) === JSON.stringify(want);
	t.check(name, ok, ok ? "" : "got " + JSON.stringify(got) + ", want " + JSON.stringify(want));
	return ok;
}
function near(name, got, want, tol) {
	const ok = typeof got === "number" && Math.abs(got - want) <= tol;
	t.check(name, ok, ok ? "" : "got " + got + ", want " + want + " +/- " + tol);
	return ok;
}
function time(fn) { const s = t.now(); fn(); return t.now() - s; }
function stats(xs) {
	const a = xs.slice().sort((x, y) => x - y), n = a.length;
	const q = p => n ? a[Math.min(n - 1, Math.max(0, Math.ceil(p * n) - 1))] : NaN;
	const r = v => Math.round(v * 100) / 100;
	return { n: n, min: r(a[0]), p50: r(q(0.5)), p95: r(q(0.95)), p99: r(q(0.99)), max: r(a[n - 1]),
		 mean: r(a.reduce((s, v) => s + v, 0) / (n || 1)) };
}
// Fresh empty show; returns the doc.
function fresh() { app.newDocument(); t.wait(150); return t.doc(); }
// Ids helpers over t.doc().
function sceneIds() { return t.doc().sceneOrder; }
function layersOf(sceneId) { return t.doc().scenes[sceneId].layers; }
function inst(id) { return t.doc().instances[id]; }
function lastAdded(before, after) { return after.filter(x => before.indexOf(x) < 0); }
)JS";

class TestApi : public QObject {
	Q_OBJECT
public:
	TestApi(App &c, Engine &e, QQmlApplicationEngine &q, QJSEngine &js, const QString &script, const QString &out)
		: m_c(c), m_e(e), m_qml(q), m_js(js), m_script(script), m_out(out)
	{
		m_clock.start();
		connect(&m_e, &Engine::recordingFinished, this, [this](bool ok, const QString &path, qint64 bytes) {
			m_lastRec = QVariantMap{{"ok", ok}, {"path", path}, {"bytes", bytes}};
			++m_recCount;
		});
		connect(&m_e, &Engine::replaySaved, this, [this](const QString &path) { m_lastReplay = path; ++m_replayCount; });
		connect(&m_e, &Engine::streamError, this, [this](const QString &m) { m_streamErrors << m; });
		if (QQuickWindow *w = mainWindow())
			connect(w, &QQuickWindow::frameSwapped, this, [this]() { ++m_frames; m_frameTimes << m_clock.nsecsElapsed() / 1e6; }, Qt::DirectConnection);
	}

	int failures() const { return m_failures; }

	// ---------- time ----------
	Q_INVOKABLE double now() const { return m_clock.nsecsElapsed() / 1e6; }
	Q_INVOKABLE void wait(int ms)
	{
		QEventLoop loop;
		QTimer::singleShot(ms, &loop, &QEventLoop::quit);
		loop.exec();
	}
	Q_INVOKABLE bool waitUntil(const QJSValue &fn, int timeoutMs = 5000, int stepMs = 10)
	{
		const double end = now() + timeoutMs;
		QJSValue f = fn;
		while (true) {
			const QJSValue r = f.call();
			if (!r.isError() && r.toBool())
				return true;
			if (now() > end)
				return false;
			wait(stepMs);
		}
	}
	// Waits for the main window to present n more frames; returns ms waited (-1 on timeout).
	Q_INVOKABLE double waitFrames(int n = 1, int timeoutMs = 2000)
	{
		const double start = now();
		const qint64 target = m_frames + n;
		if (QQuickWindow *w = mainWindow())
			w->update();
		while (m_frames < target) {
			if (now() - start > timeoutMs)
				return -1;
			QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
		}
		return now() - start;
	}
	// Frame presentation times (ms) since the last call: for frame-interval measurement.
	Q_INVOKABLE QVariantList takeFrameTimes()
	{
		QVariantList out;
		for (double v : m_frameTimes)
			out << v;
		m_frameTimes.clear();
		return out;
	}
	Q_INVOKABLE void setTimeout(int seconds) { m_timeoutS = seconds; }
	int timeoutS() const { return m_timeoutS; }

	// ---------- files ----------
	Q_INVOKABLE QString out(const QString &name) const { return m_out + "/" + name; }
	Q_INVOKABLE QString scriptDir() const { return QFileInfo(m_script).absolutePath(); }
	Q_INVOKABLE QUrl url(const QString &path) const { return QUrl::fromLocalFile(path); }
	Q_INVOKABLE bool writeFile(const QString &path, const QString &text) const
	{
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile f(path);
		return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(text.toUtf8()) >= 0;
	}
	Q_INVOKABLE QString readFile(const QString &path) const
	{
		QFile f(path);
		return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
	}
	Q_INVOKABLE bool exists(const QString &path) const { return QFileInfo::exists(path); }
	Q_INVOKABLE qint64 fileSize(const QString &path) const { return QFileInfo(path).size(); }
	Q_INVOKABLE bool removeFile(const QString &path) const { return QFile::remove(path); }
	Q_INVOKABLE QStringList listDir(const QString &path) const { return QDir(path).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot); }
	// LOBS-001: which argument lists start a self-test (and so use a throwaway document).
	Q_INVOKABLE bool isSelfTestArgs(const QStringList &args) const { return isSelfTestRun(args); }
	// Runs both JavaScript collectors (the scenario's and the UI's), to tell garbage from leaks.
	// Diagnostics: stop the controller notifying the UI, to tell UI costs from C++ ones.
	Q_INVOKABLE void blockAppSignals(bool on) { m_c.blockSignals(on); }
	// Diagnostics: creates and destroys a QML snippet n times in the UI's engine (under the main
	// window), then collects, and returns the private-memory growth in MB.
	Q_INVOKABLE double churn(const QString &qml, int n)
	{
		QQuickWindow *w = m_qml.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(m_qml.rootObjects().first());
		if (!w)
			return -1;
		QQmlComponent comp(&m_qml);
		const QString header = QStringLiteral("import QtQuick\nimport QtQuick.Controls\nimport LOBS\n");
		comp.setData((header + qml).toUtf8(), QUrl("qrc:/qt/qml/LOBS/qml/churn.qml"));
		if (comp.isError()) {
			qWarning("[churn] %s", qPrintable(comp.errorString()));
			return -2;
		}
		auto mb = [] {
			PROCESS_MEMORY_COUNTERS_EX pm{};
			GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pm), sizeof(pm));
			return double(pm.PrivateUsage) / (1 << 20);
		};
		m_qml.collectGarbage();
		QCoreApplication::processEvents();
		const double m0 = mb();
		for (int i = 0; i < n; ++i) {
			QObject *o = comp.create(m_qml.rootContext());
			if (auto *it = qobject_cast<QQuickItem *>(o))
				it->setParentItem(w->contentItem());
			delete o;
			if (i % 50 == 0)
				QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		}
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		m_qml.collectGarbage();
		QCoreApplication::processEvents();
		return mb() - m0;
	}
	Q_INVOKABLE void gc() { m_js.collectGarbage(); m_qml.collectGarbage(); }
	Q_INVOKABLE QString tempDir() const { return QDir::tempPath(); }
	Q_INVOKABLE QString appDataDir() const { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); }
	Q_INVOKABLE bool include(const QString &file)
	{
		const QString path = QFileInfo(file).isAbsolute() ? file : scriptDir() + "/" + file;
		const QJSValue r = m_js.evaluate(readFile(path), path);
		if (r.isError()) {
			check("include " + file, false, r.toString());
			return false;
		}
		return true;
	}

	// ---------- document ----------
	Q_INVOKABLE QVariantMap doc() const { return m_c.doc().toJson().toVariantMap(); }
	Q_INVOKABLE QString docJson() const { return QString::fromUtf8(QJsonDocument(m_c.doc().toJson()).toJson(QJsonDocument::Compact)); }
	Q_INVOKABLE bool saveDocTo(const QString &path) const { return writeFile(path, QString::fromUtf8(QJsonDocument(m_c.doc().toJson()).toJson())); }

	// ---------- engine ----------
	Q_INVOKABLE QString programScene() const { return m_e.programScene(); }
	Q_INVOKABLE QVariantMap engineStats() const { return QVariantMap{{"stats", m_e.stats()}, {"live", m_e.liveStats()}, {"status", m_e.status()}, {"layers", m_e.layerResources()}}; }
	Q_INVOKABLE QVariantMap inputFilters() const { return m_e.inputFilters().toVariantMap(); }
	Q_INVOKABLE QString lastError() const { return m_e.lastError(); }
	Q_INVOKABLE QVariantList obsInputs() const
	{
		QVariantList out;
		obs_enum_sources(
			[](void *p, obs_source_t *s) {
				auto *list = static_cast<QVariantList *>(p);
				*list << QVariantMap{{"name", QString::fromUtf8(obs_source_get_name(s))},
						     {"id", QString::fromUtf8(obs_source_get_id(s))},
						     {"active", obs_source_active(s)},
						     {"showing", obs_source_showing(s)},
						     {"width", int(obs_source_get_width(s))},
						     {"height", int(obs_source_get_height(s))}};
				return true;
			},
			&out);
		return out;
	}
	Q_INVOKABLE QVariantMap sourceSize(const QString &sourceId) const
	{
		const QSize s = m_e.sourceSize(sourceId);
		return {{"w", s.width()}, {"h", s.height()}};
	}
	// Renders a scene ("" = program) at full canvas width unless given; returns the PNG path.
	Q_INVOKABLE QString snapshot(const QString &sceneId, const QString &name, int width = 0)
	{
		const QString sid = sceneId.isEmpty() ? m_e.programScene() : sceneId;
		const QString path = out(name);
		QDir().mkpath(QFileInfo(path).absolutePath());
		m_e.renderThumbnail(sid, width > 0 ? width : m_e.canvasW()).save(path);
		return path;
	}
	Q_INVOKABLE QVariantMap pixel(const QString &path, int x, int y) const
	{
		const QImage img(path);
		if (img.isNull() || x < 0 || y < 0 || x >= img.width() || y >= img.height())
			return {{"ok", false}, {"w", img.width()}, {"h", img.height()}};
		const QColor c = img.pixelColor(x, y);
		return {{"ok", true}, {"r", c.red()}, {"g", c.green()}, {"b", c.blue()}, {"a", c.alpha()}, {"hex", c.name(QColor::HexArgb)},
			{"w", img.width()}, {"h", img.height()}};
	}
	Q_INVOKABLE void setAllScenesShowing(bool on) { m_e.setAllScenesShowing(on); }
	Q_INVOKABLE void setRecordFolder(const QString &dir)
	{
		AppSettings s = m_e.settings();
		s.record.folder = dir;
		QDir().mkpath(dir);
		m_e.setSettings(s);
	}
	Q_INVOKABLE void setReplaySeconds(int s)
	{
		AppSettings a = m_e.settings();
		a.record.replaySeconds = s;
		m_e.setSettings(a);
	}
	Q_INVOKABLE QVariantMap engineSettings() const { return m_e.settings().toMap(); }
	Q_INVOKABLE void recordClip(int seconds) { m_e.recordClip(seconds); }
	Q_INVOKABLE void startRecording() { m_e.startRecording(); }
	Q_INVOKABLE void stopRecording() { m_e.stopRecording(); }
	Q_INVOKABLE bool isRecording() const { return m_e.recording(); }
	Q_INVOKABLE int recordingsFinished() const { return m_recCount; }
	Q_INVOKABLE QVariantMap lastRecording() const { return m_lastRec; }
	Q_INVOKABLE bool startReplay() { return m_e.startReplay(); }
	Q_INVOKABLE void stopReplay() { m_e.stopReplay(); }
	Q_INVOKABLE void saveReplay() { m_e.saveReplay(); }
	Q_INVOKABLE int replaysSaved() const { return m_replayCount; }
	Q_INVOKABLE QString lastReplay() const { return m_lastReplay; }
	Q_INVOKABLE bool vcamAvailable() const { return m_e.vcamAvailable(); }
	Q_INVOKABLE bool startVirtualCam() { return m_e.startVirtualCam(); }
	Q_INVOKABLE void stopVirtualCam() { m_e.stopVirtualCam(); }
	Q_INVOKABLE QStringList streamErrors() const { return m_streamErrors; }

	// ---------- process ----------
	Q_INVOKABLE QVariantMap proc() const
	{
		PROCESS_MEMORY_COUNTERS_EX pm{};
		pm.cb = sizeof(pm);
		GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pm), sizeof(pm));
		DWORD handles = 0;
		GetProcessHandleCount(GetCurrentProcess(), &handles);
		return {{"workingSetMB", double(pm.WorkingSetSize) / (1 << 20)},
			{"privateMB", double(pm.PrivateUsage) / (1 << 20)},
			{"handles", int(handles)},
			{"gdi", int(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS))},
			{"user", int(GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS))}};
	}

	// ---------- canvas input (through the controller, as the preview window delivers it) ----------
	// Canvas pixel coordinates. mods: "shift", "ctrl", "alt" joined by '+'. Returns per-move ms.
	Q_INVOKABLE QVariantMap canvasDrag(double x0, double y0, double x1, double y1, int steps = 20, const QString &mods = {}, int stepMs = 0)
	{
		const Qt::KeyboardModifiers m = parseMods(mods);
		QVariantList moves;
		const double s = now();
		m_c.canvasPress(m_c.fromCanvas({x0, y0}), m, false);
		for (int i = 1; i <= steps; ++i) {
			const double f = double(i) / steps;
			const double a = now();
			m_c.canvasMove(m_c.fromCanvas({x0 + (x1 - x0) * f, y0 + (y1 - y0) * f}), m);
			moves << now() - a;
			if (stepMs > 0)
				wait(stepMs);
		}
		const double r = now();
		m_c.canvasRelease();
		return {{"moves", moves}, {"releaseMs", now() - r}, {"totalMs", now() - s}};
	}
	Q_INVOKABLE void canvasClick(double x, double y, const QString &mods = {}, bool dbl = false)
	{
		m_c.canvasPress(m_c.fromCanvas({x, y}), parseMods(mods), dbl);
		m_c.canvasRelease();
	}

	// ---------- UI (QML items in every LOBS window) ----------
	// Visible items whose text contains `text` (case-insensitive): [{index, cls, text, x, y, w, h}].
	Q_INVOKABLE QVariantList items(const QString &text) const
	{
		QVariantList out;
		const auto found = findItems(text, false);
		for (int i = 0; i < found.size(); ++i) {
			QQuickItem *it = found[i];
			const QPointF p = it->mapToScene({0, 0});
			out << QVariantMap{{"index", i}, {"cls", QString::fromLatin1(it->metaObject()->className())},
					   {"text", it->property("text").toString()}, {"x", p.x()}, {"y", p.y()},
					   {"w", it->width()}, {"h", it->height()}, {"enabled", it->isEnabled()}};
		}
		return out;
	}
	// Clicks the nth visible item whose text equals `text` exactly (falls back to contains).
	Q_INVOKABLE bool click(const QString &text, int nth = 0, const QString &mods = {}, bool dbl = false, bool right = false)
	{
		QList<QQuickItem *> found = findItems(text, true);
		if (found.isEmpty())
			found = findItems(text, false);
		if (nth >= found.size())
			return false;
		QQuickItem *it = found[nth];
		QQuickWindow *w = it->window();
		const QPoint p = it->mapToScene({it->width() / 2, it->height() / 2}).toPoint();
		const Qt::MouseButton b = right ? Qt::RightButton : Qt::LeftButton;
		if (dbl)
			QTest::mouseDClick(w, b, parseMods(mods), p);
		else
			QTest::mouseClick(w, b, parseMods(mods), p);
		QCoreApplication::processEvents();
		return true;
	}
	// Window-relative click (logical px) in the main window.
	Q_INVOKABLE void clickAt(int x, int y, const QString &mods = {}, bool right = false)
	{
		if (QQuickWindow *w = mainWindow())
			QTest::mouseClick(w, right ? Qt::RightButton : Qt::LeftButton, parseMods(mods), QPoint(x, y));
		QCoreApplication::processEvents();
	}
	// A key sequence like "Ctrl+G", "Delete", "Ctrl+Shift+G" into the focused window.
	Q_INVOKABLE bool key(const QString &seq)
	{
		const QKeySequence ks(seq);
		QWindow *w = QGuiApplication::focusWindow();
		if (!w)
			w = mainWindow();
		if (ks.isEmpty() || !w)
			return false;
		const QKeyCombination kc = ks[0];
		QTest::keyClick(w, kc.key(), kc.keyboardModifiers());
		QCoreApplication::processEvents();
		return true;
	}
	Q_INVOKABLE void typeText(const QString &text)
	{
		QWindow *w = QGuiApplication::focusWindow() ? QGuiApplication::focusWindow() : mainWindow();
		if (w)
			for (const QChar ch : text)
				QTest::sendKeyEvent(QTest::Click, w, Qt::Key_unknown, QString(ch), Qt::NoModifier);
		QCoreApplication::processEvents();
	}
	Q_INVOKABLE QString grab(const QString &name)
	{
		QQuickWindow *w = mainWindow();
		if (!w)
			return {};
		const QString path = out(name);
		QDir().mkpath(QFileInfo(path).absolutePath());
		w->grabWindow().save(path);
		return path;
	}
	Q_INVOKABLE QVariantMap windowSize() const
	{
		QQuickWindow *w = mainWindow();
		return w ? QVariantMap{{"w", w->width()}, {"h", w->height()}, {"dpr", w->devicePixelRatio()}} : QVariantMap{};
	}
	Q_INVOKABLE void resizeWindow(int w, int h)
	{
		if (QQuickWindow *win = mainWindow()) {
			win->showNormal();
			win->resize(w, h);
		}
		wait(300);
	}
	// Clicks the item at `index` in the list items(text) returned (any LOBS window).
	Q_INVOKABLE bool clickItem(const QString &text, int index, const QString &mods = {}, bool right = false, bool dbl = false)
	{
		const auto found = findItems(text, false);
		if (index < 0 || index >= found.size())
			return false;
		QQuickItem *it = found[index];
		const QPoint p = it->mapToScene({it->width() / 2, it->height() / 2}).toPoint();
		const Qt::MouseButton b = right ? Qt::RightButton : Qt::LeftButton;
		if (dbl)
			QTest::mouseDClick(it->window(), b, parseMods(mods), p);
		else
			QTest::mouseClick(it->window(), b, parseMods(mods), p);
		QCoreApplication::processEvents();
		return true;
	}
	// Visible native dialogs (Win32 class #32770, e.g. file pickers, message boxes) of this process.
	Q_INVOKABLE QStringList nativeDialogs() const
	{
		QStringList out;
		EnumWindows(
			[](HWND h, LPARAM p) -> BOOL {
				DWORD pid = 0;
				GetWindowThreadProcessId(h, &pid);
				wchar_t cls[64] = {}, title[256] = {};
				if (pid == GetCurrentProcessId() && IsWindowVisible(h) && GetClassNameW(h, cls, 64) && wcscmp(cls, L"#32770") == 0) {
					GetWindowTextW(h, title, 256);
					reinterpret_cast<QStringList *>(p)->append(QString::fromWCharArray(title));
				}
				return TRUE;
			},
			reinterpret_cast<LPARAM>(&out));
		return out;
	}
	// Closes native dialogs of this process (Cancel), so a test never leaves one open.
	Q_INVOKABLE int closeNativeDialogs()
	{
		int n = 0;
		EnumWindows(
			[](HWND h, LPARAM p) -> BOOL {
				DWORD pid = 0;
				GetWindowThreadProcessId(h, &pid);
				wchar_t cls[64] = {};
				if (pid == GetCurrentProcessId() && IsWindowVisible(h) && GetClassNameW(h, cls, 64) && wcscmp(cls, L"#32770") == 0) {
					PostMessageW(h, WM_COMMAND, IDCANCEL, 0);
					++*reinterpret_cast<int *>(p);
				}
				return TRUE;
			},
			reinterpret_cast<LPARAM>(&n));
		return n;
	}
	Q_INVOKABLE int windowCount() const
	{
		int n = 0;
		for (QWindow *w : QGuiApplication::topLevelWindows())
			n += w->isVisible();
		return n;
	}
	// Items with a non-empty Accessible name vs all interactive-looking items (for AX-01).
	Q_INVOKABLE QVariantMap accessibilityCensus() const
	{
		int interactive = 0, named = 0;
		QStringList unnamed;
		for (QQuickItem *root : roots())
			walk(root, [&](QQuickItem *it) {
				const QString cls = QString::fromLatin1(it->metaObject()->className());
				// Static text accepts the left button only for link clicks, and Qt already names it from
				// its text for screen readers, so it isn't a control here.
				const bool control = !it->inherits("QQuickText") && (it->acceptedMouseButtons() != Qt::NoButton || it->activeFocusOnTab() || cls.contains("Button") || cls.contains("TextInput") || cls.contains("TextField"));
				if (!control)
					return;
				++interactive;
				QObject *att = nullptr;
				for (QObject *ch : it->children())
					if (QString::fromLatin1(ch->metaObject()->className()).startsWith("QQuickAccessibleAttached"))
						att = ch;
				const QString name = att ? att->property("name").toString() : QString();
				if (!name.isEmpty())
					++named;
				else if (unnamed.size() < 80)
					unnamed << cls + ":" + it->property("text").toString() + " in " + (it->parentItem() ? QString::fromLatin1(it->parentItem()->metaObject()->className()) + "/" + it->parentItem()->property("text").toString() + "/" + it->parentItem()->property("title").toString() : QString());
			});
		return {{"interactive", interactive}, {"named", named}, {"sampleUnnamed", unnamed}};
	}

	// ---------- results ----------
	Q_INVOKABLE void beginStory(const QString &id, const QString &title)
	{
		m_story = QJsonObject{{"id", id}, {"title", title}, {"status", "running"}, {"checks", QJsonArray()}, {"metrics", QJsonObject()}, {"notes", QJsonArray()}};
		m_storyStart = now();
		qInfo("[scenario] story %s: %s", qPrintable(id), qPrintable(title));
		writeResults("running");
	}
	Q_INVOKABLE void endStory()
	{
		if (m_story.isEmpty())
			return;
		const QJsonArray checks = m_story["checks"].toArray();
		bool ok = true;
		for (const QJsonValue &c : checks)
			ok = ok && c.toObject()["ok"].toBool();
		if (m_story["status"].toString() == "running")
			m_story["status"] = checks.isEmpty() ? "nochecks" : (ok ? "pass" : "fail");
		m_story["ms"] = int(now() - m_storyStart);
		m_stories << m_story;
		m_story = {};
		writeResults("running");
	}
	Q_INVOKABLE void check(const QString &name, bool ok, const QString &detail = {})
	{
		if (!ok)
			++m_failures;
		QJsonObject c{{"name", name}, {"ok", ok}};
		if (!detail.isEmpty())
			c["detail"] = detail;
		if (m_story.isEmpty())
			beginStory("_", "outside a story");
		QJsonArray a = m_story["checks"].toArray();
		a << c;
		m_story["checks"] = a;
		qInfo("[scenario]   %s %s %s", ok ? "PASS" : "FAIL", qPrintable(name), qPrintable(detail.left(300)));
	}
	Q_INVOKABLE void metric(const QString &name, const QJSValue &value)
	{
		QJsonObject m = m_story["metrics"].toObject();
		m[name] = QJsonValue::fromVariant(value.toVariant(QJSValue::ConvertJSObjects));
		m_story["metrics"] = m;
	}
	Q_INVOKABLE void note(const QString &text)
	{
		QJsonArray a = m_story["notes"].toArray();
		a << text;
		m_story["notes"] = a;
		qInfo("[scenario]   note: %s", qPrintable(text.left(300)));
	}
	Q_INVOKABLE void blocked(const QString &reason)
	{
		m_story["status"] = "blocked";
		note("BLOCKED: " + reason);
	}
	Q_INVOKABLE void log(const QString &text) const { qInfo("[scenario] %s", qPrintable(text)); }

	void writeResults(const QString &status)
	{
		QJsonArray all = m_stories;
		if (!m_story.isEmpty())
			all << m_story;
		int pass = 0, fail = 0, other = 0;
		for (const QJsonValue &v : all) {
			const QString s = v.toObject()["status"].toString();
			s == "pass" ? ++pass : s == "fail" ? ++fail : ++other;
		}
		const QJsonObject r{{"script", m_script}, {"status", status}, {"updated", QDateTime::currentDateTime().toString(Qt::ISODate)},
				    {"totals", QJsonObject{{"pass", pass}, {"fail", fail}, {"other", other}, {"failedChecks", m_failures}}},
				    {"stories", all}};
		QDir().mkpath(m_out);
		QFile f(m_out + "/results.json");
		if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
			f.write(QJsonDocument(r).toJson());
	}

private:
	static Qt::KeyboardModifiers parseMods(const QString &mods)
	{
		Qt::KeyboardModifiers m;
		const QString l = mods.toLower();
		if (l.contains("shift"))
			m |= Qt::ShiftModifier;
		if (l.contains("ctrl"))
			m |= Qt::ControlModifier;
		if (l.contains("alt"))
			m |= Qt::AltModifier;
		return m;
	}
	QQuickWindow *mainWindow() const
	{
		const auto roots = m_qml.rootObjects();
		return roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
	}
	QList<QQuickItem *> roots() const
	{
		QList<QQuickItem *> out;
		for (QWindow *w : QGuiApplication::topLevelWindows())
			if (auto *qw = qobject_cast<QQuickWindow *>(w); qw && qw->isVisible())
				out << qw->contentItem();
		return out;
	}
	template <typename F> static void walk(QQuickItem *it, const F &fn)
	{
		if (!it || !it->isVisible() || it->opacity() <= 0)
			return;
		fn(it);
		for (QQuickItem *ch : it->childItems())
			walk(ch, fn);
	}
	QList<QQuickItem *> findItems(const QString &text, bool exact) const
	{
		QList<QQuickItem *> out;
		// "prop=value" matches another property (e.g. "tip=Copy to another scene").
		const int eqAt = int(text.indexOf('='));
		const QByteArray prop = eqAt > 0 && !text.left(eqAt).contains(' ') ? text.left(eqAt).toLatin1() : QByteArray("text");
		const QString want = prop == "text" ? text : text.mid(eqAt + 1);
		for (QQuickItem *root : roots())
			walk(root, [&](QQuickItem *it) {
				const QVariant v = it->property(prop.constData());
				if (!v.isValid())
					return;
				const QString s = v.toString();
				if (exact ? s == want : s.contains(want, Qt::CaseInsensitive))
					out << it;
			});
		return out;
	}

	App &m_c;
	Engine &m_e;
	QQmlApplicationEngine &m_qml;
	QJSEngine &m_js;
	QString m_script, m_out;
	QElapsedTimer m_clock;
	QJsonArray m_stories;
	QJsonObject m_story;
	double m_storyStart = 0;
	int m_failures = 0;
	int m_timeoutS = 1800;
	qint64 m_frames = 0;
	QVector<double> m_frameTimes;
	QVariantMap m_lastRec;
	int m_recCount = 0, m_replayCount = 0;
	QString m_lastReplay;
	QStringList m_streamErrors;
};

void isolateProfile(const QString &outDir)
{
	QStandardPaths::setTestModeEnabled(true);
	QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
	const QString profile = outDir + "/profile";
	QDir(profile).removeRecursively();
	QDir().mkpath(profile);
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, profile);
}

void startScenario(QGuiApplication &app, App &controller, Engine &engine, QQmlApplicationEngine &qml, const QString &script, const QString &outDir)
{
	QDir().mkpath(outDir);
	auto *js = new QJSEngine(&app);
	js->installExtensions(QJSEngine::ConsoleExtension);
	auto *api = new TestApi(controller, engine, qml, *js, QFileInfo(script).absoluteFilePath(), QDir(outDir).absolutePath());
	QJSEngine::setObjectOwnership(&controller, QJSEngine::CppOwnership);
	QJSEngine::setObjectOwnership(api, QJSEngine::CppOwnership);
	js->globalObject().setProperty("app", js->newQObject(&controller));
	js->globalObject().setProperty("t", js->newQObject(api));
	api->setRecordFolder(QDir(outDir).absolutePath() + "/rec");
	api->writeResults("starting");

	// Give the engine and the first frames time to come up, like the other self-tests.
	QTimer::singleShot(3000, &app, [&app, js, api, script]() {
		std::atomic<bool> done{false};
		const int limit = api->timeoutS();
		std::thread watchdog([js, &done, limit]() {
			for (int i = 0; i < limit * 10 && !done; ++i)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (!done)
				js->setInterrupted(true);
		});
		QJSValue r = js->evaluate(QString::fromUtf8(kPrelude), "prelude.js");
		if (!r.isError()) {
			QFile f(script);
			if (!f.open(QIODevice::ReadOnly))
				r = js->evaluate("throw new Error('cannot read scenario')");
			else
				r = js->evaluate(QString::fromUtf8(f.readAll()), QFileInfo(script).absoluteFilePath());
		}
		done = true;
		watchdog.join();
		if (r.isError()) {
			api->endStory();
			api->beginStory("_script", "script error");
			api->check("script ran to the end", false,
				   r.toString() + " at line " + r.property("lineNumber").toString() + "\n" + r.property("stack").toString());
			api->endStory();
		}
		api->writeResults("done");
		const int code = api->failures() ? 1 : 0;
		QTimer::singleShot(500, &app, [&app, code]() { app.exit(code); });
	});
}

} // namespace lobs_test

#include "test_runner.moc"
