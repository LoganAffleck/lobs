#include "cli_tests.h"
#include "app.h"
#include "captions.h"
#include "engine.h"
#include "engine_manager.h"
#include "hotkeys.h"
#include "permissions.h"
#include "shapes.h"

#include <obs.h>
#include <obs-hotkey.h>

#include <QColor>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QTimer>
#include <QUrl>
#include <cmath>

int runEngineSmokeTest(QGuiApplication &app, Engine &engine, const QString &docPath, const QString &reportPath)
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
		for (const lobs::Instance &i : controller->doc().instances)
			if (controller->doc().sources.value(i.sourceId).kind == "browser")
				controller->selectLayer(i.id, false);
		controller->setTransform("flipH", 1);
	});
	QTimer::singleShot(7000, &app, [=, &engine, &app]() {
		const QImage img = engine.renderThumbnail(engine.programScene(), 320);
		const QColor left = img.isNull() ? QColor() : img.pixelColor(10, 10), right = img.isNull() ? QColor() : img.pixelColor(309, 10);
		check("mirroring", right.blue() > 150 && left.blue() < 90, left.name() + " / " + right.name());
		const QString fade = controller->addTransition("fade");
		controller->updateTransition(fade, "durationMs", 1000);
		controller->setCurrentTransition(fade);
		controller->addScene();
		controller->take();
		QTimer::singleShot(300, &app, [=, &app]() {
			obs_source_t *out = obs_get_output_source(0);
			const float t = out ? obs_transition_get_time(out) : -1;
			check("transition plays", out && obs_source_get_type(out) == OBS_SOURCE_TYPE_TRANSITION && t > 0.05f && t < 0.95f, QString("t=%1").arg(t));
			obs_source_release(out);
		});
		QTimer::singleShot(1600, &app, [=, &engine]() {
			obs_source_t *out = obs_get_output_source(0);
			obs_source_t *active = out ? obs_transition_get_active_source(out) : nullptr;
			check("transition lands on the new scene", active && active == engine.sceneSource(engine.programScene()));
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

void attachCliTests(QGuiApplication &app, App &controller, Engine &engine, EngineUpdater &engineUpdater, Captioner &captions, const QString &appOut)
{
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

	if (const int at = int(app.arguments().indexOf("--snapshot")); at > 0 && at + 1 < app.arguments().size()) {
		const QString out = app.arguments()[at + 1];
		QTimer::singleShot(4000, [&, out]() {
			engine.renderThumbnail(engine.programScene(), engine.canvasW()).save(out);
			QTimer::singleShot(500, &app, &QGuiApplication::quit);
		});
	}

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

	if (const int at = int(app.arguments().indexOf("--importtest")); at > 0 && at + 2 < app.arguments().size()) {
		const QString in = app.arguments()[at + 1], outDir = app.arguments()[at + 2];
		QTimer::singleShot(3000, [&, in, outDir]() {
			controller.importObs(in);
			QDir().mkpath(outDir);
			QFile n(outDir + "/notes.txt");
			n.open(QIODevice::WriteOnly | QIODevice::Truncate);
			n.write(controller.toast().toUtf8());
		});
		QTimer::singleShot(12000, [&]() { engine.setAllScenesShowing(true); });
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
			controller.renameAsset(inFolder, "renamed-asset");
			check("renaming a file keeps the extension and repoints the layer", fileOfLayer() == folder + "/renamed-asset.png", fileOfLayer());
			controller.assetUp();
			check("navigate up", controller.assetPath().isEmpty());
			const QVariantList hits = controller.searchAssets("renamed-asset");
			check("search reaches subfolders", hits.size() == 1 && hits[0].toMap()["where"].toString() == "LOBS folder test",
			      hits.isEmpty() ? QString("no hits") : hits[0].toMap()["where"].toString());
			const QString other = rootDir + "/Moved";
			QDir().mkpath(other);
			controller.moveAssets({folder}, other);
			const QString expected = other + "/LOBS folder test/renamed-asset.png";
			check("moving a folder repoints its files", fileOfLayer() == expected && QFile::exists(expected), fileOfLayer());
			controller.undo();
			controller.redo();
			check("undo history uses the new location", fileOfLayer() == expected, fileOfLayer());
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
#else
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

	if (const int ct = int(app.arguments().indexOf("--captiontest")); ct > 0 && ct + 2 < app.arguments().size()) {
		const QString wav = app.arguments()[ct + 1], report = app.arguments()[ct + 2];
		QTimer::singleShot(2000, &app, [&, wav, report]() {
			controller.newDocument();
			controller.addSource("media", "Speech", wav, true);
			const QString sid = controller.doc().sourceOrder.last();
			controller.take();
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
				bool all = true;
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
			QHash<QString, QString> ids;
			for (auto it = foreign.cbegin(); it != foreign.cend(); ++it) {
				controller.addSource(it.key(), "", "", true);
				const QString sid = controller.doc().sourceOrder.last();
				controller.setSourceSetting(sid, "device", it.value());
				ids[it.key()] = sid;
			}
			controller.addSource("camera", "Local camera", "", true);
			const QString local = controller.doc().sourceOrder.last(), localDev = controller.doc().sources[local].settings.value("device").toString();
			controller.fillMissingDevices();
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
			controller.setToolStroke("#ffffff");
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
			controller.openProjector("", -1);
			controller.openProjector(controller.editSceneId(), -1);
			QTimer::singleShot(600, &app, [=, &controller]() {
				controller.closeProjectors();
				check("projectors open and close", true);
			});
			{
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
				const QString rid = top;
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
				controller.deleteSelected();
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
		QTimer::singleShot(3300, &app, [=, &engine, &app]() {
			engine.laserPoint({960, 540}, true);
			for (int i = 1; i < 10; ++i)
				engine.laserPoint({960 + i * 20.0, 540}, false);
			QTimer::singleShot(120, &app, [=]() {
				obs_source_t *laser = obs_get_output_source(MAX_CHANNELS - 1);
				check("laser draws over the output", laser && obs_source_get_width(laser) > 0, QString("%1 px wide").arg(laser ? obs_source_get_width(laser) : 0));
				obs_source_release(laser);
			});
			QVector<LaserPoint> pts;
			for (int i = 0; i < 60; ++i)
				pts.push_back({QPointF(300 + i * 18.0, 400 + 120 * std::sin(i / 6.0)), qint64(i * 8), i == 0});
			bool any = false;
			renderLaser(pts, 480, 500, 1920, 1080, &any, QColor("#ef4444")).save(QDir::tempPath() + "/lobs-laser.png");
		});
		QTimer::singleShot(3500, &app, [=, &engine]() {
			const QImage img = engine.renderThumbnail(engine.programScene(), 960);
			img.save(QDir::tempPath() + "/lobs-tooltest.png");
			const QColor fill = img.pixelColor(150, 100);
			check("rectangle renders red", fill.red() > 200 && fill.green() < 60, fill.name());
			int ink = 0;
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

	if (const int at = int(app.arguments().indexOf("--streamtest")); at > 0 && at + 1 < app.arguments().size()) {
		AppSettings s = engine.settings();
		s.stream.service = "Custom";
		s.stream.customUrl = app.arguments()[at + 1];
		s.stream.key = "lobs-test";
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
}
