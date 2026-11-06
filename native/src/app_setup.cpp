#include "app_internal.h"
#include "enhanced_broadcast.h"
#include "obs_switch.h"
#include "projector.h"

#include <obs.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <memory>

using namespace lobs;

QVariantMap App::obsSetup(const QString &folder) const { return obs_switch::scan(folder.isEmpty() ? obs_switch::obsFolder() : folder); }

QVariantMap App::importFromObs(const QString &profileDir, const QString &collectionPath, const QVariantList &pluginList)
{
	QStringList done, notes;
	if (!profileDir.isEmpty()) {
		QVariantMap ch = obs_switch::readProfile(profileDir);
		notes << ch.take("_notes").toStringList();
		if (ch.contains("key") && !m_engine->settings().stream.key.isEmpty() && QSettings().contains("twitch/access"))
			ch.remove("key");
		saveSettings(ch);
		done << QString("Settings from the %1 profile: %2×%3 at %4 fps, %5%6").arg(QFileInfo(profileDir).fileName())
				.arg(ch.value("outWidth").toInt()).arg(ch.value("outHeight").toInt()).arg(ch.value("fps").toInt())
				.arg(ch.value("service").toString().isEmpty() ? QString("no stream service") : ch.value("service").toString())
				.arg(ch.contains("key") ? ", stream key" : "");
	}
	int added = 0;
	for (const QVariant &v : pluginList) {
		const QVariantMap p = v.toMap();
		QString path = p.value("path").toString();
		QTemporaryDir stage;
		if (const QString inst = p.value("installed").toString(); !inst.isEmpty()) {
			const QString name = p.value("name").toString();
			QDir(stage.path()).mkpath("obs-plugins/64bit");
			QFile::copy(path, stage.filePath("obs-plugins/64bit/" + name + ".dll"));
			const QString data = inst + "/data/obs-plugins/" + name;
			QDirIterator it(data, QDir::Files, QDirIterator::Subdirectories);
			while (it.hasNext()) {
				const QString f = it.next(), to = stage.filePath("data/obs-plugins/" + name + "/" + QDir(data).relativeFilePath(f));
				QDir().mkpath(QFileInfo(to).absolutePath());
				QFile::copy(f, to);
			}
			path = stage.path();
		}
		const QString err = addPlugin(path);
		if (err.isEmpty())
			++added;
		else
			notes << p.value("name").toString() + ": " + err;
	}
	if (added)
		done << QString("%1 plugin%2 (they load when LOBS restarts)").arg(added).arg(added == 1 ? "" : "s");
	if (!collectionPath.isEmpty() && importObs(collectionPath))
		done << "Scenes from " + QFileInfo(collectionPath).completeBaseName();
	qInfo("[lobs] switched from OBS: %s. Notes: %s", qPrintable(done.join("; ")), qPrintable(notes.join("; ")));
	return {{"done", done}, {"notes", notes}};
}

QVariantList App::obsCollections() const
{
	QVariantList out;
#ifdef Q_OS_WIN
	const QDir dir(qEnvironmentVariable("APPDATA") + "/obs-studio/basic/scenes");
#else
	const QDir dir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/obs-studio/basic/scenes");
#endif
	for (const QFileInfo &fi : dir.entryInfoList({"*.json"}, QDir::Files, QDir::Time)) {
		QFile f(fi.filePath());
		if (!f.open(QIODevice::ReadOnly))
			continue;
		const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
		if (!isObsCollection(o))
			continue;
		int scenes = 0;
		for (const QJsonValue v : o["scene_order"].toArray())
			scenes += !v.toObject()["name"].toString().startsWith("[LOBS]");
		out << QVariantMap{{"name", o["name"].toString(fi.completeBaseName())}, {"path", fi.filePath()}, {"scenes", scenes},
				   {"modified", fi.lastModified().toString("MMM d, yyyy")}};
	}
	return out;
}

bool App::importObs(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return showToast("Couldn't open " + path), false;
	const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
	if (!isObsCollection(o))
		return showToast("Not an OBS scene collection"), false;
	ObsImport imp = importObsCollection(o);
	for (Source &s : imp.doc.sources) {
		if (s.kind != "display" && s.kind != "camera")
			continue;
		const QString dev = s.settings.value("device").toString();
		bool present = false;
		for (const QVariant &v : m_engine->listDevices({}, s.kind))
			present |= v.toMap()["value"].toString() == dev;
		if (!present) {
			s.settings.remove("device");
			imp.skipped << QString("%1: device not connected, using this PC's default").arg(s.name);
		}
	}
	const QString name = o["name"].toString(QFileInfo(path).completeBaseName());
	if (live())
		return showToast("Stop streaming and recording to import a collection"), false;
	if (!m_projectFile.isEmpty() && !saveProject())
		return false;
	m_store.replace(withDefaultDevices(imp.doc, m_engine), "Import " + name + " from OBS");
	m_cleanDoc = doc();
	editScene(imp.programScene);
	take();
	m_engine->setAllScenesShowing(true);
	m_pendingPlacements = imp.pending;
	m_placeTries = 0;
	m_placeTimer.start(250);
	for (const QString &s : imp.skipped)
		qInfo("[import] skipped: %s", qPrintable(s));
	QSet<QString> filterSources;
	int unsupported = 0, devices = 0, other = 0;
	for (const QString &s : imp.skipped) {
		if (s.contains(" filter has no LOBS controls")) filterSources.insert(s.section(":", 0, 0));
		else if (s.contains("no LOBS equivalent")) ++unsupported;
		else if (s.contains("device not connected")) ++devices;
		else ++other;
	}
	QStringList gaps;
	if (!filterSources.isEmpty()) gaps << QString("controls for some filters on %1 source%2 (they still run)").arg(filterSources.size()).arg(filterSources.size() == 1 ? "" : "s");
	if (unsupported) gaps << QString("%1 source%2 with no LOBS equivalent").arg(unsupported).arg(unsupported == 1 ? "" : "s");
	if (devices) gaps << QString("%1 device%2 not connected, set to this PC's default").arg(devices).arg(devices == 1 ? "" : "s");
	if (other) gaps << QString("%1 other item%2").arg(other).arg(other == 1 ? "" : "s");
	if (!imp.pluginScenes.isEmpty())
		gaps << QString("%1 scene%2 made with the old LOBS plugin (%3)").arg(imp.pluginScenes.size()).arg(imp.pluginScenes.size() == 1 ? "" : "s").arg(imp.pluginScenes.join(", "));
	const int n = int(imp.doc.scenes.size());
	showToast(QString("Imported %1 scene%2 and %3 sources from OBS \"%4\".").arg(n).arg(n == 1 ? "" : "s").arg(imp.doc.sources.size()).arg(name) +
		  (gaps.isEmpty() ? QString() : " Not carried over: " + gaps.join(", ") + "."));
	return true;
}

void App::resolvePlacements(bool last)
{
	QVector<ObsPlacement> waiting;
	QVector<QPair<QString, Transform>> done;
	for (const ObsPlacement &p : m_pendingPlacements) {
		if (!doc().instances.contains(p.instId))
			continue;
		const Instance &i = doc().instances[p.instId];
		const QSize ns = m_engine->sourceSize(i.sourceId);
		if (ns.isEmpty() && !last) {
			waiting << p;
			continue;
		}
		if (ns.isEmpty())
			qInfo("[import] size unknown for %s; using OBS scale on a placeholder", qPrintable(doc().sources[i.sourceId].name));
		Transform t = resolveObsPlacement(p, ns.width(), ns.height(), doc().crop(i));
		const Transform cur = doc().transform(i);
		t.rot = cur.rot, t.flipH = cur.flipH, t.flipV = cur.flipV;
		done << qMakePair(p.instId, t);
	}
	if (!done.isEmpty())
		m_store.amend([&](Doc &d) {
			for (const auto &[id, t] : done)
				d.setTransform(id, t);
		});
	m_pendingPlacements = waiting;
	if (waiting.isEmpty() || last) {
		m_placeTimer.stop(), m_pendingPlacements.clear();
		m_engine->setAllScenesShowing(m_thumbTimer.isActive());
	}
}

QVariantList App::transitions() const
{
	QVariantList out;
	for (const lobs::Transition &t : doc().transitions)
		out << QVariantMap{{"id", t.id}, {"name", t.name}, {"type", t.type}, {"durationMs", t.durationMs}, {"settings", t.settings}};
	return out;
}

QString App::addTransition(const QString &type)
{
	static const QHash<QString, QString> names = {{"cut", "Cut"}, {"fade", "Fade"}, {"swipe", "Swipe"}, {"slide", "Slide"},
						      {"fade_to_color", "Fade to color"}, {"luma_wipe", "Luma wipe"}, {"stinger", "Stinger"}};
	if (!names.contains(type))
		return {};
	lobs::Transition t{lobs::uid("tr_"), names[type], type, type == "cut" ? 0 : type == "stinger" ? 1000 : 500, {}};
	if (type == "swipe" || type == "slide")
		t.settings = {{"direction", "left"}};
	if (type == "swipe")
		t.settings["swipe_in"] = false;
	if (type == "fade_to_color")
		t.settings = {{"color", "#000000"}, {"switch_point", 50}};
	if (type == "luma_wipe")
		t.settings = {{"luma_image", "linear-h.png"}, {"luma_softness", 0.03}, {"luma_invert", false}};
	if (type == "stinger")
		t.settings = {{"path", ""}, {"transition_point_type", 0}, {"transition_point", 500}, {"audio_monitoring", 0}, {"audio_fade_style", 0}, {"hw_decode", true}};
	int n = 1;
	for (const lobs::Transition &x : doc().transitions)
		if (x.type == type)
			++n;
	if (n > 1)
		t.name += QString(" %1").arg(n);
	m_store.commit("Add transition", [&](Doc &d) { d.transitions.push_back(t); });
	return t.id;
}

void App::removeTransition(const QString &id)
{
	if (doc().transitions.size() <= 1)
		return showToast("A show needs at least one transition");
	m_store.commit("Delete transition", [&](Doc &d) {
		for (int i = 0; i < d.transitions.size(); ++i)
			if (d.transitions[i].id == id)
				d.transitions.removeAt(i--);
		if (d.currentTransition == id)
			d.currentTransition = d.transitions.first().id;
	});
}

void App::setCurrentTransition(const QString &id)
{
	if (id == currentTransition())
		return;
	m_store.commit("Use transition", [&](Doc &d) { d.currentTransition = id; });
}

void App::updateTransition(const QString &id, const QString &field, const QVariant &value)
{
	m_store.commit("Edit transition", [&](Doc &d) {
		for (lobs::Transition &t : d.transitions) {
			if (t.id != id)
				continue;
			if (field == "name")
				t.name = value.toString().trimmed().isEmpty() ? t.name : value.toString().trimmed();
			else if (field == "durationMs")
				t.durationMs = std::clamp(value.toInt(), 50, 10000);
			else
				t.settings[field] = value;
		}
	});
}

void App::previewTransition(const QString &id)
{
	for (const lobs::Transition &t : doc().transitions)
		if (t.id == id)
			return m_engine->previewTransition(t);
}

void App::stopTransitionPreview() { m_engine->stopTransitionPreview(); }

QStringList App::lumaWipes() const
{
	QStringList out;
#ifdef Q_OS_WIN
	const QString engineDir = QCoreApplication::applicationDirPath() + "/engine";
#else
	const QString engineDir = QCoreApplication::applicationDirPath() + "/../PlugIns/obs-transitions.plugin";
#endif
	QDirIterator it(engineDir, {"wipes.json"}, QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QDir dir = QFileInfo(it.next()).dir();
		for (const QString &f : dir.entryList({"*.png"}, QDir::Files, QDir::Name))
			out << f;
		if (!out.isEmpty())
			break;
	}
	return out;
}

QVariantList App::screens() const
{
	QVariantList out;
	const auto list = QGuiApplication::screens();
	for (int i = 0; i < list.size(); ++i) {
		const QScreen *s = list[i];
		const QSize px = s->size() * s->devicePixelRatio();
		out << QVariantMap{{"index", i}, {"name", QString("%1: %2×%3").arg(s->model().isEmpty() ? s->name() : s->model()).arg(px.width()).arg(px.height())}};
	}
	return out;
}

void App::openProjector(const QString &sceneId, int screenIndex)
{
	const auto list = QGuiApplication::screens();
	QScreen *screen = screenIndex >= 0 && screenIndex < list.size() ? list[screenIndex] : nullptr;
	const QString what = sceneId.isEmpty() ? "Program" : doc().scenes.value(sceneId).name;
	m_projectors << new Projector(m_engine, sceneId, "LOBS projector · " + what, screen);
}

void App::closeProjectors()
{
	for (const QPointer<QWindow> &w : m_projectors)
		delete w.data();
	m_projectors.clear();
}

void App::runDemo(const QString &name)
{
	loadSample();
	m_store.amend([&](Doc &d) {
		QStringList gone;
		for (const Source &src : d.sources)
			if (src.kind == "display" || src.kind == "window" || src.kind == "game")
				gone << src.id;
		QStringList insts;
		for (const Instance &i : d.instances)
			if (gone.contains(i.sourceId))
				insts << i.id;
		for (const QString &id : insts)
			d.deleteInstance(id);
		for (const QString &sid : gone) {
			d.sources.remove(sid);
			d.sourceOrder.removeAll(sid);
		}
		for (const QString &sid : d.sceneOrder) {
			bool hasBg = false;
			for (const QString &id : d.scenes[sid].layers)
				hasBg |= d.sources.value(d.instances.value(id).sourceId).kind == "color";
			if (!hasBg) {
				placeBackground(d, sid);
				const QString bg = d.scenes[sid].layers.last();
				d.sources[d.instances[bg].sourceId].settings["color"] = "#1d1b2e";
			}
		}
	});
	QString scene = doc().sceneOrder.value(0);
	if (name != "tools")
		for (const QString &sid : doc().sceneOrder)
			for (const QString &id : doc().scenes.value(sid).layers)
				if (doc().instances.value(id).name == "Facecam" && scene == doc().sceneOrder.value(0))
					scene = sid;
	editScene(scene);
	auto layerNamed = [&](const QString &n) -> QString {
		for (const QString &id : doc().scenes.value(m_editScene).layers)
			if (doc().instances.value(id).name == n)
				return id;
		return {};
	};
	auto drag = [&](const QString &tool, QVector<QPointF> pts) {
		setTool(tool);
		canvasPress(fromCanvas(pts.first()), {}, false);
		for (int i = 1; i < pts.size(); ++i)
			canvasMove(fromCanvas(pts[i]), {});
		canvasRelease();
		endTextEdit();
	};
	if (name == "tools") {
		m_toolFill = "#ef4444";
		m_toolStroke = "#ffffff";
		m_toolStrokeWidth = 6;
		drag("rect", {{1320, 120}, {1500, 240}, {1780, 330}});
		drag("ellipse", {{120, 760}, {260, 860}, {360, 960}});
		drag("arrow", {{900, 700}, {1100, 640}, {1290, 300}});
		QVector<QPointF> wave;
		for (int i = 0; i <= 40; ++i)
			wave << QPointF(500 + i * 14, 880 + 50 * std::sin(i / 4.0));
		drag("pencil", wave);
		setTool("select");
		if (!m_selection.isEmpty())
			m_selection = {m_selection.first()};
		const QString rect = [&] {
			for (const QString &id : doc().scenes.value(m_editScene).layers)
				if (doc().sources.value(doc().instances.value(id).sourceId).settings.value("shape").toString() == "rect")
					return id;
			return QString();
		}();
		if (!rect.isEmpty())
			m_selection = {rect};
	} else if (name == "crop") {
		const QString cam = layerNamed("Facecam");
		if (!cam.isEmpty()) {
			m_selection = {cam};
			setCornerRadius(50, true);
			QTimer::singleShot(2500, this, [this, cam] {
				m_selection = {cam};
				cropToAspect(1);
				setCropMode(true);
			});
		}
	} else if (name == "layers") {
		m_toolFill = "#7c3aed";
		m_toolStrokeWidth = 0;
		drag("rect", {{80, 860}, {600, 1000}});
		drag("text", {{120, 890}, {120, 960}});
		const QStringList top = doc().scenes.value(m_editScene).layers;
		if (top.size() >= 2) {
			m_selection = {top[0], top[1]};
			groupSelected();
		}
	} else {
		const QString cam = layerNamed("Facecam");
		if (!cam.isEmpty())
			m_selection = {cam};
	}
	updateOverlay();
	emit docChanged();
}

void App::pingServers(const QString &service)
{
	struct Probe {
		QString name, url, host;
		quint16 port = 1935;
		QHostAddress addr;
		int best = -1, tries = 0;
	};
	auto probes = std::make_shared<QVector<Probe>>();
	for (const QVariant &v : m_engine->servers(service)) {
		const QVariantMap m = v.toMap();
		const QUrl u(m["value"].toString());
		if (u.host().isEmpty())
			continue;
		probes->push_back({m["name"].toString(), m["value"].toString(), u.host(), quint16(u.port(1935))});
	}
	if (probes->isEmpty())
		return emit serverPings({});
	auto pending = std::make_shared<int>(int(probes->size()));
	auto finish = [this, probes]() {
		std::sort(probes->begin(), probes->end(), [](const Probe &a, const Probe &b) {
			return (a.best < 0 ? 1e9 : a.best) < (b.best < 0 ? 1e9 : b.best);
		});
		QVariantList out;
		for (const Probe &p : *probes)
			out << QVariantMap{{"name", p.name}, {"value", p.url}, {"ms", p.best}};
		emit serverPings(out);
	};
	auto oneDone = [pending, finish]() { if (--*pending == 0) finish(); };
	std::shared_ptr<std::function<void(int)>> tryConnect = std::make_shared<std::function<void(int)>>();
	*tryConnect = [this, probes, oneDone, tryConnect](int i) {
		Probe &p = (*probes)[i];
		auto *sock = new QTcpSocket(this);
		auto t = std::make_shared<QElapsedTimer>();
		auto settled = std::make_shared<bool>(false);
		auto done = [=](bool ok) {
			if (*settled)
				return;
			*settled = true;
			Probe &q = (*probes)[i];
			if (ok) {
				const int ms = int(t->elapsed());
				q.best = q.best < 0 ? ms : std::min(q.best, ms);
			}
			sock->abort();
			sock->deleteLater();
			if (++q.tries < 3 && ok)
				(*tryConnect)(i);
			else
				oneDone();
		};
		connect(sock, &QTcpSocket::connected, this, [=] { done(true); });
		connect(sock, &QTcpSocket::errorOccurred, this, [=] { done(false); });
		QTimer::singleShot(2500, this, [=] { done(false); });
		t->start();
		sock->connectToHost(p.addr, p.port);
	};
	for (int i = 0; i < probes->size(); ++i) {
		QHostInfo::lookupHost((*probes)[i].host, this, [=](const QHostInfo &info) {
			Probe &p = (*probes)[i];
			const auto addrs = info.addresses();
			if (addrs.isEmpty())
				return oneDone();
			p.addr = addrs.first();
			(*tryConnect)(i);
		});
	}
}

bool App::startBandwidthTest(const QString &server, int kbps)
{
	const StreamSettings &st = m_engine->settings().stream;
	if (st.key.isEmpty())
		return showToast("Add your stream key first"), false;
	if (st.service != "Twitch")
		return showToast("The bandwidth test only works with Twitch"), false;
	return m_engine->startBandwidthTest(server, st.key + "?bandwidthtest", std::clamp(kbps, 1000, 20000), 10);
}

void App::stopBandwidthTest() { m_engine->stopBandwidthTest(); }

QVariantMap App::recommendSettings(int measured, const QString &service) const
{
	const int cap = service == "Twitch" ? 6000 : service.startsWith("YouTube") ? 9000 : 8000;
	const int bitrate = std::clamp(int(measured * 0.75 / 100) * 100, 100, cap);
	const bool slow = measured < 2000;
	int w = 1920, h = 1080, fps = 60;
	if (bitrate < 2500) w = 852, h = 480, fps = 30;
	else if (bitrate < 3500) w = 1280, h = 720, fps = 30;
	else if (bitrate < 4500) w = 1280, h = 720, fps = 60;
	else if (bitrate < 5800) w = 1600, h = 900, fps = 60;
	const QString enc = m_engine->resolveEncoder();
	const bool hardware = enc != "obs_x264";
	if (!hardware && h == 1080)
		w = 1280, h = 720;
	QString encName = enc;
	for (const QVariant &e : m_engine->encoders())
		if (e.toMap()["value"].toString() == enc)
			encName = e.toMap()["name"].toString();
	return {{"bitrate", bitrate}, {"outWidth", w}, {"outHeight", h}, {"fps", fps}, {"encoder", "auto"}, {"encoderName", encName}, {"hardware", hardware}, {"measured", measured}, {"slow", slow}};
}

QString App::streamKeyUrl(const QString &service) const
{
	if (service == "Twitch")
		return "https://dashboard.twitch.tv/settings/stream";
	if (service.startsWith("YouTube"))
		return "https://studio.youtube.com/channel/UC/livestreaming";
	if (service == "Kick")
		return "https://dashboard.kick.com/channel/stream";
	return {};
}

QVariantMap App::settings() const
{
	QVariantMap m = m_engine->settings().toMap();
	m["resolvedEncoder"] = m_engine->resolveEncoder();
	return m;
}

bool App::saveSettings(const QVariantMap &changes)
{
	AppSettings s = m_engine->settings();
	const int w = s.video.outWidth, h = s.video.outHeight, fps = s.video.fps;
	s.applyMap(changes);
	m_engine->setSettings(s);
	s.save();
	if (changes.contains("transition"))
		m_engine->setProgramScene(m_engine->programScene());
	if (s.video.outWidth != w || s.video.outHeight != h || s.video.fps != fps) {
		if (m_engine->live())
			showToast("Resolution and frame rate changes apply after you stop streaming and recording");
		else if (!m_engine->resetVideo())
			showToast(m_engine->lastError());
	}
	if (changes.contains("bitrate") && m_engine->streaming())
		m_engine->applyStreamBitrate();
	emit settingsChanged();
	return true;
}

static QString profileFile(const QString &name)
{
	QString safe = name;
	safe.replace(QRegularExpression(R"([<>:"/\\|?*\x00-\x1f])"), "_");
	return AppSettings::profilesFolder() + "/" + safe + ".json";
}

static bool writeProfile(const QString &name, const AppSettings &s)
{
	QDir().mkpath(AppSettings::profilesFolder());
	QJsonObject o = s.profileJson();
	o["name"] = name;
	QSaveFile f(profileFile(name));
	return f.open(QIODevice::WriteOnly) && f.write(QJsonDocument(o).toJson()) > 0 && f.commit();
}

QStringList App::profiles() const
{
	QStringList out;
	for (const QFileInfo &fi : QDir(AppSettings::profilesFolder()).entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
		QFile f(fi.filePath());
		if (f.open(QIODevice::ReadOnly))
			out << QJsonDocument::fromJson(f.readAll()).object().value("name").toString(fi.completeBaseName());
	}
	const QString cur = m_engine->settings().profile;
	if (!out.contains(cur))
		out.prepend(cur);
	return out;
}

QString App::switchProfile(const QString &name)
{
	AppSettings s = m_engine->settings();
	if (name == s.profile)
		return {};
	if (live())
		return "Stop streaming and recording to switch profiles";
	QFile f(profileFile(name));
	if (!f.open(QIODevice::ReadOnly))
		return "No profile called " + name;
	const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
	f.close();
	writeProfile(s.profile, s);
	const int w = s.video.outWidth, h = s.video.outHeight, fps = s.video.fps;
	s.applyProfile(o);
	s.profile = name;
	m_engine->setSettings(s);
	s.save();
	if (s.video.outWidth != w || s.video.outHeight != h || s.video.fps != fps)
		if (!m_engine->resetVideo())
			showToast(m_engine->lastError());
	emit settingsChanged();
	showToast("Profile: " + name);
	return {};
}

QString App::newProfile(const QString &name)
{
	const QString n = name.trimmed();
	if (n.isEmpty())
		return "Name the profile";
	if (profiles().contains(n, Qt::CaseInsensitive))
		return "There's already a profile called " + n;
	AppSettings s = m_engine->settings();
	writeProfile(s.profile, s);
	if (!writeProfile(n, s))
		return "Couldn't save the profile";
	s.profile = n;
	m_engine->setSettings(s);
	s.save();
	emit settingsChanged();
	return {};
}

QString App::renameProfile(const QString &from, const QString &to)
{
	const QString n = to.trimmed();
	if (n.isEmpty() || n == from)
		return {};
	if (profiles().contains(n, Qt::CaseInsensitive) && n.compare(from, Qt::CaseInsensitive) != 0)
		return "There's already a profile called " + n;
	AppSettings s = m_engine->settings();
	if (from == s.profile) {
		writeProfile(n, s);
		s.profile = n;
		m_engine->setSettings(s);
		s.save();
	} else {
		QFile f(profileFile(from));
		if (!f.open(QIODevice::ReadOnly))
			return "No profile called " + from;
		AppSettings p;
		p.applyProfile(QJsonDocument::fromJson(f.readAll()).object());
		f.close();
		writeProfile(n, p);
	}
	if (profileFile(from) != profileFile(n))
		QFile::remove(profileFile(from));
	emit settingsChanged();
	return {};
}

QString App::deleteProfile(const QString &name)
{
	if (name == m_engine->settings().profile)
		return "Switch to another profile before deleting this one";
	if (!QFile::remove(profileFile(name)))
		return "No profile called " + name;
	emit settingsChanged();
	return {};
}

void App::toggleRecording()
{
	m_engine->recording() ? m_engine->stopRecording() : m_engine->startRecording();
}

void App::toggleReplay()
{
	if (m_engine->replayActive())
		m_engine->stopReplay();
	else if (!m_engine->startReplay())
		showToast(m_engine->lastError());
	emit engineChanged();
}

void App::saveReplay()
{
	if (!m_engine->replayActive())
		return showToast("Turn on the replay buffer first");
	m_engine->saveReplay();
}

void App::toggleVirtualCam()
{
	if (m_engine->vcamActive())
		m_engine->stopVirtualCam();
	else if (!m_engine->startVirtualCam())
		showToast(m_engine->lastError());
	emit engineChanged();
}

void App::showRecordings()
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(m_engine->settings().recordFolder()));
}

QVariantList App::hotkeyActions() const
{
	QVariantList out;
	const auto add = [&](const QString &group, const QString &id, const QString &label) { out << QVariantMap{{"group", group}, {"id", id}, {"label", label}}; };
	add("Output", "stream", "Start or stop streaming");
	add("Output", "record", "Start or stop recording");
	add("Output", "replay", "Turn the replay buffer on or off");
	add("Output", "replaySave", "Save replay");
	add("Output", "vcam", "Start or stop the virtual camera");
	for (const QString &id : doc().sceneOrder)
		add("Scenes", "scene:" + id, "Show " + doc().scenes[id].name);
	for (const QString &id : doc().sourceOrder) {
		const Source &s = doc().sources[id];
		if (!s.hasAudio)
			continue;
		add("Audio", "mute:" + id, "Mute or unmute " + s.name);
		add("Audio", "ptt:" + id, "Push to talk: " + s.name);
	}
	return out;
}

void App::setHotkey(const QString &action, const QString &combo)
{
	QVariantMap keys = m_engine->settings().hotkeys;
	if (combo.isEmpty())
		keys.remove(action);
	else {
		for (auto it = keys.begin(); it != keys.end();)
			it = (it.value().toString() == combo && it.key() != action) ? keys.erase(it) : std::next(it);
		keys[action] = combo;
	}
	saveSettings({{"hotkeys", keys}});
	rebuildHotkeys();
}

void App::rebuildHotkeys()
{
	QString sig;
	for (const QString &id : doc().sceneOrder)
		sig += id + "=" + doc().scenes.value(id).name + ";";
	for (const QString &id : doc().sourceOrder)
		sig += id + "=" + doc().sources.value(id).name + ";";
	sig += QJsonDocument(QJsonObject::fromVariantMap(m_engine->settings().hotkeys)).toJson(QJsonDocument::Compact);
	if (sig == m_hotkeySig)
		return;
	m_hotkeySig = sig;
	QStringList valid;
	for (const QVariant &v : hotkeyActions())
		valid << v.toMap()["id"].toString();
	const QVariantMap keys = m_engine->settings().hotkeys;
	m_hotkeys->apply(keys, valid);
	QSet<QString> ptt;
	for (auto it = keys.begin(); it != keys.end(); ++it)
		if (it.key().startsWith("ptt:") && valid.contains(it.key()) && !it.value().toString().isEmpty())
			ptt.insert(it.key().mid(4));
	m_engine->setPushToTalk(ptt);
}

void App::runHotkey(const QString &action, bool pressed)
{
	if (action.startsWith("ptt:"))
		return m_engine->pushToTalk(action.mid(4), pressed);
	if (!pressed)
		return;
	if (action == "stream") toggleStreaming();
	else if (action == "record") toggleRecording();
	else if (action == "replay") toggleReplay();
	else if (action == "replaySave") saveReplay();
	else if (action == "vcam") toggleVirtualCam();
	else if (action.startsWith("scene:") && doc().scenes.contains(action.mid(6))) {
		editScene(action.mid(6));
		take();
	} else if (action.startsWith("mute:") && doc().sources.contains(action.mid(5))) {
		const QString id = action.mid(5);
		setAudio(id, "muted", !doc().sources[id].audio.muted, true);
	}
}

void App::toggleStreaming()
{
	if (m_engine->streaming()) {
		m_engine->stopStreaming();
		return;
	}
	beginStream(false);
}

void App::startTestStream()
{
	if (!m_engine->streaming())
		beginStream(true);
}

void App::beginStream(bool test)
{
	if (m_preparingStream)
		return;
	const AppSettings s = m_engine->settings();
	auto start = [this, test](const QJsonObject &plan) {
		m_preparingStream = false;
		if (!m_engine->startStreaming(test, plan))
			showToast(m_engine->lastError());
		else {
			m_liveUnlock = false;
			if (test)
				showToast("Test stream started");
		}
		emit engineChanged();
	};
	if (!s.stream.service.startsWith("Twitch") || !s.stream.enhanced || s.stream.key.isEmpty())
		return start({});
	m_preparingStream = true;
	emit engineChanged();
	if (!m_net)
		m_net = new QNetworkAccessManager(this);
	QNetworkRequest r{QUrl(enhanced_broadcast::kConfigUrl)};
	r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	r.setTransferTimeout(10000);
	const QJsonObject body = enhanced_broadcast::request(s.stream.key, s.video.outWidth, s.video.outHeight, s.video.fps, obs_get_version_string(),
							     doc().verticalOn ? QSize(doc().verticalW, doc().verticalH) : QSize());
	QNetworkReply *rep = m_net->post(r, QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(rep, &QNetworkReply::finished, this, [this, rep, start] {
		rep->deleteLater();
		const QByteArray data = rep->readAll();
		const QJsonObject plan = QJsonDocument::fromJson(data).object();
		QString why = rep->error() != QNetworkReply::NoError && plan.isEmpty() ? "Twitch didn't answer (" + rep->errorString() + ")" : enhanced_broadcast::check(plan);
		if (why.isEmpty())
			return start(plan);
		qInfo("[lobs] Enhanced Broadcasting unavailable: %s. Answer: %s", qPrintable(why), data.left(2000).constData());
		showToast("Enhanced Broadcasting isn't available: " + why + ". Streaming in one quality.");
		start({});
	});
}

void App::setLiveUnlock(bool on)
{
	m_liveUnlock = on;
	emit engineChanged();
}
