#include "app.h"
#include "app_updater.h"
#include "browser_view.h"
#include "captions.h"
#include "cli_tests.h"
#include "crash_report.h"
#include "dock_tabs.h"
#include "engine.h"
#include "engine_manager.h"
#include "frontend.h"
#include "icons.h"
#include "permissions.h"
#include "platform.h"
#include "plugin_panel.h"
#include "preview_host.h"
#include "rows_model.h"
#include "source_preview.h"
#include "startup_mode.h"
#include "twitch_account.h"
#include "websocket_server.h"
#include "window_chrome.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/qtquick/Platform.h>
#include <obs.h>

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QMutex>
#include <QPainter>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRasterWindow>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QThreadPool>
#include <QTimer>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
QString installEngineZip(const QString &zip, const QString &version, QString *note);
#endif
#ifdef LOBS_TESTS
#include "test_runner.h"
#endif

static QFile *g_log = nullptr;
static QMutex g_logLock;
static QElapsedTimer g_uptime;

static void logHandler(int level, const char *fmt, va_list args, void *)
{
	char buf[4096];
	vsnprintf(buf, sizeof(buf), fmt, args);
	if (g_log) {
		static const char *names[] = {"error", "warning", "info", "debug"};
		const int idx = level <= LOG_ERROR ? 0 : level <= LOG_WARNING ? 1 : level <= LOG_INFO ? 2 : 3;
		const QByteArray line = "[" + QByteArray::number(g_uptime.elapsed()) + "ms] " + names[idx] + ": " + buf + "\n";
		QMutexLocker lock(&g_logLock);
		if (g_log) {
			g_log->write(line);
			g_log->flush();
		}
	}
}

class Splash : public QRasterWindow {
public:
	explicit Splash(const QString &iconDir)
	{
		setFlags(Qt::SplashScreen | Qt::FramelessWindowHint);
		resize(360, 200);
		if (QScreen *s = QGuiApplication::primaryScreen())
			setPosition(s->availableGeometry().center() - QPoint(width() / 2, height() / 2));
		m_logo.load(iconDir + "/lobs-128.png");
		show();
		step("Starting");
	}
	void step(const QString &text)
	{
		m_text = text;
		update();
		QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
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
#ifndef Q_OS_WIN
	obs_set_cmdline_args(argc, argv);
#endif
	QApplication app(argc, argv);
	app.setOrganizationName("LOBS");
	app.setApplicationName("LOBS");
#ifdef LOBS_TESTS
	const int scenarioAt = int(app.arguments().indexOf("--scenariotest"));
	const bool scenario = scenarioAt > 0 && scenarioAt + 2 < app.arguments().size();
	if (scenario)
		lobs_test::isolateProfile(app.arguments()[scenarioAt + 2]);
#endif
	// Benchmarks and scripted runs: the show and settings live apart from the user's own.
	if (const int at = int(app.arguments().indexOf("--sandbox")); at > 0 && at + 1 < app.arguments().size()) {
		QStandardPaths::setTestModeEnabled(true);
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, app.arguments()[at + 1]);
	}

	for (const QString &a : app.arguments().mid(1)) {
		if (!a.endsWith(".lobsfx", Qt::CaseInsensitive) || !QFileInfo::exists(a))
			continue;
		const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/effects";
		QDir().mkpath(dir);
		const QString to = dir + "/" + QFileInfo(a).fileName();
		QFile::remove(to);
		const bool ok = QFile::copy(a, to);
		QMessageBox::information(nullptr, "LOBS", ok ? QStringLiteral(u"Added \u201c") + QFileInfo(a).completeBaseName() + QStringLiteral(u"\u201d to your presets.\n\nIn LOBS: select an audio source, then + Effect \u2192 Start from a preset.")
							      : QStringLiteral(u"Couldn't add that preset."));
		return ok ? 0 : 1;
	}

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
	const HMODULE rtwq = LoadLibraryW(L"RTWorkQ.dll");
	if (rtwq)
		if (auto startup = reinterpret_cast<HRESULT(STDAPICALLTYPE *)()>(GetProcAddress(rtwq, "RtwqStartup")))
			startup();
	const QString engineBin = sel.root + "/bin/64bit";
	SetDllDirectoryW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(engineBin).utf16()));
	qputenv("PATH", QDir::toNativeSeparators(engineBin).toLocal8Bit() + ";" + qgetenv("PATH"));
	for (const QFileInfo &fi : QDir(engineBin).entryInfoList({"*.exe"}, QDir::Files)) {
		const QString dest = QCoreApplication::applicationDirPath() + "/" + fi.fileName();
		const QFileInfo d(dest);
		if (!d.exists() || d.size() != fi.size() || d.lastModified() != fi.lastModified()) {
			QFile::remove(dest);
			QFile::copy(fi.filePath(), dest);
		}
	}
#else
	const engine_store::Selected sel = engine_store::select();
	if (sel.version.isEmpty()) {
		platform::fatalError("LOBS couldn't find its OBS engine. Reinstall LOBS to restore it.");
		return 3;
	}
#endif
	QQuickStyle::setStyle("Basic");
	QString fontFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
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

	const QString root = platform::resourceDir(), appOut = platform::outputDir();
#ifdef Q_OS_WIN
	QDir::setCurrent(engineBin);
#endif

	{
		QIcon icon;
		for (int s : {16, 20, 24, 32, 40, 48, 64, 128, 256})
			icon.addFile(root + QString("/icons/lobs-%1.png").arg(s), QSize(s, s));
		QGuiApplication::setWindowIcon(icon);
	}
	std::unique_ptr<Splash> splash;
	if (!isSelfTestRun(app.arguments()) && !app.arguments().contains("--engine-smoketest"))
		splash = std::make_unique<Splash>(root + "/icons");
	if (QFileInfo prev(appOut + "/lobs.log"); prev.exists() && prev.size() > 0) {
		QDir logs(appOut + "/logs");
		logs.mkpath(".");
		QFile::rename(prev.absoluteFilePath(), logs.filePath("lobs-" + prev.lastModified().toString("yyyy-MM-dd HH-mm-ss") + ".log"));
		const QFileInfoList old = logs.entryInfoList({"lobs-*.log"}, QDir::Files, QDir::Name | QDir::Reversed);
		for (int i = 10; i < old.size(); i++)
			QFile::remove(old[i].absoluteFilePath());
	}
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
	qmlRegisterType<PluginPanel>("LOBS.Web", 1, 0, "PluginPanel");
	qmlRegisterType<RowsModel>("LOBS.Models", 1, 0, "RowsModel");
	engine.setSettings(AppSettings::load());
	blog(LOG_INFO, "[lobs] OBS engine %s from %s", qPrintable(sel.version), qPrintable(sel.root));
	if (!engine.start(sel.root))
		qWarning("engine failed: %s", qPrintable(engine.status()));
	else {
		engine.resetVideo();
		BrowserView::setEngineReady(true);
		SourcePreview::setEngine(&engine);
	}
	blog(LOG_INFO, "[lobs] startup: engine up");
	if (splash)
		splash->step("Loading your show");
	crash_report::install(crashDir, appOut + "/lobs.log");
	EngineUpdater engineUpdater(sel);

	const bool selfTest = isSelfTestRun(app.arguments());
	const QString docPath = selfTest ? QDir::tempPath() + "/lobs-selftest.lobs.json"
					 : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/current.lobs.json";
	if (selfTest) {
		QFile::remove(docPath);
		QFile::remove(QDir::tempPath() + "/layout-v2.json");
		if (const QString l = qEnvironmentVariable("LOBS_QA_LAYOUT"); !l.isEmpty())
			QFile::copy(l, QDir::tempPath() + "/layout-v2.json");
		AppSettings::defaultRecordFolderOverride() = QDir::tempPath() + "/lobs-selftest-recordings";
	}

	if (const int st = int(app.arguments().indexOf("--engine-smoketest")); st > 0 && st + 1 < app.arguments().size())
		return runEngineSmokeTest(app, engine, docPath, app.arguments()[st + 1]);
	App controller(&engine, docPath);
	lobs::frontend::setApp(&controller);
	blog(LOG_INFO, "[lobs] startup: show loaded");
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
	dockConfig.setDraggedWindowOpacity(0.35);
	dockConfig.setTransparencyOnlyOverDropIndicator(false);
	dockConfig.setViewFactory(new LobsViewFactory());

	WindowChrome chrome;
	Captioner captions(&engine);
	CrashReport crashReport(crashDir);
	DockTabs dockTabs;
	if (selfTest)
		crashReport.dismiss();
	AppUpdater appUpdater(app.arguments().contains("--updated"));
	QQmlApplicationEngine qml;
	KDDockWidgets::QtQuick::Platform::instance()->setQmlEngine(&qml);
	PreviewHost previewHost(&engine, &controller);
	TwitchAccount twitch(&controller);
	ObsWebSocket websocket(&controller);
	qml.rootContext()->setContextProperty("previewHost", &previewHost);
	qml.rootContext()->setContextProperty("app", &controller);
	qml.rootContext()->setContextProperty("twitch", &twitch);
	qml.rootContext()->setContextProperty("websocket", &websocket);
	qml.rootContext()->setContextProperty("engineUpdater", &engineUpdater);
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
	QTimer::singleShot(0, &app, [] { lobs::frontend::loaded(); });
	if (qml.rootObjects().isEmpty())
		return 1;
	blog(LOG_INFO, "[lobs] startup: UI built");
	if (app.arguments().contains("--startrecording")) // as OBS's flag: for shortcuts and scripts
		QTimer::singleShot(0, &engine, [&engine] { engine.startRecording(); });
	if (auto *mainWindow = qobject_cast<QQuickWindow *>(qml.rootObjects().first())) {
		QObject::connect(mainWindow, &QQuickWindow::frameSwapped, mainWindow, [&splash] {
			blog(LOG_INFO, "[lobs] startup: first frame");
			QTimer::singleShot(0, [&splash] { splash.reset(); });
		}, Qt::SingleShotConnection);
		chrome.attach(mainWindow);
	}
#ifdef LOBS_TESTS
	if (scenario)
		lobs_test::startScenario(app, controller, engine, qml, app.arguments()[scenarioAt + 1], app.arguments()[scenarioAt + 2]);
	if (qEnvironmentVariableIsSet("LOBS_CRASH_TEST"))
		QTimer::singleShot(3000, [] { *static_cast<volatile int *>(nullptr) = 1; });
#endif
	QTimer::singleShot(5000, [&]() {
		if (engine.started())
			engine_store::markGood(sel.version);
		if (!selfTest) {
			appUpdater.checkIfDue();
			permissions.askForNeeded();
		}
	});
	QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [&] {
		if (!selfTest && QGuiApplication::applicationState() != Qt::ApplicationActive && permissions.grantedNeedingRelaunch()) {
			blog(LOG_INFO, "[lobs] quit from outside after a permission change: reopening");
			platform::relaunchAfterExit();
		}
	});
	QObject::connect(&permissions, &Permissions::relaunchRequested, &app, [&] {
		platform::relaunchAfterExit();
		if (auto *w = qobject_cast<QQuickWindow *>(qml.rootObjects().value(0)))
			w->close();
		else
			QCoreApplication::quit();
	});
	QObject::connect(&appUpdater, &AppUpdater::quitForInstall, &app, [&] {
		if (auto *w = qobject_cast<QQuickWindow *>(qml.rootObjects().value(0)))
			w->close();
		else
			QCoreApplication::quit();
	});
	if (qEnvironmentVariableIsSet("LOBS_UPDATE_FEED") && qEnvironmentVariableIsSet("LOBS_UPDATE_AUTO") && !appUpdater.justUpdated()) {
		QObject::connect(&appUpdater, &AppUpdater::changed, &app, [&] {
			if (qEnvironmentVariable("LOBS_UPDATE_AUTO") == "check")
				return;
			if (appUpdater.state() == "available" && appUpdater.message().isEmpty())
				appUpdater.download();
			else if (appUpdater.state() == "ready")
				QTimer::singleShot(1000, &appUpdater, &AppUpdater::installAndRestart);
		});
		QTimer::singleShot(3000, &appUpdater, &AppUpdater::check);
	}

	attachCliTests(app, controller, engine, engineUpdater, captions, appOut);

	const int rc = app.exec();
	lobs::frontend::exiting();
	BrowserView::releaseAll();
	SourcePreview::releaseAll();
	QThreadPool::globalInstance()->waitForDone();
	captions.stop();
	engine.shutdown();
#ifdef Q_OS_WIN
	if (rtwq) {
		if (auto shutdown = reinterpret_cast<HRESULT(STDAPICALLTYPE *)()>(GetProcAddress(rtwq, "RtwqShutdown")))
			shutdown();
		FreeLibrary(rtwq);
	}
#endif
	{
		QMutexLocker lock(&g_logLock);
		g_log = nullptr;
	}
	return rc;
}
