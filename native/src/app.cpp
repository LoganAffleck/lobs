#include "app.h"
#include "model/audio_fx.h"
#include "shellmeta.h"
#include "shapes.h"
#include "projector.h"
#include <QScreen>
#include <util/base.h>
#include <obs.h>
#include <QElapsedTimer>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <QDir>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QDirIterator>
#include <QTcpSocket>
#include <QHostInfo>
#include <functional>
#include <memory>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QImageReader>
#include <QProcess>
#include <QSettings>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QUrl>
#include <cmath>

using namespace lobs;

static QVariantMap filterDefaults(const QString &type)
{
	if (type == "color") return {{"brightness", 0.0}, {"contrast", 0.0}, {"saturation", 0.0}, {"gamma", 0.0}, {"hue", 0.0}};
	if (type == "chroma_key") return {{"key_color", "#00ff00"}, {"similarity", 400}, {"smoothness", 80}};
	if (type == "sharpness") return {{"amount", 0.08}};
	if (type == "lut") return {{"path", ""}, {"amount", 1.0}};
	return {};
}

static QString filterLabel(const QString &type)
{
	if (type == "color") return "Color Correction";
	if (type == "chroma_key") return "Chroma Key";
	if (type == "sharpness") return "Sharpen";
	if (type == "lut") return "LUT";
	return type;
}

static Doc loadDoc(const QString &path)
{
	QFile f(path);
	if (f.open(QIODevice::ReadOnly)) {
		const QJsonDocument j = QJsonDocument::fromJson(f.readAll());
		if (j.isObject() && j["version"].toInt() == 1)
			return Doc::fromJson(j.object());
	}
	return Doc::sample();
}

static QString firstRealDevice(Engine *engine, const QString &kind)
{
	for (const QVariant &v : engine->listDevices({}, kind)) {
		const QVariantMap m = v.toMap();
		if (!m["name"].toString().contains("Virtual", Qt::CaseInsensitive) && !m["value"].toString().isEmpty())
			return m["value"].toString();
	}
	return {};
}

/// Sources that need a device but have none (e.g. a show made on another PC) get this PC's first real device.
static Doc withDefaultDevices(Doc d, Engine *engine)
{
	for (Source &s : d.sources)
		if ((s.kind == "camera" || s.kind == "display" || s.kind == "mic" || s.kind == "desktop_audio") && s.settings.value("device").toString().isEmpty())
			if (const QString dev = firstRealDevice(engine, s.kind); !dev.isEmpty())
				s.settings["device"] = dev;
	return d;
}

App::App(Engine *engine, const QString &docPath, QObject *parent)
	: QObject(parent), m_engine(engine), m_store(withDefaultDevices(loadDoc(docPath), engine)), m_docPath(docPath)
{
	connect(&m_store, &Store::changed, this, &App::onStoreChanged);
	connect(engine, &Engine::statsChanged, this, &App::engineChanged);
	connect(engine, &Engine::statusChanged, this, &App::engineChanged);
	connect(engine, &Engine::recordingChanged, this, &App::engineChanged);
	m_toastTimer.setSingleShot(true);
	connect(&m_toastTimer, &QTimer::timeout, this, [this]() { m_toast.clear(); emit toastChanged(); });
	connect(engine, &Engine::streamingChanged, this, &App::engineChanged);
	connect(engine, &Engine::streamStatsChanged, this, &App::streamStatsChanged);
	connect(engine, &Engine::streamError, this, [this](const QString &m) { showToast(m); });
	connect(&m_levelTimer, &QTimer::timeout, this, [this]() { m_levels = m_engine->audioLevels(); emit levelsChanged(); });
	m_levelTimer.start(50);
	connect(&m_thumbTimer, &QTimer::timeout, this, [this]() { m_thumbTick++; emit thumbTickChanged(); });
	connect(&m_placeTimer, &QTimer::timeout, this, [this]() { resolvePlacements(++m_placeTries >= 40); });
	m_hotkeys = new Hotkeys([this](const QString &a, bool pressed) { runHotkey(a, pressed); }, this);
	QTimer::singleShot(0, this, &App::rebuildHotkeys);
	connect(engine, &Engine::replayChanged, this, &App::engineChanged);
	connect(engine, &Engine::vcamChanged, this, &App::engineChanged);
	connect(engine, &Engine::replaySaved, this, [this](const QString &path) { showToast("Replay saved: " + QFileInfo(path).fileName()); });
	// Self-tests run on a throwaway document in the temp folder: they never see your project.
	if (!QDir::cleanPath(QFileInfo(m_docPath).absolutePath()).startsWith(QDir::cleanPath(QDir::tempPath())))
		m_projectFile = QSettings().value("project/file").toString();
	if (!m_projectFile.isEmpty() && !QFile::exists(m_projectFile))
		m_projectFile.clear();
	watchAssets();
	connect(&m_assetWatch, &QFileSystemWatcher::directoryChanged, this, [this]() { emit assetsChanged(); });
	QTimer::singleShot(0, this, &App::adoptExternalFiles);
	m_pixelPerfect = QSettings().value("ui/preview/pixelPerfect", true).toBool();
	m_toolFill = uiValue("tools/fill", "#ef4444");
	if (m_toolFill == "#7c5cff") // the old default
		m_toolFill = "#ef4444";
	m_toolStroke = uiValue("tools/stroke", "#ffffff");
	m_toolStrokeWidth = uiValue("tools/strokeWidth", "6").toInt();
	m_laserTrailMs = std::clamp(uiValue("tools/laserTrailMs", "500").toInt(), 150, 3000);
	m_toolFont = uiValue("tools/font", "Segoe UI");
	m_toolBold = uiValue("tools/bold", "1") == "1";
	m_toolItalic = uiValue("tools/italic", "0") == "1";
	connect(&m_eyedropper, &Eyedropper::picked, this, &App::screenColorPicked);
	connect(&m_eyedropper, &Eyedropper::canceled, this, &App::screenColorCanceled);
	connect(m_engine, &Engine::bandwidthTestProgress, this, &App::bandwidthProgress);
	connect(m_engine, &Engine::bandwidthTestDone, this, &App::bandwidthDone);
	m_caretTimer.setInterval(530);
	connect(&m_caretTimer, &QTimer::timeout, this, [this]() { m_caretOn = !m_caretOn; layoutEditedText(); });
	m_engine->setPixelPerfect(m_pixelPerfect);
	connect(&m_statusTimer, &QTimer::timeout, this, &App::refreshSourceStatus);
	m_statusTimer.start(1000);
	m_saveTimer.setSingleShot(true);
	connect(&m_saveTimer, &QTimer::timeout, this, &App::autosave);

	m_engine->apply(doc());
	if (!doc().sceneOrder.isEmpty()) {
		m_editScene = doc().sceneOrder.first();
		m_engine->setEditScene(m_editScene);
		m_engine->setProgramScene(m_editScene);
	}
}

void App::onStoreChanged()
{
	const Doc &d = doc();
	QElapsedTimer pt;
	pt.start();
	m_engine->apply(d);
	m_perfApplyUs += pt.nsecsElapsed() / 1000;
	QStringList sel;
	for (const QString &id : m_selection)
		if (d.instances.contains(id))
			sel << id;
	m_selection = sel;
	if (!d.scenes.contains(m_editScene)) {
		m_editScene = d.sceneOrder.value(0);
		m_selection.clear();
	}
	pt.restart();
	m_engine->setEditScene(m_editScene);
	if (!d.scenes.contains(m_engine->programScene()))
		m_engine->setProgramScene(m_editScene);
	updateOverlay();
	m_perfEditUs += pt.nsecsElapsed() / 1000;
	m_saveTimer.start(400);
	pt.restart();
	rebuildHotkeys(); // scenes and sources come and go
	m_perfHotkeyUs += pt.nsecsElapsed() / 1000;
	pt.restart();
	emit docChanged();
	m_perfQmlUs += pt.nsecsElapsed() / 1000;
}

void App::autosave()
{
	QDir().mkpath(QFileInfo(m_docPath).absolutePath());
	QFile f(m_docPath);
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		f.write(QJsonDocument(doc().toJson()).toJson());
}

void App::showToast(const QString &msg)
{
	m_toast = msg;
	emit toastChanged();
	m_toastTimer.start(2200);
}

// ---------- QML-facing data ----------

QVariantList App::scenes() const
{
	QVariantList out;
	for (const QString &id : doc().sceneOrder) {
		const Scene &s = doc().scenes[id];
		out << QVariantMap{{"id", id}, {"name", s.name}, {"count", s.layers.size()}, {"favorite", s.favorite},
				   {"editing", id == m_editScene}, {"program", id == m_engine->programScene()}};
	}
	return out;
}

static QString assetKind(const QString &file);

// Kinds that aren't inputs: files come from the Asset Library, text and color are made as layers.
static bool isAssetKind(const QString &kind) { return kind == "image" || kind == "media" || kind == "text" || kind == "color" || kind == "shape"; }

// Asset-backed sources exist only for their layers: drop them once no layer uses them.
static void pruneUnusedAssetSources(Doc &d)
{
	QSet<QString> used;
	for (const Instance &i : d.instances)
		used.insert(i.sourceId);
	for (const QString &id : QStringList(d.sourceOrder))
		if (isAssetKind(d.sources[id].kind) && !used.contains(id))
			d.deleteSource(id);
}

// Sources panel sections: unbinned inputs are grouped by what they are; your own bins follow.
static QString poolCategory(const QString &kind)
{
	if (kind == "camera") return "Cameras";
	if (kind == "display" || kind == "window" || kind == "game") return "Screen capture";
	if (kind == "mic" || kind == "desktop_audio") return "Audio";
	return "Media"; // browser sources
}

QVariantList App::pool() const
{
	QVariantList out;
	struct Group { QString name, bin, category; };
	QVector<Group> groups;
	QSet<QString> inScene;            // sources with a layer in the edited scene
	QHash<QString, QStringList> usedIn; // source -> scene names
	for (const Instance &i : doc().instances) {
		if (i.sceneId == m_editScene)
			inScene.insert(i.sourceId);
		const QString sn = doc().scenes.value(i.sceneId).name;
		if (!usedIn[i.sourceId].contains(sn))
			usedIn[i.sourceId] << sn;
	}
	for (const char *c : {"Cameras", "Screen capture", "Audio", "Media"})
		groups << Group{c, "", c};
	for (const QString &b : doc().bins)
		groups << Group{b, b, ""};
	for (const Group &g : groups) {
		bool header = false;
		for (const QString &id : doc().sourceOrder) {
			const Source &s = doc().sources[id];
			if (isAssetKind(s.kind))
				continue; // files are in the Asset Library; text and color are plain layers
			if (s.bin != g.bin || (g.bin.isEmpty() && poolCategory(s.kind) != g.category))
				continue;
			if (!header) {
				out << QVariantMap{{"header", true}, {"name", g.name}};
				header = true;
			}
			int uses = 0;
			for (const auto &i : doc().instances)
				uses += i.sourceId == id;
			const KindInfo &k = kindInfo(s.kind);
			out << QVariantMap{{"header", false}, {"id", id}, {"name", s.name}, {"kind", s.kind}, {"icon", k.icon}, {"color", s.color},
					   {"uses", uses}, {"video", k.video}, {"selected", id == m_poolSelection}, {"indent", false}, {"kindLabel", k.label},
					   {"inScene", inScene.contains(id)}, {"scenes", usedIn.value(id)}, {"muted", s.audio.muted}, {"hasAudio", s.hasAudio}};
		}
	}
	return out;
}


static bool isBackgroundLayer(const Doc &d, const QString &id); // defined with the scene helpers

QVariantList App::layers() const
{
	QVariantList out;
	if (!doc().scenes.contains(m_editScene))
		return out;
	std::function<void(const QStringList &, int)> walk = [&](const QStringList &ids, int depth) {
		for (const QString &id : ids) {
			const Instance &i = doc().instances[id];
			auto sit = doc().sources.constFind(i.sourceId);
			const Source *s = sit != doc().sources.constEnd() ? &*sit : nullptr;
			bool linked = false;
			for (Group g : kGroups)
				linked |= !doc().linkedPeers(i, g).isEmpty();
			out << QVariantMap{{"id", id}, {"name", i.name}, {"depth", depth}, {"group", i.kind == "group"},
					   {"icon", i.kind == "group" ? "layers" : (s ? kindInfo(s->kind).icon : "square")}, {"color", s ? s->color : "#555"},
					   {"visible", doc().visibility(i).visible}, {"locked", i.locked}, {"linked", linked},
					   {"fx", i.kind == "group" ? 0 : doc().filters(i).size()}, {"selected", m_selection.contains(id)},
					   {"background", isBackgroundLayer(doc(), id)}};
			if (i.kind == "group")
				walk(i.children, depth + 1);
		}
	};
	walk(doc().scenes[m_editScene].layers, 0);
	return out;
}

QString App::singleSelection() const
{
	return m_selection.size() == 1 ? m_selection.first() : QString();
}

QVariantMap App::inspector() const
{
	const Doc &d = doc();
	if (m_selection.size() > 1)
		return {{"mode", "multi"}, {"count", m_selection.size()}};
	const QString id = singleSelection();
	if (!id.isEmpty()) {
		const Instance &i = d.instances[id];
		const Transform t = d.bounds(id);
		const Visibility v = d.visibility(i);
		QVariantMap m{{"mode", i.kind == "group" ? "group" : "layer"}, {"id", id}, {"name", i.name},
			      {"x", t.x}, {"y", t.y}, {"w", t.w}, {"h", t.h}, {"rot", t.rot}, {"flipH", t.flipH}, {"flipV", t.flipV}, {"visible", v.visible}, {"opacity", v.opacity}};
		auto linkInfo = [&](Group g) {
			QStringList where;
			for (const Instance *p : d.linkedPeers(i, g))
				where << d.scenes.value(p->sceneId).name;
			return where.join(", ");
		};
		m["linkTransform"] = linkInfo(Group::Transform);
		m["linkCrop"] = linkInfo(Group::Crop);
		m["linkFilters"] = linkInfo(Group::Filters);
		m["linkVisibility"] = linkInfo(Group::Visibility);
		// Per section: who it's linked with, what it could link with (the same source placed in
		// other scenes), and whether it differs from its defaults (so a reset is worth offering).
		const PropMap def = d.defaultProps(i.kind == "group" ? nullptr : &d.sources[i.sourceId]);
		QVariantMap sections;
		for (Group g : kGroups) {
			QVariantList peers, candidates;
			QSet<QString> peerIds;
			for (const Instance *p : d.linkedPeers(i, g)) {
				peers << QVariantMap{{"id", p->id}, {"scene", d.scenes.value(p->sceneId).name}};
				peerIds.insert(p->id);
			}
			if (i.kind != "group")
				for (const auto &o : d.instances)
					if (o.id != i.id && o.sourceId == i.sourceId && !peerIds.contains(o.id))
						candidates << QVariantMap{{"id", o.id}, {"scene", d.scenes.value(o.sceneId).name + (o.sceneId == i.sceneId ? " (this scene)" : "")}};
			bool isDefault = true;
			switch (g) {
			case Group::Transform: isDefault = i.kind == "group" || d.transform(i) == def.transform; break;
			case Group::Crop: isDefault = d.crop(i) == lobs::Crop{}; break;
			case Group::Filters: isDefault = d.filters(i).isEmpty(); break;
			case Group::Visibility: isDefault = d.visibility(i) == Visibility{}; break;
			}
			sections[groupKey(g)] = QVariantMap{{"peers", peers}, {"candidates", candidates}, {"isDefault", isDefault}};
		}
		m["sections"] = sections;
		if (i.kind != "group") {
			const Source &s = d.sources[i.sourceId];
			const Crop c = d.crop(i);
			m["cl"] = c.l, m["ct"] = c.t, m["cr"] = c.r, m["cb"] = c.b, m["radius"] = c.radius;
			m["sourceId"] = s.id;
			m["sourceName"] = s.name;
			m["sourceIcon"] = kindInfo(s.kind).icon;
			int uses = 0;
			QSet<QString> scenesUsing;
			for (const auto &o : d.instances)
				if (o.sourceId == s.id)
					uses++, scenesUsing.insert(o.sceneId);
			m["uses"] = uses;
			m["sceneCount"] = scenesUsing.size();
			const QSize ns = m_engine->sourceSize(s.id);
			m["nativeSize"] = ns.isEmpty() ? QString() : QString("%1×%2").arg(ns.width()).arg(ns.height());
			m["croppable"] = !ns.isEmpty() && s.kind != "text" && s.kind != "shape" && s.kind != "color";
			// Aspect of what's visible after cropping (w/h), to mark the matching preset.
			if (!ns.isEmpty())
				m["visibleAspect"] = double(ns.width() - c.l - c.r) / std::max(1, ns.height() - c.t - c.b);
			QVariantList fs;
			for (const Filter &f : d.filters(i)) {
				QVariantList params;
				const QVariantMap def = filterDefaults(f.type);
				for (auto it = def.begin(); it != def.end(); ++it) {
					const QVariant val = f.settings.value(it.key(), *it);
					params << QVariantMap{{"key", it.key()}, {"value", val}, {"string", it->typeId() == QMetaType::QString}};
				}
				fs << QVariantMap{{"type", f.type}, {"label", filterLabel(f.type)}, {"enabled", f.enabled}, {"params", params}};
			}
			m["filters"] = fs;
			m["source"] = sourceInfo(s); // Source section at the top of the layer inspector
		}
		return m;
	}
	if (!m_poolSelection.isEmpty() && d.sources.contains(m_poolSelection)) {
		QVariantMap m = sourceInfo(d.sources[m_poolSelection]);
		m["mode"] = "source";
		return m;
	}
	return {{"mode", "none"}};
}

QVariantList App::history() const
{
	QVariantList out;
	for (const HistoryEntry &h : m_store.history())
		out << QVariantMap{{"label", h.label}, {"time", h.at.toString("HH:mm:ss")}};
	return out;
}

QString App::editSceneName() const
{
	return doc().scenes.value(m_editScene).name;
}

QStringList App::filterTypes() const
{
	return {"color", "chroma_key", "sharpness", "lut"};
}

// ---------- scenes ----------

void App::editScene(const QString &id)
{
	if (!doc().scenes.contains(id))
		return;
	m_editScene = id;
	m_selection.clear();
	m_engine->setEditScene(id);
	updateOverlay();
	emit docChanged();
}

void App::take()
{
	m_engine->setProgramScene(m_editScene, true);
	showToast("On program: " + editSceneName());
	emit docChanged();
}

static bool isBackgroundLayer(const Doc &d, const QString &id)
{
	const Instance &i = d.instances.value(id);
	if (i.kind == "group" || !d.scenes.contains(i.sceneId))
		return false;
	const Source &s = d.sources.value(i.sourceId);
	if (s.kind != "color")
		return false;
	if (s.settings.value("background").toBool())
		return true;
	const QStringList &top = d.scenes[i.sceneId].layers;
	return !top.isEmpty() && top.last() == id && s.name.startsWith("Background");
}

// A black color layer at the very bottom of a scene. New scenes start with one; it can be
// deleted (scenes used inside other scenes need to be see-through) and added back.
static void placeBackground(Doc &d, const QString &sceneId)
{
	const QString sid = d.addSource("color", "Background", {{"color", "#000000"}, {"background", true}}).id;
	const QString iid = d.placeSource(sceneId, sid).id;
	QStringList &layers = d.scenes[sceneId].layers;
	layers.removeAll(iid);
	layers.append(iid); // layers are top-first
}

void App::addScene()
{
	QString id;
	m_store.commit("Add scene", [&](Doc &d) {
		id = d.addScene("Scene").id;
		placeBackground(d, id);
	});
	editScene(id);
}

bool App::editSceneHasBackground() const
{
	if (!doc().scenes.contains(m_editScene))
		return true;
	for (const QString &id : doc().scenes[m_editScene].layers)
		if (isBackgroundLayer(doc(), id))
			return true;
	return false;
}

void App::setSwatches(const QStringList &list)
{
	QStringList clean;
	for (const QString &c : list)
		if (QColor::isValidColorName(c) && !clean.contains(c.toLower()))
			clean << c.toLower();
	if (clean == doc().swatches)
		return;
	const QString label = clean.size() > doc().swatches.size() ? "Save swatch" : clean.size() < doc().swatches.size() ? "Remove swatch" : "Change swatches";
	m_store.commit(label, [&](Doc &d) { d.swatches = clean; });
}

void App::clearSelection()
{
	if (m_selection.isEmpty() && m_poolSelection.isEmpty())
		return;
	endTextEdit();
	m_selection.clear();
	m_poolSelection.clear();
	updateOverlay();
	emit docChanged();
}

// ---------- transitions ----------

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
	// Number repeats: "Swipe 2".
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
	// The engine's obs-transitions data folder.
	QStringList out;
	QDirIterator it(QCoreApplication::applicationDirPath() + "/engine", {"wipes.json"}, QDir::Files, QDirIterator::Subdirectories);
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
	// Screenshots go public: no screen, window or game capture in any scene (they'd show the
	// real desktop), and every scene gets a plain background instead.
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
	// The tools demo draws on Starting Soon; the rest use the first scene with the camera.
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
			QTimer::singleShot(2500, this, [this, cam] { // after the camera reports its size
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

void App::addBackground()
{
	if (!doc().scenes.contains(m_editScene))
		return;
	m_store.commit("Add background", [&](Doc &d) { placeBackground(d, m_editScene); });
}

void App::renameScene(const QString &id, const QString &name)
{
	if (!name.trimmed().isEmpty())
		m_store.commit("Rename scene", [&](Doc &d) { d.scenes[id].name = name.trimmed(); });
}

void App::deleteScene(const QString &id)
{
	m_store.commit("Delete scene", [&](Doc &d) { d.deleteScene(id); });
}

void App::duplicateScene(const QString &id, bool linked)
{
	QString nid;
	m_store.commit(linked ? "Duplicate scene (linked)" : "Duplicate scene", [&](Doc &d) {
		Scene &n = d.addScene(d.scenes[id].name + " copy");
		nid = n.id;
		d.copyToScene(d.scenes[id].layers, nid, linked);
	});
	editScene(nid);
}

// ---------- pool ----------

void App::addSource(const QString &kind, const QString &name, const QString &extra, bool place)
{
	QVariantMap settings;
	if (kind == "text") settings = {{"text", extra.isEmpty() ? "Text" : extra}, {"size", 96}};
	else if (kind == "color") settings = {{"color", extra.isEmpty() ? "#1d1b2e" : extra}};
	else if (kind == "image" || kind == "media") settings = {{"file", extra}};
	else if (kind == "browser") {
		QString url = extra.trimmed();
		if (!url.isEmpty() && !url.contains("://") && !QFileInfo(url).isAbsolute())
			url.prepend("https://"); // "streamlabs.com/alert-box/..." just works
		settings = {{"url", url}};
	}
	else if (kind == "camera" || kind == "display" || kind == "mic" || kind == "desktop_audio") {
		if (const QString dev = firstRealDevice(m_engine, kind); !dev.isEmpty())
			settings = {{"device", dev}};
	}
	QString sid, iid;
	const KindInfo &k = kindInfo(kind);
	m_store.commit("Add " + (name.isEmpty() ? k.label : name), [&](Doc &d) {
		sid = d.addSource(kind, name.trimmed().isEmpty() ? k.label : name.trimmed(), settings).id;
		// Browser audio goes into the mix, not your speakers: monitor it by default so alerts are
		// heard. (Desktop Audio capturing the monitor device is deduplicated by libobs.)
		if (kind == "browser")
			d.sources[sid].audio.monitor = "monitor_and_output";
		if (place && k.video && d.scenes.contains(m_editScene))
			iid = d.placeSource(m_editScene, sid).id;
	});
	if (!iid.isEmpty())
		m_selection = {iid}, m_poolSelection.clear();
	else
		m_poolSelection = sid, m_selection.clear();
	updateOverlay();
	emit docChanged();
}

void App::placeSource(const QString &id)
{
	if (!doc().sources.contains(id) || !doc().scenes.contains(m_editScene))
		return;
	if (!kindInfo(doc().sources[id].kind).video)
		return showToast("Audio sources aren't placed on the canvas");
	QString iid;
	m_store.commit("Place " + doc().sources[id].name, [&](Doc &d) { iid = d.placeSource(m_editScene, id).id; });
	m_selection = {iid};
	m_poolSelection.clear();
	updateOverlay();
	emit docChanged();
}

void App::renameSource(const QString &id, const QString &name)
{
	if (!name.trimmed().isEmpty())
		m_store.commit("Rename source", [&](Doc &d) { d.sources[id].name = name.trimmed(); });
}

void App::deleteSource(const QString &id)
{
	m_store.commit("Delete source", [&](Doc &d) { d.deleteSource(id); });
	if (m_poolSelection == id)
		m_poolSelection.clear();
}

void App::selectSource(const QString &id)
{
	m_poolSelection = id;
	m_selection.clear();
	updateOverlay();
	emit docChanged();
}

QVariantList App::devices(const QString &sourceId)
{
	return doc().sources.contains(sourceId) ? m_engine->listDevices(sourceId, doc().sources[sourceId].kind) : QVariantList{};
}

void App::setSourceSetting(const QString &id, const QString &key, const QVariant &value)
{
	m_store.commit("Edit " + doc().sources.value(id).name, [&](Doc &d) { d.sources[id].settings[key] = value; });
}

// ---------- layers ----------

void App::selectLayer(const QString &id, bool additive)
{
	if (additive)
		m_selection.contains(id) ? (void)m_selection.removeAll(id) : m_selection.append(id);
	else
		m_selection = {id};
	m_poolSelection.clear();
	updateOverlay();
	emit docChanged();
}

void App::toggleVisible(const QString &id)
{
	Visibility v = doc().visibility(doc().instances[id]);
	v.visible = !v.visible;
	m_store.commit(v.visible ? "Show layer" : "Hide layer", [&](Doc &d) { d.setVisibility(id, v); });
}

void App::toggleLock(const QString &id)
{
	const bool lock = !doc().instances[id].locked;
	m_store.commit(lock ? "Lock layer" : "Unlock layer", [&](Doc &d) { d.instances[id].locked = lock; });
}

void App::moveLayer(const QString &id, int delta)
{
	if (isBackgroundLayer(doc(), id))
		return;
	// Stop above the background, which stays at the bottom.
	const QStringList &sib = doc().scenes.value(doc().instances.value(id).sceneId).layers;
	if (delta > 0 && sib.contains(id) && !sib.isEmpty() && isBackgroundLayer(doc(), sib.last()) && sib.indexOf(id) + delta >= sib.size() - 1)
		delta = int(sib.size()) - 2 - int(sib.indexOf(id));
	if (delta == 0)
		return;
	m_store.commit(delta < 0 ? "Move layer up" : "Move layer down", [&](Doc &d) { d.moveLayer(id, delta); });
}

void App::moveSelected(int delta)
{
	if (m_selection.isEmpty())
		return;
	const QStringList ids = m_selection;
	const QString label = delta <= -100 ? "Bring to front" : delta >= 100 ? "Send to back" : delta < 0 ? "Bring forward" : "Send backward";
	m_store.commit(label, [&](Doc &d) {
		// Move the ones nearest the destination first, so a multi-selection keeps its order.
		QStringList order = ids;
		std::sort(order.begin(), order.end(), [&](const QString &a, const QString &b) {
			const int ia = d.siblings(a).indexOf(a), ib = d.siblings(b).indexOf(b);
			return delta < 0 ? ia < ib : ia > ib;
		});
		for (const QString &id : order) {
			if (!d.instances.contains(id) || isBackgroundLayer(d, id))
				continue;
			int step = delta;
			const QStringList &sib = d.siblings(id);
			if (step > 0 && !sib.isEmpty() && isBackgroundLayer(d, sib.last())) // stay above the background
				step = std::min(step, int(sib.size()) - 2 - int(sib.indexOf(id)));
			if (step != 0)
				d.moveLayer(id, step);
		}
	});
}

void App::dropLayer(const QString &id, const QString &target, const QString &mode)
{
	const Doc &cur = doc();
	if (id == target || !cur.instances.contains(id) || !cur.instances.contains(target) || cur.descendants(id).contains(target))
		return;
	if (mode == "into" && cur.instances[target].kind != "group")
		return;
	if (isBackgroundLayer(cur, id) || (isBackgroundLayer(cur, target) && mode != "above"))
		return; // the background stays at the bottom
	const QString label = mode == "group" ? "Group layers" : mode == "into" ? "Move into group" : "Move layer";
	QString gid;
	m_store.commit(label, [&](Doc &d) {
		const Instance *oldParent = d.parentOf(id);
		const QString oldParentId = oldParent ? oldParent->id : QString();
		d.siblings(id).removeAll(id);
		if (mode == "into") {
			d.instances[target].children.prepend(id); // on top inside the group
		} else {
			QStringList &to = d.siblings(target);
			to.insert(to.indexOf(target) + (mode == "below" ? 1 : 0), id);
			if (mode == "group")
				gid = d.groupLayers({id, target});
		}
		// A group left empty goes away.
		if (!oldParentId.isEmpty() && d.instances.contains(oldParentId) && d.instances[oldParentId].children.isEmpty())
			d.deleteInstance(oldParentId);
	});
	m_selection = {gid.isEmpty() ? id : gid};
	updateOverlay();
	emit docChanged();
}

void App::renameLayer(const QString &id, const QString &name)
{
	if (!name.trimmed().isEmpty())
		m_store.commit("Rename layer", [&](Doc &d) { d.instances[id].name = name.trimmed(); });
}

void App::deleteSelected()
{
	if (m_selection.isEmpty())
		return;
	QStringList ids;
	for (const QString &id : m_selection)
		if (!isBackgroundLayer(doc(), id))
			ids << id;
	if (ids.isEmpty())
		return showToast("The background can't be deleted. Hide it instead.");
	m_store.commit(ids.size() > 1 ? QString("Delete %1 layers").arg(ids.size()) : "Delete layer", [&](Doc &d) {
		for (const QString &id : ids)
			d.deleteInstance(id);
		pruneUnusedAssetSources(d);
	});
}

void App::groupSelected()
{
	QString gid;
	const QStringList ids = m_selection;
	m_store.commit("Group layers", [&](Doc &d) { gid = d.groupLayers(ids); });
	if (!gid.isEmpty())
		m_selection = {gid}, updateOverlay(), emit docChanged();
}

void App::ungroupSelected()
{
	const QString id = singleSelection();
	if (!id.isEmpty() && doc().instances[id].kind == "group")
		m_store.commit("Ungroup", [&](Doc &d) { d.ungroup(id); });
}

void App::duplicateSelected()
{
	if (m_selection.isEmpty())
		return;
	QStringList made;
	const QStringList ids = m_selection;
	m_store.commit("Duplicate layer", [&](Doc &d) {
		made = d.copyToScene(ids, m_editScene, false);
		QSet<QString> seen;
		for (const QString &id : made)
			d.moveBy(id, 20, 20, seen);
	});
	m_selection = made;
	updateOverlay();
	emit docChanged();
}

void App::copySelectedTo(const QString &sceneId, bool linked)
{
	if (m_selection.isEmpty() || !doc().scenes.contains(sceneId))
		return;
	const QStringList ids = m_selection;
	m_store.commit(QString("Copy to %1 (%2)").arg(doc().scenes[sceneId].name, linked ? "linked" : "independent"),
		       [&](Doc &d) { d.copyToScene(ids, sceneId, linked); });
	showToast(QString("Copied to %1 as %2").arg(doc().scenes[sceneId].name, linked ? "linked" : "independent"));
}

// ---------- inspector ----------

void App::setTransform(const QString &field, double value)
{
	const QString id = singleSelection();
	if (id.isEmpty())
		return;
	const Instance &i = doc().instances[id];
	if (i.kind == "group") {
		const Transform b = doc().bounds(id);
		m_store.commit("Move group", [&](Doc &d) {
			QSet<QString> seen;
			d.moveBy(id, field == "x" ? value - b.x : 0, field == "y" ? value - b.y : 0, seen);
		});
		return;
	}
	Transform t = doc().transform(i);
	if (field == "x") t.x = value;
	else if (field == "y") t.y = value;
	else if (field == "w") t.w = std::max(1.0, value);
	else if (field == "h") t.h = std::max(1.0, value);
	else if (field == "rot") t.rot = value;
	else if (field == "flipH") t.flipH = value != 0;
	else if (field == "flipV") t.flipV = value != 0;
	const QString label = field == "flipH" ? (t.flipH ? "Flip horizontally" : "Unflip horizontally")
			    : field == "flipV" ? (t.flipV ? "Flip vertically" : "Unflip vertically") : "Set " + field;
	m_store.commit(label, [&](Doc &d) { d.setTransform(id, t); });
}

// One edit from the Inspector: a commit, or a step of a scrub gesture (final ends it).
static void applyEdit(Store &store, const QString &label, const std::function<void(Doc &)> &fn, int mode)
{
	if (mode == 0)
		return store.commit(label, fn);
	store.gesture(fn);
	if (mode == 2)
		store.endGesture(label);
}

static Transform withField(Transform t, const QString &field, double value, bool keepAspect)
{
	if (field == "x") t.x = value;
	else if (field == "y") t.y = value;
	else if (field == "w") { const double w = std::max(1.0, value); if (keepAspect && t.w > 0) t.h = std::max(1.0, t.h * w / t.w); t.w = w; }
	else if (field == "h") { const double h = std::max(1.0, value); if (keepAspect && t.h > 0) t.w = std::max(1.0, t.w * h / t.h); t.h = h; }
	else if (field == "rot") t.rot = value;
	return t;
}

void App::scrubTransform(const QString &field, double value, bool final, bool keepAspect)
{
	const QString id = singleSelection();
	if (id.isEmpty())
		return;
	const int mode = final ? 2 : 1;
	if (doc().instances[id].kind == "group") {
		if (field != "x" && field != "y")
			return;
		// Groups move by a delta from where the gesture started.
		const Doc &base = m_store.gestureBase() ? *m_store.gestureBase() : doc();
		const Transform b = base.bounds(id);
		applyEdit(m_store, "Move group", [&](Doc &d) {
			QSet<QString> seen;
			d.moveBy(id, field == "x" ? value - b.x : 0, field == "y" ? value - b.y : 0, seen);
		}, mode);
		return;
	}
	const Doc &base = m_store.gestureBase() ? *m_store.gestureBase() : doc();
	const Transform t = withField(base.transform(base.instances[id]), field, value, keepAspect);
	applyEdit(m_store, "Set " + field, [&](Doc &d) { d.setTransform(id, t); }, mode);
}

void App::setSize(const QString &field, double value, bool keepAspect)
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return;
	const Transform t = withField(doc().transform(doc().instances[id]), field, value, keepAspect);
	m_store.commit("Resize", [&](Doc &d) { d.setTransform(id, t); });
}

// Sets a layer's crop and keeps its scale: the box shrinks or grows by what's trimmed, and
// what stays visible doesn't move or stretch. ns is the source's own size (empty: unknown).
static void cropKeepingScale(Doc &d, const QString &id, Crop nc, QSize ns)
{
	const Instance &inst = d.instances[id];
	const Crop oc = d.crop(inst);
	nc.radius = oc.radius;
	if (ns.isEmpty()) {
		d.setCrop(id, nc);
		return;
	}
	nc.l = std::clamp(nc.l, 0, ns.width() - 2), nc.r = std::clamp(nc.r, 0, ns.width() - 2 - nc.l);
	nc.t = std::clamp(nc.t, 0, ns.height() - 2), nc.b = std::clamp(nc.b, 0, ns.height() - 2 - nc.t);
	Transform t = d.transform(inst);
	const double sx = t.w / std::max(1, ns.width() - oc.l - oc.r), sy = t.h / std::max(1, ns.height() - oc.t - oc.b);
	t.x += (nc.l - oc.l) * sx;
	t.y += (nc.t - oc.t) * sy;
	t.w = std::max(1.0, (ns.width() - nc.l - nc.r) * sx);
	t.h = std::max(1.0, (ns.height() - nc.t - nc.b) * sy);
	d.setCrop(id, nc);
	d.setTransform(id, t);
}

void App::cropToAspect(double ratio)
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return;
	const QSize ns = m_engine->sourceSize(doc().instances[id].sourceId);
	if (ns.isEmpty())
		return showToast("This layer's size isn't known yet");
	Crop nc;
	if (ratio > 0) {
		const double W = ns.width(), H = ns.height();
		if (W / H > ratio) {
			const int cut = int(std::lround((W - H * ratio) / 2));
			nc.l = nc.r = cut;
		} else {
			const int cut = int(std::lround((H - W / ratio) / 2));
			nc.t = nc.b = cut;
		}
	}
	m_store.commit(ratio > 0 ? "Crop to aspect" : "Remove crop", [&](Doc &d) { cropKeepingScale(d, id, nc, ns); });
}

void App::setCornerRadius(double pct, bool final)
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return;
	const Doc &base = m_store.gestureBase() ? *m_store.gestureBase() : doc();
	Crop c = base.crop(base.instances[id]);
	c.radius = std::clamp(pct, 0.0, 50.0);
	applyEdit(m_store, "Round corners", [&](Doc &d) { d.setCrop(id, c); }, final ? 2 : 1);
}

void App::setCropMode(bool on)
{
	const QString id = singleSelection();
	if (on && (id.isEmpty() || doc().instances[id].kind == "group" || m_engine->sourceSize(doc().instances[id].sourceId).isEmpty()))
		on = false;
	if (on == m_cropMode)
		return;
	m_cropMode = on;
	updateOverlay();
	emit docChanged();
}

void App::scrubCrop(const QString &field, int value, bool final)
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return;
	const Doc &base = m_store.gestureBase() ? *m_store.gestureBase() : doc();
	Crop c = base.crop(base.instances[id]);
	value = std::max(0, value);
	if (field == "l") c.l = value;
	else if (field == "t") c.t = value;
	else if (field == "r") c.r = value;
	else if (field == "b") c.b = value;
	const QSize ns = m_engine->sourceSize(base.instances[id].sourceId);
	applyEdit(m_store, "Crop", [&](Doc &d) { cropKeepingScale(d, id, c, ns); }, final ? 2 : 1);
}

void App::setCrop(const QString &field, int value)
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return;
	Crop c = doc().crop(doc().instances[id]);
	value = std::max(0, value);
	if (field == "l") c.l = value;
	else if (field == "t") c.t = value;
	else if (field == "r") c.r = value;
	else if (field == "b") c.b = value;
	const QSize ns = m_engine->sourceSize(doc().instances[id].sourceId);
	m_store.commit("Crop", [&](Doc &d) { cropKeepingScale(d, id, c, ns); });
}

void App::setOpacity(double value, bool final)
{
	const QStringList ids = m_selection;
	auto fn = [&](Doc &d) {
		for (const QString &id : ids) {
			Visibility v = d.visibility(d.instances[id]);
			v.opacity = value;
			d.setVisibility(id, v);
		}
	};
	liveEdit("opacity", "Opacity", fn, final);
}

void App::setVisibleFlag(bool visible)
{
	const QStringList ids = m_selection;
	m_store.commit(visible ? "Show layer" : "Hide layer", [&](Doc &d) {
		for (const QString &id : ids) {
			Visibility v = d.visibility(d.instances[id]);
			v.visible = visible;
			d.setVisibility(id, v);
		}
	});
}

void App::fit(const QString &mode)
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return;
	const Instance &i = doc().instances[id];
	const QSize ns = m_engine->sourceSize(i.sourceId);
	const Crop c = doc().crop(i);
	Transform t = doc().transform(i);
	const double W = doc().canvasW, H = doc().canvasH;
	const double aspect = ns.isEmpty() ? t.w / std::max(1.0, t.h) : double(ns.width() - c.l - c.r) / std::max(1, ns.height() - c.t - c.b);
	if (mode == "center") {
		t.x = std::round((W - t.w) / 2), t.y = std::round((H - t.h) / 2);
	} else if (mode == "aspect") {
		t.h = std::round(t.w / aspect);
	} else {
		const double k = mode == "fit" ? std::min(W / aspect, H) : std::max(W / aspect, H);
		t.w = std::round(k * aspect), t.h = std::round(k);
		t.x = std::round((W - t.w) / 2), t.y = std::round((H - t.h) / 2), t.rot = 0;
	}
	m_store.commit(mode == "fit" ? "Fit to canvas" : mode == "fill" ? "Fill canvas" : mode == "center" ? "Center" : "Fix aspect",
		       [&](Doc &d) { d.setTransform(id, t); });
}

void App::addFilter(const QString &type)
{
	const QString id = singleSelection();
	if (id.isEmpty() || !filterTypes().contains(type))
		return;
	QVector<Filter> fs = doc().filters(doc().instances[id]);
	fs << Filter{uid("flt_"), type, true, filterDefaults(type)};
	m_store.commit("Add " + filterLabel(type), [&](Doc &d) { d.setFilters(id, fs); });
}

void App::removeFilter(int index)
{
	const QString id = singleSelection();
	if (id.isEmpty())
		return;
	QVector<Filter> fs = doc().filters(doc().instances[id]);
	if (index < 0 || index >= fs.size())
		return;
	fs.removeAt(index);
	m_store.commit("Remove filter", [&](Doc &d) { d.setFilters(id, fs); });
}

void App::toggleFilter(int index)
{
	const QString id = singleSelection();
	if (id.isEmpty())
		return;
	QVector<Filter> fs = doc().filters(doc().instances[id]);
	if (index < 0 || index >= fs.size())
		return;
	fs[index].enabled = !fs[index].enabled;
	m_store.commit("Toggle filter", [&](Doc &d) { d.setFilters(id, fs); });
}

void App::setFilterValue(int index, const QString &key, const QVariant &value, bool final)
{
	const QString id = singleSelection();
	if (id.isEmpty())
		return;
	auto fn = [&](Doc &d) {
		QVector<Filter> fs = d.filters(d.instances[id]);
		if (index >= 0 && index < fs.size()) {
			fs[index].settings[key] = value;
			d.setFilters(id, fs);
		}
	};
	liveEdit(QString("filter:%1:%2").arg(index).arg(key), "Edit filter", fn, final);
}

static bool groupFromName(const QString &name, Group &out)
{
	for (Group g : kGroups)
		if (groupKey(g) == name)
			return out = g, true;
	return false;
}

void App::linkGroup(const QString &group, const QString &targetId, bool keepMine)
{
	const QString id = singleSelection();
	Group g;
	if (id.isEmpty() || !groupFromName(group, g) || !doc().instances.contains(targetId))
		return;
	const QString where = doc().scenes.value(doc().instances[targetId].sceneId).name;
	m_store.commit(QString("Link %1 with %2").arg(group, where), [&](Doc &d) { d.linkTo(id, targetId, g, keepMine); });
	showToast(QString("%1 linked with %2").arg(group.left(1).toUpper() + group.mid(1), where));
}

void App::resetGroup(const QString &group)
{
	const QString id = singleSelection();
	Group g;
	if (id.isEmpty() || !groupFromName(group, g))
		return;
	const Instance &i = doc().instances[id];
	const PropMap def = doc().defaultProps(i.kind == "group" ? nullptr : &doc().sources[i.sourceId]);
	m_store.commit("Reset " + group, [&](Doc &d) {
		switch (g) {
		case Group::Transform: if (i.kind != "group") d.setTransform(id, def.transform); break;
		case Group::Crop: d.setCrop(id, {}); break;
		case Group::Filters: d.setFilters(id, {}); break;
		case Group::Visibility: d.setVisibility(id, {}); break;
		}
	});
}

void App::unlinkGroup(const QString &group)
{
	const QString id = singleSelection();
	for (Group g : kGroups)
		if (groupKey(g) == group && !id.isEmpty())
			m_store.commit("Unlink " + group, [&](Doc &d) { d.unlink(id, g); });
}

// A live (slider) edit previews on every move and lands in history as one step on release.
// Only one runs at a time: touching a different control first closes the open one, so a
// release that never arrived can't make the next slider start from stale values.
void App::liveEdit(const QString &key, const QString &label, const std::function<void(Doc &)> &fn, bool final)
{
	if (!m_liveKey.isEmpty() && m_liveKey != key)
		m_store.endGesture(m_liveLabel);
	m_liveKey = key;
	m_liveLabel = label;
	m_store.gesture(fn);
	if (final) {
		m_store.endGesture(label);
		m_liveKey.clear();
	}
}

// ---------- history & files ----------

void App::undo()
{
	if (m_store.canUndo()) {
		const QString label = m_store.undoLabel();
		m_store.undo();
		showToast("Undo: " + label);
	}
}

void App::redo()
{
	if (m_store.canRedo()) {
		const QString label = m_store.redoLabel();
		m_store.redo();
		showToast("Redo: " + label);
	}
}

void App::jumpHistory(int index)
{
	m_store.jumpTo(index);
}

void App::newDocument()
{
	m_store.replace(Doc(), "New document");
	m_poolSelection.clear();
	m_editScene.clear();
	addScene();
}

void App::loadSample()
{
	m_store.replace(withDefaultDevices(Doc::sample(), m_engine), "Load sample show");
	editScene(doc().sceneOrder.value(0));
}

// ---------- setup assistant ----------

// Times a TCP connect to each server. Names are looked up first so DNS doesn't count (a cold
// lookup can take longer than the connection and makes the first check random), and each
// server is tried three times; the best try counts.
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
			continue; // "auto" and the like
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
	// One timed connect; repeats until three tries are done.
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
	// Leave a quarter of the measured upload free for the rest of your connection.
	const int cap = service == "Twitch" ? 6000 : service.startsWith("YouTube") ? 9000 : 8000;
	const int bitrate = std::clamp(int(measured * 0.75 / 100) * 100, 1500, cap);
	int w = 1920, h = 1080, fps = 60;
	if (bitrate < 2500) w = 852, h = 480, fps = 30;
	else if (bitrate < 3500) w = 1280, h = 720, fps = 30;
	else if (bitrate < 4500) w = 1280, h = 720, fps = 60;
	else if (bitrate < 5800) w = 1600, h = 900, fps = 60;
	const QString enc = m_engine->resolveEncoder();
	const bool hardware = enc != "obs_x264";
	// Software encoding at 1080p60 is heavy; a step down keeps the game smooth.
	if (!hardware && h == 1080)
		w = 1280, h = 720;
	QString encName = enc;
	for (const QVariant &e : m_engine->encoders())
		if (e.toMap()["id"].toString() == enc)
			encName = e.toMap()["name"].toString();
	return {{"bitrate", bitrate}, {"outWidth", w}, {"outHeight", h}, {"fps", fps}, {"encoder", "auto"}, {"encoderName", encName}, {"hardware", hardware}, {"measured", measured}};
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

bool App::saveProject()
{
	if (m_projectFile.isEmpty())
		return false;
	QFile f(m_projectFile);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return showToast("Couldn't save " + m_projectFile), false;
	f.write(QJsonDocument(doc().toJson()).toJson());
	return true;
}

QString App::defaultProjectsFolder() const
{
	return QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/LOBS Projects");
}

static QString cleanFolder(const QString &folder)
{
	QString f = folder.trimmed();
	if (f.startsWith("file:"))
		f = QUrl(f).toLocalFile();
	return QDir::cleanPath(f);
}

QString App::checkNewProject(const QString &name, const QString &folder) const
{
	const QString n = name.trimmed();
	if (n.isEmpty())
		return "Give the project a name.";
	static const QRegularExpression bad(R"([<>:"/\\|?*])");
	if (n.contains(bad) || n.endsWith('.'))
		return "Names can't contain < > : \" / \\ | ? * or end with a period.";
	const QString f = cleanFolder(folder);
	if (f.isEmpty() || !QDir::isAbsolutePath(f))
		return "Choose where to keep it.";
	if (QFileInfo::exists(f + "/" + n))
		return "There's already a folder named \"" + n + "\" there.";
	return {};
}

bool App::createProject(const QString &name, const QString &folder)
{
	if (!checkNewProject(name, folder).isEmpty())
		return false;
	const QString dir = cleanFolder(folder) + "/" + name.trimmed();
	if (!QDir().mkpath(dir + "/assets"))
		return showToast("Couldn't create " + QDir::toNativeSeparators(dir)), false;
	saveProject(); // the project being left keeps its latest changes
	newDocument();
	const bool ok = saveAs(QUrl::fromLocalFile(dir + "/" + name.trimmed() + ".lobs.json"));
	emit projectChanged();
	if (ok)
		showToast("Created " + name.trimmed());
	return ok;
}

bool App::saveAs(const QUrl &url)
{
	// The show's folder becomes the project: its assets folder sits next to it.
	m_projectFile = url.toLocalFile();
	QSettings().setValue("project/file", m_projectFile);
	watchAssets();
	adoptExternalFiles();
	emit assetsChanged();
	QFile f(url.toLocalFile());
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return showToast("Couldn't save " + url.toLocalFile()), false;
	f.write(QJsonDocument(doc().toJson()).toJson());
	showToast("Saved " + QFileInfo(f).fileName());
	emit projectChanged();
	return true;
}

bool App::open(const QUrl &url)
{
	QFile f(url.toLocalFile());
	if (!f.open(QIODevice::ReadOnly))
		return showToast("Couldn't open " + url.toLocalFile()), false;
	const QJsonDocument j = QJsonDocument::fromJson(f.readAll());
	if (j.isObject() && isObsCollection(j.object()))
		return f.close(), importObs(url.toLocalFile());
	if (!j.isObject() || j["version"].toInt() != 1)
		return showToast("Not a LOBS document"), false;
	m_store.replace(withDefaultDevices(Doc::fromJson(j.object()), m_engine), "Open " + QFileInfo(f).fileName());
	m_projectFile = url.toLocalFile();
	QSettings().setValue("project/file", m_projectFile);
	watchAssets();
	adoptExternalFiles();
	emit assetsChanged();
	editScene(doc().sceneOrder.value(0));
	return true;
}

// ---------- OBS import ----------

QVariantList App::obsCollections() const
{
	QVariantList out;
	const QDir dir(qEnvironmentVariable("APPDATA") + "/obs-studio/basic/scenes");
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
	// Monitors and cameras the collection names but this PC doesn't have (unplugged, or the
	// IDs changed) fall back to this PC's first real device, like a new source would.
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
	// Read-only: the OBS file is never written. The import becomes a new LOBS document.
	m_store.replace(withDefaultDevices(imp.doc, m_engine), "Import " + name + " from OBS");
	editScene(imp.programScene);
	take();
	// Sources only report their size while running, so run every scene until sizes settle.
	m_engine->setAllScenesShowing(true);
	m_pendingPlacements = imp.pending;
	m_placeTries = 0;
	m_placeTimer.start(250);
	for (const QString &s : imp.skipped)
		qInfo("[import] skipped: %s", qPrintable(s));
	// One short line per kind of gap; the full list goes to the log.
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
	const int n = int(imp.doc.scenes.size());
	showToast(QString("Imported %1 scene%2 and %3 sources from OBS \"%4\".").arg(n).arg(n == 1 ? "" : "s").arg(imp.doc.sources.size()).arg(name) +
		  (gaps.isEmpty() ? QString() : " Not carried over: " + gaps.join(", ") + "."));
	return true;
}

// OBS stores scale factors, so a layer's pixel size waits on its source's natural size,
// which captures and cameras only report once running. Poll briefly, then settle.
void App::resolvePlacements(bool last)
{
	QVector<ObsPlacement> waiting;
	QVector<QPair<QString, Transform>> done;
	for (const ObsPlacement &p : m_pendingPlacements) {
		if (!doc().instances.contains(p.instId))
			continue; // deleted meanwhile
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
		m_engine->setAllScenesShowing(m_thumbTimer.isActive()); // back to what the scene grid needs
	}
}

void App::toggleRecording()
{
	m_engine->recording() ? m_engine->stopRecording() : m_engine->startRecording();
}

void App::nudge(int dx, int dy)
{
	if (m_selection.isEmpty())
		return;
	if (liveLocked())
		return showToast("Live lock is on. Unlock to nudge while live.");
	const QStringList ids = m_selection;
	m_store.commit("Nudge", [&](Doc &d) {
		QSet<QString> seen;
		for (const QString &id : ids)
			d.moveBy(id, dx, dy, seen);
	});
}

// ---------- media pool → canvas drag ----------

static Transform dropTransform(const Doc &d, const QString &sourceId, QPointF c)
{
	const PropMap p = d.defaultProps(&d.sources[sourceId]);
	Transform t = p.transform;
	if (t.w >= d.canvasW && t.h >= d.canvasH) // full-canvas sources (displays, games) drop full-canvas
		return t;
	t.x = std::round(c.x() - t.w / 2);
	t.y = std::round(c.y() - t.h / 2);
	return t;
}

void App::poolDragMove(const QString &sourceId, double x, double y)
{
	if (!doc().sources.contains(sourceId) || !kindInfo(doc().sources[sourceId].kind).video)
		return;
	const Transform t = dropTransform(doc(), sourceId, toCanvas({x, y}));
	OverlayBox b;
	b.rect = QRectF(t.x, t.y, t.w, t.h);
	b.drop = true;
	m_dropBoxes = {b};
	updateOverlay();
}

void App::poolDrop(const QString &sourceId, double x, double y)
{
	m_dropBoxes.clear();
	if (!doc().sources.contains(sourceId) || !doc().scenes.contains(m_editScene))
		return updateOverlay();
	if (!kindInfo(doc().sources[sourceId].kind).video) {
		updateOverlay();
		return showToast("Audio sources aren't placed on the canvas");
	}
	const Transform t = dropTransform(doc(), sourceId, toCanvas({x, y}));
	QString iid;
	m_store.commit("Place " + doc().sources[sourceId].name, [&](Doc &d) {
		iid = d.placeSource(m_editScene, sourceId).id;
		d.setTransform(iid, t);
	});
	m_selection = {iid};
	m_poolSelection.clear();
	updateOverlay();
	emit docChanged();
}

void App::poolDragCancel()
{
	m_dropBoxes.clear();
	updateOverlay();
}

void App::setGridVisible(bool on)
{
	m_engine->setAllScenesShowing(on);
	on ? m_thumbTimer.start(1000) : m_thumbTimer.stop();
	if (on) { m_thumbTick++; emit thumbTickChanged(); }
}

// ---------- going live ----------

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
		m_engine->setProgramScene(m_engine->programScene()); // rebuild with the new type, no cut
	if (s.video.outWidth != w || s.video.outHeight != h || s.video.fps != fps) {
		if (m_engine->live())
			showToast("Resolution and FPS changes apply after you stop streaming and recording");
		else if (!m_engine->resetVideo())
			showToast(m_engine->lastError());
	}
	emit settingsChanged();
	return true;
}

QString App::dataFolder() const
{
	return QDir::toNativeSeparators(QFileInfo(m_docPath).absolutePath());
}

// ---------- Sources panel: status ----------

// Each input's second line (device, format) and its problem, if any. Problems only show for
// inputs that are running (in a shown scene), so a camera that's simply not used isn't "broken".
void App::refreshSourceStatus()
{
	if (m_sourceTick % 30 == 0) // listing cameras can take a moment: refresh devices every 30 s
		m_deviceCache.clear();
	++m_sourceTick;
	QVariantMap out;
	for (const QString &id : doc().sourceOrder) {
		const Source &s = doc().sources[id];
		if (isAssetKind(s.kind))
			continue;
		const QString dev = s.settings.value("device").toString();
		QString detail, status, statusText;
		if (QStringList{"camera", "display", "window", "mic", "desktop_audio"}.contains(s.kind) || (s.kind == "game" && !dev.isEmpty())) {
			if (!m_deviceCache.contains(s.kind))
				m_deviceCache[s.kind] = m_engine->listDevices({}, s.kind);
			const QVariantList &devs = m_deviceCache[s.kind];
			QString devName;
			for (const QVariant &v : devs)
				if (v.toMap()["value"].toString() == dev)
					devName = v.toMap()["name"].toString();
			if (dev.isEmpty() || dev == "default")
				detail = s.kind == "mic" || s.kind == "desktop_audio" ? "Windows default device" : "No device chosen";
			else if (devName.isEmpty() && !devs.isEmpty() && s.kind != "window" && s.kind != "game")
				status = "missing", statusText = "Not connected";
			else
				// Device names carry extra detail after a colon (monitors: "LG 4K: 3840x2160 @ 0,0 (Primary)").
				detail = (devName.isEmpty() ? dev : devName).section(':', 0, 0).trimmed();
		} else if (s.kind == "game") {
			detail = s.settings.value("mode").toString() == "window" ? "A specific game window" : "Any fullscreen game";
		} else if (s.kind == "browser") {
			detail = QUrl(s.settings.value("url").toString()).host();
			if (detail.isEmpty())
				detail = s.settings.value("url").toString().isEmpty() ? "No URL yet" : "Local page";
		}
		const KindInfo &k = kindInfo(s.kind);
		bool hasSize = false;
		if (k.video) {
			const QSize ns = m_engine->sourceSize(id);
			hasSize = !ns.isEmpty();
			if (!ns.isEmpty())
				detail += (detail.isEmpty() ? "" : " · ") + QString("%1×%2").arg(ns.width()).arg(ns.height());
			else if (status.isEmpty() && m_engine->sourceActive(id))
				status = "nosignal", statusText = s.kind == "game" ? "Waiting for a game" : s.kind == "window" ? "Window not found" : "No signal";
		}
		out[id] = QVariantMap{{"detail", detail}, {"status", status}, {"statusText", statusText}, {"hasSize", hasSize}};
	}
	m_sourceStatus = out;
	emit sourceStatusChanged();
}

void App::toggleInScene(const QString &sourceId)
{
	if (!doc().sources.contains(sourceId) || !doc().scenes.contains(m_editScene))
		return;
	QStringList here;
	for (const Instance &i : doc().instances)
		if (i.sceneId == m_editScene && i.sourceId == sourceId)
			here << i.id;
	if (here.isEmpty())
		return placeSource(sourceId);
	const QString name = doc().sources[sourceId].name;
	m_store.commit("Remove " + name + " from " + editSceneName(), [&](Doc &d) {
		for (const QString &id : here)
			d.deleteInstance(id);
	});
	for (const QString &id : here)
		m_selection.removeAll(id);
	updateOverlay();
	emit docChanged();
}

// ---------- Asset Library ----------

static QString assetKind(const QString &file)
{
	static const QStringList images = {"png", "jpg", "jpeg", "gif", "webp", "bmp", "tga", "psd"};
	static const QStringList videos = {"mp4", "mov", "mkv", "webm", "avi", "flv", "ts", "m4v"};
	static const QStringList audio = {"mp3", "wav", "ogg", "flac", "m4a", "aac", "opus"};
	const QString ext = QFileInfo(file).suffix().toLower();
	return images.contains(ext) ? "image" : videos.contains(ext) ? "video" : audio.contains(ext) ? "audio" : "other";
}

QString App::assetsFolder() const
{
	const QString base = m_projectFile.isEmpty() ? QFileInfo(m_docPath).absolutePath() : QFileInfo(m_projectFile).absolutePath();
	return QDir::cleanPath(base + "/assets");
}

void App::watchAssets()
{
	if (!m_assetWatch.directories().isEmpty())
		m_assetWatch.removePaths(m_assetWatch.directories());
	QDir().mkpath(assetsFolder());
	if (!QDir(currentAssetDir()).exists())
		m_assetDir.clear(); // the folder went away (deleted in Explorer): back to the top
	m_assetWatch.addPath(currentAssetDir());
}

QString App::currentAssetDir() const
{
	return m_assetDir.isEmpty() ? assetsFolder() : QDir::cleanPath(assetsFolder() + "/" + m_assetDir);
}

bool App::insideAssets(const QString &path) const
{
	const QString p = QDir::cleanPath(QFileInfo(path).absoluteFilePath()), r = assetsFolder();
	return p.compare(r, Qt::CaseInsensitive) == 0 || p.startsWith(r + "/", Qt::CaseInsensitive);
}

int App::filesInUse(const QString &path) const
{
	const QString p = QDir::cleanPath(path);
	int n = 0;
	for (const Instance &i : doc().instances) {
		const Source &s = doc().sources.value(i.sourceId);
		const QString f = QDir::cleanPath(s.settings.value("file").toString());
		if ((s.kind == "image" || s.kind == "media") && (f.compare(p, Qt::CaseInsensitive) == 0 || f.startsWith(p + "/", Qt::CaseInsensitive)))
			++n;
	}
	return n;
}

// A file or folder moved on disk: point the show (and its undo history) at the new place.
void App::repointFiles(const QString &from, const QString &to)
{
	const QString a = QDir::cleanPath(from), b = QDir::cleanPath(to);
	m_store.rewriteAll([&](Doc &d) {
		for (Source &s : d.sources) {
			if (s.kind != "image" && s.kind != "media")
				continue;
			const QString f = QDir::cleanPath(s.settings.value("file").toString());
			if (f.compare(a, Qt::CaseInsensitive) == 0)
				s.settings["file"] = b;
			else if (f.startsWith(a + "/", Qt::CaseInsensitive))
				s.settings["file"] = b + f.mid(a.size());
		}
	});
}

QVariantList App::assetCrumbs() const
{
	QVariantList out{QVariantMap{{"name", "Assets"}, {"path", ""}}};
	QString acc;
	for (const QString &part : m_assetDir.split('/', Qt::SkipEmptyParts)) {
		acc += (acc.isEmpty() ? "" : "/") + part;
		out << QVariantMap{{"name", part}, {"path", acc}};
	}
	return out;
}

void App::goToAssetPath(const QString &relative)
{
	const QString target = relative.isEmpty() ? assetsFolder() : QDir::cleanPath(assetsFolder() + "/" + relative);
	if (!QDir(target).exists() || !insideAssets(target))
		return;
	m_assetDir = relative.isEmpty() ? QString() : QDir(assetsFolder()).relativeFilePath(target);
	watchAssets();
	emit assetsChanged();
}

void App::openAssetFolder(const QString &path)
{
	if (!QFileInfo(path).isDir() || !insideAssets(path))
		return;
	goToAssetPath(QDir(assetsFolder()).relativeFilePath(QDir::cleanPath(path)));
}

void App::assetUp()
{
	if (!m_assetDir.isEmpty())
		goToAssetPath(m_assetDir.contains('/') ? m_assetDir.section('/', 0, -2) : QString());
}

static QString uniqueIn(const QString &dir, const QString &name, bool isDir)
{
	const QFileInfo fi(name);
	const QString base = isDir ? name : fi.completeBaseName(), ext = isDir || fi.suffix().isEmpty() ? QString() : "." + fi.suffix();
	QString candidate = dir + "/" + name;
	for (int n = 2; QFileInfo::exists(candidate); ++n)
		candidate = dir + "/" + base + QString(" (%1)").arg(n) + ext;
	return candidate;
}

QString App::createAssetFolder()
{
	const QString path = uniqueIn(currentAssetDir(), "New folder", true);
	if (!QDir().mkpath(path))
		return showToast("Couldn't create a folder here"), QString();
	emit assetsChanged();
	return path;
}

bool App::renameAsset(const QString &path, const QString &name)
{
	QString n = name.trimmed();
	static const QRegularExpression bad(R"([\\/:*?"<>|])");
	if (n.isEmpty() || n.contains(bad) || n == "." || n == "..")
		return showToast("Names can't be empty or contain \\ / : * ? \" < > |"), false;
	const QFileInfo fi(path);
	if (!fi.exists() || !insideAssets(path))
		return false;
	if (fi.isFile() && QFileInfo(n).suffix().isEmpty() && !fi.suffix().isEmpty())
		n += "." + fi.suffix(); // keep the extension when it's left off
	const QString dest = fi.absolutePath() + "/" + n;
	if (QDir::cleanPath(dest) == QDir::cleanPath(fi.absoluteFilePath()))
		return true;
	if (QFileInfo::exists(dest) && dest.compare(fi.absoluteFilePath(), Qt::CaseInsensitive) != 0)
		return showToast("Something named \"" + n + "\" is already here"), false;
	if (!QDir().rename(fi.absoluteFilePath(), dest))
		return showToast("Couldn't rename " + fi.fileName() + " (is it open in another app?)"), false;
	repointFiles(fi.absoluteFilePath(), dest);
	emit assetsChanged();
	return true;
}

// The clipboard is Windows' own, so copy/cut/paste work with File Explorer in both directions.
// "Preferred DropEffect" marks a cut (2 = move) the way Explorer does.
static void putFilesOnClipboard(const QStringList &paths, bool cut)
{
	auto *mime = new QMimeData;
	QList<QUrl> urls;
	for (const QString &p : paths)
		urls << QUrl::fromLocalFile(p);
	mime->setUrls(urls);
	const quint32 effect = cut ? 2 : 5;
	mime->setData("Preferred DropEffect", QByteArray(reinterpret_cast<const char *>(&effect), 4));
	QGuiApplication::clipboard()->setMimeData(mime);
}

void App::copyAssets(const QStringList &paths)
{
	if (paths.isEmpty())
		return;
	putFilesOnClipboard(paths, false);
	showToast(paths.size() == 1 ? "Copied " + QFileInfo(paths.first()).fileName() : QString("Copied %1 items").arg(paths.size()));
}

void App::cutAssets(const QStringList &paths)
{
	if (paths.isEmpty())
		return;
	putFilesOnClipboard(paths, true);
	showToast(paths.size() == 1 ? "Cut " + QFileInfo(paths.first()).fileName() + ". Paste it into another folder." : QString("Cut %1 items").arg(paths.size()));
}

bool App::clipboardHasFiles() const
{
	const QMimeData *m = QGuiApplication::clipboard()->mimeData();
	if (!m || !m->hasUrls())
		return false;
	for (const QUrl &u : m->urls())
		if (u.isLocalFile() && QFileInfo::exists(u.toLocalFile()))
			return true;
	return false;
}

static bool copyRecursive(const QString &from, const QString &to)
{
	if (QFileInfo(from).isFile())
		return QFile::copy(from, to);
	if (!QDir().mkpath(to))
		return false;
	bool ok = true;
	for (const QFileInfo &fi : QDir(from).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot))
		ok &= copyRecursive(fi.absoluteFilePath(), to + "/" + fi.fileName());
	return ok;
}

void App::moveAssets(const QStringList &paths, const QString &destFolder)
{
	const QString dest = QDir::cleanPath(destFolder);
	if (!QFileInfo(dest).isDir() || !insideAssets(dest))
		return;
	int moved = 0;
	for (const QString &p : paths) {
		const QFileInfo fi(p);
		const QString src = QDir::cleanPath(fi.absoluteFilePath());
		if (!fi.exists() || QDir::cleanPath(fi.absolutePath()) == dest)
			continue; // already here
		if (dest.compare(src, Qt::CaseInsensitive) == 0 || dest.startsWith(src + "/", Qt::CaseInsensitive)) {
			showToast("A folder can't go inside itself");
			continue;
		}
		const QString target = uniqueIn(dest, fi.fileName(), fi.isDir());
		// Within the same drive this is a rename; across drives, copy then remove.
		bool ok = QDir().rename(src, target);
		if (!ok && copyRecursive(src, target))
			ok = fi.isDir() ? QDir(src).removeRecursively() : QFile::remove(src);
		if (!ok)
			continue;
		if (insideAssets(src))
			repointFiles(src, target);
		++moved;
	}
	if (moved)
		showToast(moved == 1 ? "Moved 1 item" : QString("Moved %1 items").arg(moved));
	emit assetsChanged();
}

void App::pasteAssets()
{
	const QMimeData *m = QGuiApplication::clipboard()->mimeData();
	if (!m || !m->hasUrls())
		return showToast("Nothing to paste. Copy files here or in File Explorer first.");
	QStringList paths;
	for (const QUrl &u : m->urls())
		if (u.isLocalFile() && QFileInfo::exists(u.toLocalFile()))
			paths << QDir::cleanPath(u.toLocalFile());
	const QByteArray fx = m->data("Preferred DropEffect");
	const bool cut = fx.size() >= 4 && (*reinterpret_cast<const quint32 *>(fx.constData()) & 2);
	if (cut) {
		moveAssets(paths, currentAssetDir());
		QGuiApplication::clipboard()->clear(); // a cut pastes once, like Explorer
		return;
	}
	int copied = 0;
	for (const QString &p : paths) {
		const QFileInfo fi(p);
		const QString dest = currentAssetDir();
		if (fi.isDir() && (dest.compare(p, Qt::CaseInsensitive) == 0 || dest.startsWith(p + "/", Qt::CaseInsensitive)))
			continue; // a folder can't be pasted into itself
		const QString target = uniqueIn(dest, fi.fileName(), fi.isDir());
		if (copyRecursive(p, target))
			++copied;
	}
	showToast(copied == 1 ? "Pasted 1 item" : QString("Pasted %1 items").arg(copied));
	emit assetsChanged();
}

void App::trashAssets(const QStringList &paths)
{
	int trashed = 0, blocked = 0;
	for (const QString &p : paths) {
		if (!insideAssets(p) || QDir::cleanPath(p) == assetsFolder())
			continue;
		if (filesInUse(p)) { // never pull media out from under the show
			++blocked;
			continue;
		}
		if (QFile::moveToTrash(p))
			++trashed;
	}
	if (blocked)
		showToast(QString("%1 item%2 in use by layers, so kept. Remove those layers first.").arg(blocked).arg(blocked == 1 ? " is" : "s are"));
	else if (trashed)
		showToast(trashed == 1 ? "Moved 1 item to the Recycle Bin" : QString("Moved %1 items to the Recycle Bin").arg(trashed));
	watchAssets();
	emit assetsChanged();
}

// One Asset Library entry (file or folder), with what the tiles and tooltips show.
QVariantMap App::assetEntry(const QFileInfo &fi, const QHash<QString, QStringList> &scenesByFile) const
{
	const QString path = QDir::cleanPath(fi.absoluteFilePath());
	const QString rel = QFileInfo(QDir(assetsFolder()).relativeFilePath(path)).path();
	const QString where = rel == "." ? QString() : rel;
	if (fi.isDir()) {
		const int items = int(QDir(path).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).size());
		return {{"name", fi.fileName()}, {"path", path}, {"kind", "folder"}, {"icon", "folder"}, {"folder", true}, {"color", folderColor(path)},
			{"size", items == 1 ? QString("1 item") : QString("%1 items").arg(items)}, {"bytes", 0}, {"modified", fi.lastModified().toMSecsSinceEpoch()},
			{"uses", filesInUse(path)}, {"scenes", QStringList()}, {"placeable", false}, {"where", where}};
	}
	const QString k = assetKind(path);
	const double kb = double(fi.size()) / 1024.0;
	QString dims;
	qint64 durMs = 0;
	if (k == "image" || k == "video" || k == "audio") {
		const ShellMeta m = shellMeta(path);
		if (!m.size.isEmpty())
			dims = QString("%1×%2").arg(m.size.width()).arg(m.size.height());
		durMs = m.durationMs;
	}
	const QStringList scenes = scenesByFile.value(path.toLower());
	return {{"name", fi.fileName()}, {"path", path}, {"url", QUrl::fromLocalFile(path).toString()}, {"kind", k},
		{"icon", k == "image" ? "image" : k == "video" ? "film" : k == "audio" ? "audio-lines" : "file"},
		{"size", kb > 1024 ? QString("%1 MB").arg(kb / 1024, 0, 'f', 1) : QString("%1 KB").arg(kb, 0, 'f', 0)},
		{"bytes", double(fi.size())}, {"modified", fi.lastModified().toMSecsSinceEpoch()},
		{"dims", dims}, {"durationMs", durMs}, {"ext", fi.suffix().toUpper()},
		{"uses", int(scenes.size()) ? usesOf(path) : 0}, {"scenes", scenes},
		{"thumb", (k == "image" || k == "video") ? "image://assetthumb/" + QString::fromLatin1(QUrl::toPercentEncoding(path)) + "?" + QString::number(fi.lastModified().toMSecsSinceEpoch()) : QString()},
		{"placeable", k != "other"}, {"folder", false}, {"where", where}};
}

int App::usesOf(const QString &file) const
{
	int n = 0;
	for (const Instance &i : doc().instances) {
		const Source &s = doc().sources.value(i.sourceId);
		if ((s.kind == "image" || s.kind == "media") && QDir::cleanPath(s.settings.value("file").toString()).compare(file, Qt::CaseInsensitive) == 0)
			++n;
	}
	return n;
}

QHash<QString, QStringList> App::scenesByFile() const
{
	QHash<QString, QStringList> out; // lower-cased path -> scene names using it
	for (const Instance &i : doc().instances) {
		const Source &s = doc().sources.value(i.sourceId);
		if (s.kind != "image" && s.kind != "media")
			continue;
		const QString f = QDir::cleanPath(s.settings.value("file").toString()).toLower();
		const QString sn = doc().scenes.value(i.sceneId).name;
		if (!out[f].contains(sn))
			out[f] << sn;
	}
	return out;
}

QVariantList App::assets() const
{
	const auto byFile = scenesByFile();
	QVariantList out;
	for (const QFileInfo &fi : QDir(currentAssetDir()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase))
		out << assetEntry(fi, byFile);
	for (const QFileInfo &fi : QDir(currentAssetDir()).entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase))
		if (fi.fileName() != ".lobsfolder")
			out << assetEntry(fi, byFile);
	return out;
}

QString App::folderColor(const QString &folder)
{
	QFile f(folder + "/.lobsfolder");
	if (!f.open(QIODevice::ReadOnly))
		return {};
	return QJsonDocument::fromJson(f.readAll()).object()["color"].toString();
}

void App::setFolderColor(const QString &folder, const QString &color)
{
	if (!QFileInfo(folder).isDir() || !insideAssets(folder))
		return;
	const QString file = folder + "/.lobsfolder";
	const std::wstring w = QDir::toNativeSeparators(file).toStdWString();
	SetFileAttributesW(w.c_str(), FILE_ATTRIBUTE_NORMAL); // hidden files can't be rewritten in place
	if (color.isEmpty()) {
		QFile::remove(file);
	} else {
		QFile f(file);
		if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
			f.write(QJsonDocument(QJsonObject{{"color", color}}).toJson(QJsonDocument::Compact));
		f.close();
		SetFileAttributesW(w.c_str(), FILE_ATTRIBUTE_HIDDEN); // out of the way in File Explorer
	}
	emit assetsChanged();
}

// Layers whose file is gone (moved or deleted outside LOBS): [{sourceId, name, file, layers}].
QVariantList App::missingAssets() const
{
	QVariantList out;
	for (const Source &s : doc().sources) {
		if (s.kind != "image" && s.kind != "media")
			continue;
		const QString f = s.settings.value("file").toString();
		if (f.isEmpty() || QFile::exists(f))
			continue;
		int layers = 0;
		for (const Instance &i : doc().instances)
			layers += i.sourceId == s.id;
		if (layers)
			out << QVariantMap{{"sourceId", s.id}, {"name", QFileInfo(f).fileName()}, {"file", f}, {"layers", layers}};
	}
	return out;
}

// Looks for each missing file by name anywhere in assets and points its layers there.
void App::findMissingAssets()
{
	int fixed = 0, notFound = 0;
	for (const QVariant &v : missingAssets()) {
		const QVariantMap m = v.toMap();
		QString match;
		QDirIterator it(assetsFolder(), QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext() && match.isEmpty()) {
			const QString p = it.next();
			if (QFileInfo(p).fileName().compare(m["name"].toString(), Qt::CaseInsensitive) == 0)
				match = p;
		}
		if (match.isEmpty()) {
			++notFound;
			continue;
		}
		repointFiles(m["file"].toString(), match);
		++fixed;
	}
	if (fixed)
		showToast(QString("Found %1 missing file%2 in assets").arg(fixed).arg(fixed == 1 ? "" : "s") + (notFound ? QString("; %1 still missing").arg(notFound) : QString()));
	else if (notFound)
		showToast("Not found in assets. Use Choose file…");
	emit assetsChanged();
	emit sourceStatusChanged();
}

// Points a missing file's layers at a file you choose (copied into the current folder).
void App::relinkAsset(const QString &sourceId, const QString &file)
{
	if (!doc().sources.contains(sourceId) || !QFile::exists(file))
		return;
	const QString old = doc().sources[sourceId].settings.value("file").toString();
	const QString copy = insideAssets(file) ? QDir::cleanPath(file) : copyIntoAssets(file);
	if (copy.isEmpty())
		return showToast("Couldn't copy that file into assets");
	repointFiles(old, copy);
	showToast("Relinked " + QFileInfo(copy).fileName());
	emit assetsChanged();
	emit sourceStatusChanged();
}

// Copies a file into the assets folder (unless it's already there) and returns the copy's path.
// A different file with the same name gets " (2)", " (3)", ...; the same file is reused.
QString App::copyIntoAssets(const QString &file, const QString &destDir)
{
	const QFileInfo src(file);
	const QString dir = destDir.isEmpty() ? currentAssetDir() : destDir;
	if (!src.isFile())
		return {};
	if (QDir::cleanPath(src.absolutePath()) == QDir::cleanPath(dir))
		return QDir::cleanPath(src.absoluteFilePath());
	QDir().mkpath(dir);
	QString dest = dir + "/" + src.fileName();
	for (int n = 2; QFile::exists(dest); ++n) {
		const QFileInfo d(dest);
		if (d.size() == src.size() && d.lastModified() == src.lastModified())
			return dest; // already copied
		dest = dir + "/" + src.completeBaseName() + QString(" (%1)").arg(n) + (src.suffix().isEmpty() ? "" : "." + src.suffix());
	}
	if (!QFile::copy(src.absoluteFilePath(), dest))
		return {};
	QFile(dest).setFileTime(src.lastModified(), QFileDevice::FileModificationTime); // so a re-drop is recognized
	return dest;
}

void App::importAssets(const QVariantList &urls)
{
	int added = 0, skipped = 0;
	for (const QVariant &u : urls) {
		const QString file = u.toUrl().isLocalFile() ? u.toUrl().toLocalFile() : u.toString();
		if (QFileInfo(file).isDir()) { // a dropped folder: its files, one level deep
			for (const QFileInfo &fi : QDir(file).entryInfoList(QDir::Files))
				copyIntoAssets(fi.absoluteFilePath()).isEmpty() ? ++skipped : ++added;
			continue;
		}
		copyIntoAssets(file).isEmpty() ? ++skipped : ++added;
	}
	emit assetsChanged();
	showToast(QString("Added %1 file%2 to assets").arg(added).arg(added == 1 ? "" : "s") + (skipped ? QString(" (%1 couldn't be copied)").arg(skipped) : QString()));
}

// Files the show uses from elsewhere are copied into the assets folder, and their sources point
// at the copies, so a project folder carries everything it needs.
void App::adoptExternalFiles()
{
	QHash<QString, QString> moved;
	for (const Source &s : doc().sources) {
		if (s.kind != "image" && s.kind != "media")
			continue;
		const QString f = s.settings.value("file").toString();
		if (f.isEmpty() || !QFile::exists(f) || insideAssets(f))
			continue;
		if (const QString copy = copyIntoAssets(f, assetsFolder()); !copy.isEmpty())
			moved[s.id] = copy;
	}
	if (moved.isEmpty())
		return;
	m_store.amend([&](Doc &d) {
		for (auto it = moved.begin(); it != moved.end(); ++it)
			d.sources[it.key()].settings["file"] = it.value();
	});
	emit assetsChanged();
}

// Opens File Explorer through the shell API (as Explorer's own "Show in folder" does),
// rather than starting explorer.exe with arguments, which breaks on quoting and some setups.
static void shellReveal(const QString &path, bool select)
{
	const std::wstring w = QDir::toNativeSeparators(QDir::cleanPath(path)).toStdWString();
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	// Explorer may reuse a window it already has open on this folder; let it come to the front
	// (Windows otherwise keeps another process from taking focus).
	AllowSetForegroundWindow(ASFW_ANY);
	HRESULT hr = E_FAIL;
	if (select) {
		if (PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(w.c_str())) {
			hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
			ILFree(pidl);
		}
		if (FAILED(hr)) { // fall back to Explorer's own /select switch, through the shell
			const std::wstring args = L"/select,\"" + w + L"\"";
			const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL));
			blog(LOG_INFO, "[lobs] reveal fallback for %s: %lld", path.toUtf8().constData(), (long long)r);
		}
	} else {
		const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"explore", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
		hr = r > 32 ? S_OK : E_FAIL;
	}
	blog(LOG_INFO, "[lobs] reveal %s (select=%d): hr=0x%08lx", path.toUtf8().constData(), int(select), (unsigned long)hr);
}

void App::revealAsset(const QString &path)
{
	blog(LOG_INFO, "[lobs] Show in File Explorer: %s (exists=%d)", path.toUtf8().constData(), int(QFile::exists(path)));
	if (!QFile::exists(path))
		return showToast(QFileInfo(path).fileName() + " is no longer in the assets folder");
	shellReveal(path, true);
}

void App::openAssetsFolder()
{
	QDir().mkpath(assetsFolder());
	shellReveal(assetsFolder(), false);
}

void App::trashAsset(const QString &path)
{
	if (QFile::moveToTrash(path))
		showToast("Moved " + QFileInfo(path).fileName() + " to the Recycle Bin");
	else
		showToast("Couldn't move " + QFileInfo(path).fileName() + " to the Recycle Bin");
	emit assetsChanged();
}

QString App::assetSource(Doc &d, const QString &path) const
{
	const QString clean = QDir::cleanPath(path);
	for (const Source &s : d.sources)
		if ((s.kind == "image" || s.kind == "media") && QDir::cleanPath(s.settings.value("file").toString()) == clean)
			return s.id; // one source per file, shared by all its layers
	const QString k = assetKind(path);
	QVariantMap st{{"file", clean}};
	if (k != "image")
		st["loop"] = true;
	return d.addSource(k == "image" ? "image" : "media", QFileInfo(path).completeBaseName(), st).id;
}

// Drop size: the file's own size (images), scaled down to fit the canvas, centered on the point.
Transform App::assetTransform(const QString &path, QPointF c) const
{
	QSizeF sz = assetKind(path) == "image" ? QSizeF(QImageReader(path).size()) : QSizeF(1280, 720);
	if (sz.isEmpty())
		sz = QSizeF(640, 360);
	const double W = doc().canvasW;
	const double H = doc().canvasH;
	const double k = std::min(1.0, std::min(W / sz.width(), H / sz.height()));
	Transform t;
	t.w = std::round(sz.width() * k), t.h = std::round(sz.height() * k);
	t.x = std::round(c.x() - t.w / 2), t.y = std::round(c.y() - t.h / 2);
	if (t.w >= W - 1 && t.h >= H - 1)
		t.x = 0, t.y = 0;
	return t;
}

void App::assetDragMove(const QString &path, double x, double y)
{
	const Transform t = assetTransform(path, toCanvas({x, y}));
	OverlayBox b;
	b.rect = QRectF(t.x, t.y, t.w, t.h);
	b.drop = true;
	m_dropBoxes = {b};
	updateOverlay();
}

void App::assetDrop(const QString &path, double x, double y)
{
	m_dropBoxes.clear();
	if (!doc().scenes.contains(m_editScene) || assetKind(path) == "other")
		return updateOverlay();
	const QPointF at = x < 0 ? QPointF(doc().canvasW / 2.0, doc().canvasH / 2.0) : toCanvas({x, y});
	const Transform t = assetKind(path) == "audio" ? Transform{0, 0, 1, 1} : assetTransform(path, at);
	QString iid;
	m_store.commit("Place " + QFileInfo(path).fileName(), [&](Doc &d) {
		const QString sid = assetSource(d, path);
		iid = d.placeSource(m_editScene, sid).id;
		d.setTransform(iid, t);
	});
	m_selection = {iid};
	m_poolSelection.clear();
	updateOverlay();
	emit docChanged();
	emit assetsChanged();
}

void App::placeAsset(const QString &path)
{
	assetDrop(path, -1, -1); // -1: canvas center
}

void App::addGraphic(const QString &kind)
{
	if (kind == "text")
		addSource("text", "Text", "Text", true);
	else
		addSource("color", "Color", "#1d1b2e", true);
}

bool App::uiFlag(const QString &key, bool fallback) const { return QSettings().value("ui/" + key, fallback).toBool(); }
void App::setUiFlag(const QString &key, bool on) { QSettings().setValue("ui/" + key, on); }
QString App::uiValue(const QString &key, const QString &fallback) const { return QSettings().value("ui/" + key, fallback).toString(); }
void App::setUiValue(const QString &key, const QString &value) { QSettings().setValue("ui/" + key, value); }

// Assets search: matches names in every folder under assets. Same fields as assets(), plus
// "where" (the folder it's in, relative; "" for the top).
QVariantList App::searchAssets(const QString &query) const
{
	const QString q = query.trimmed();
	QVariantList out;
	if (q.isEmpty())
		return out;
	const auto byFile = scenesByFile();
	QDirIterator it(assetsFolder(), QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext() && out.size() < 500) {
		const QFileInfo fi(it.next());
		if (fi.fileName() != ".lobsfolder" && fi.fileName().contains(q, Qt::CaseInsensitive))
			out << assetEntry(fi, byFile);
	}
	return out;
}

QVariantMap App::sourceInfo(const Source &s) const
{
	const Doc &d = doc();
	QVariantList usedIn;
	QStringList scenes;
	for (const auto &o : d.instances)
		if (o.sourceId == s.id) {
			const QString sn = d.scenes.value(o.sceneId).name;
			usedIn << QVariantMap{{"sceneId", o.sceneId}, {"instId", o.id}, {"scene", sn}};
			if (!scenes.contains(sn))
				scenes << sn;
		}
	return {{"id", s.id}, {"name", s.name}, {"kind", s.kind}, {"kindLabel", kindInfo(s.kind).label},
		{"icon", kindInfo(s.kind).icon}, {"settings", s.settings}, {"usedIn", usedIn}, {"video", kindInfo(s.kind).video},
		{"scenes", scenes}, {"sceneCount", scenes.size()},
		{"hasDevice", QStringList{"camera", "display", "window", "mic", "desktop_audio"}.contains(s.kind) || (s.kind == "game" && s.settings.value("mode").toString() == "window")}};
}

bool App::firstTime(const QString &key)
{
	QSettings st;
	if (st.value("once/" + key).toBool())
		return false;
	st.setValue("once/" + key, true);
	return true;
}

bool App::layoutHas(const QString &file, const QString &name) const
{
	QFile f(file);
	return f.open(QIODevice::ReadOnly) && f.readAll().contains(("\"" + name + "\"").toUtf8());
}

// ---------- replay buffer, virtual camera, hotkeys ----------

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
		// One key does one thing: taking a combo moves it off any other action.
		for (auto it = keys.begin(); it != keys.end();)
			it = (it.value().toString() == combo && it.key() != action) ? keys.erase(it) : std::next(it);
		keys[action] = combo;
	}
	saveSettings({{"hotkeys", keys}});
	rebuildHotkeys();
}

void App::rebuildHotkeys()
{
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
	if (!m_engine->startStreaming())
		showToast(m_engine->lastError());
	else
		m_liveUnlock = false;
	emit engineChanged();
}

void App::setLiveUnlock(bool on)
{
	m_liveUnlock = on;
	emit engineChanged();
}

QVariantList App::mixer() const
{
	QVariantList out;
	for (const QString &id : doc().sourceOrder) {
		const Source &s = doc().sources[id];
		if (!s.hasAudio)
			continue;
		const bool global = !kindInfo(s.kind).video;
		QVariantList tracks;
		for (bool t : s.audio.tracks)
			tracks << t;
		QStringList warn;
		if (s.kind == "desktop_audio" && s.audio.monitor == "monitor_and_output")
			warn << "Echo risk: monitoring desktop audio plays it back into itself.";
		if (s.audio.monitor == "monitor_only")
			warn << "Monitor only: you hear it, the stream and recording don't.";
		if (s.kind == "browser" && s.audio.monitor == "off")
			warn << "Not monitored: viewers hear this page's sounds (alerts), you don't. Click the headphones to hear it.";
		if (!s.audio.tracks.value(0))
			warn << "Track 1 is off, so it isn't in the stream.";
		QVariantList fx;
		for (const Filter &f : s.audio.filters) {
			const FxType *t = audioFx(f.type);
			QVariantList params;
			if (t)
				for (const FxParam &p : t->params)
					params << QVariantMap{{"key", p.key}, {"label", p.label}, {"suffix", p.suffix}, {"min", p.min}, {"max", p.max},
							      {"decimals", p.decimals}, {"value", f.settings.value(p.key, p.def).toDouble()}};
			const QString label = t ? t->label : f.settings.value("_label", f.type.mid(4)).toString();
			fx << QVariantMap{{"type", f.type}, {"label", label}, {"enabled", f.enabled}, {"params", params}, {"known", t != nullptr}};
		}
		out << QVariantMap{{"id", id}, {"name", s.name}, {"icon", kindInfo(s.kind).icon}, {"color", s.color}, {"volume", s.audio.volume},
				   {"muted", s.audio.muted}, {"monitor", s.audio.monitor}, {"tracks", tracks}, {"global", global}, {"warnings", warn}, {"fx", fx},
				   {"mono", s.audio.mono}, {"balance", s.audio.balance}, {"syncMs", s.audio.syncMs}};
	}
	return out;
}

QVariantList App::audioFxTypes() const
{
	QVariantList out;
	for (const FxType &t : lobs::audioFxTypes())
		out << QVariantMap{{"type", t.type}, {"label", t.label}};
	return out;
}

void App::addAudioFilter(const QString &sourceId, const QString &type)
{
	const FxType *t = audioFx(type);
	if (!t || !doc().sources.contains(sourceId))
		return;
	m_store.commit("Add " + t->label, [&](Doc &d) { d.sources[sourceId].audio.filters << Filter{uid("afx_"), type, true, audioFxDefaults(type)}; });
}

void App::removeAudioFilter(const QString &sourceId, int index)
{
	if (!doc().sources.contains(sourceId) || index < 0 || index >= doc().sources[sourceId].audio.filters.size())
		return;
	m_store.commit("Remove audio effect", [&](Doc &d) { d.sources[sourceId].audio.filters.removeAt(index); });
}

void App::moveAudioFilter(const QString &sourceId, int index, int delta)
{
	if (!doc().sources.contains(sourceId))
		return;
	const int n = int(doc().sources[sourceId].audio.filters.size()), to = index + delta;
	if (index < 0 || index >= n || to < 0 || to >= n)
		return;
	m_store.commit("Reorder audio effects", [&](Doc &d) { d.sources[sourceId].audio.filters.move(index, to); });
}

void App::setAudioFilterEnabled(const QString &sourceId, int index, bool on)
{
	if (!doc().sources.contains(sourceId) || index < 0 || index >= doc().sources[sourceId].audio.filters.size())
		return;
	m_store.commit(on ? "Enable audio effect" : "Bypass audio effect", [&](Doc &d) { d.sources[sourceId].audio.filters[index].enabled = on; });
}

void App::setAudioFilterValue(const QString &sourceId, int index, const QString &key, double value, bool final)
{
	if (!doc().sources.contains(sourceId) || index < 0 || index >= doc().sources[sourceId].audio.filters.size())
		return;
	liveEdit(QString("afx:%1:%2:%3").arg(sourceId).arg(index).arg(key), "Edit audio effect",
		 [&](Doc &d) { d.sources[sourceId].audio.filters[index].settings[key] = value; }, final);
}

void App::setAudio(const QString &sourceId, const QString &field, const QVariant &value, bool final)
{
	if (!doc().sources.contains(sourceId))
		return;
	auto fn = [&](Doc &d) {
		AudioSettings &a = d.sources[sourceId].audio;
		if (field == "volume") a.volume = value.toDouble();
		else if (field == "muted") a.muted = value.toBool();
		else if (field == "monitor") a.monitor = value.toString();
		else if (field == "mono") a.mono = value.toBool();
		else if (field == "balance") a.balance = std::clamp(value.toDouble(), 0.0, 1.0);
		else if (field == "sync") a.syncMs = std::clamp(value.toInt(), -950, 20000);
		else if (field == "track") {
			const int t = value.toInt();
			if (t >= 0 && t < a.tracks.size()) a.tracks[t] = !a.tracks[t];
		}
	};
	if (field == "volume" || field == "balance") {
		liveEdit(field + ":" + sourceId, (field == "volume" ? "Volume " : "Balance ") + doc().sources[sourceId].name, fn, final);
	} else {
		m_store.commit(field == "muted" ? (value.toBool() ? "Mute " : "Unmute ") + doc().sources[sourceId].name : "Audio " + field, fn);
	}
}

// ---------- canvas ----------

double App::canvasScale() const
{
	// Same rule as the engine's drawing, on physical pixels, back in logical units.
	return previewScaleFor(m_previewSize.width() * m_previewDpr, m_previewSize.height() * m_previewDpr, doc().canvasW, doc().canvasH, m_pixelPerfect) / m_previewDpr;
}

void App::setPixelPerfect(bool on)
{
	m_pixelPerfect = on;
	m_engine->setPixelPerfect(on);
	setUiFlag("preview/pixelPerfect", on);
	updateOverlay();
	emit previewScaleChanged();
}

QPointF App::toCanvas(QPointF p) const
{
	const double s = canvasScale();
	const double ox = (m_previewSize.width() - doc().canvasW * s) / 2, oy = (m_previewSize.height() - doc().canvasH * s) / 2;
	return {(p.x() - ox) / s, (p.y() - oy) / s};
}

QPointF App::fromCanvas(QPointF c) const
{
	const double s = canvasScale();
	const double ox = (m_previewSize.width() - doc().canvasW * s) / 2, oy = (m_previewSize.height() - doc().canvasH * s) / 2;
	return {c.x() * s + ox, c.y() * s + oy};
}

QString App::topAncestor(const QString &id) const
{
	QString cur = id;
	while (const Instance *p = doc().parentOf(cur))
		cur = p->id;
	return cur;
}

QString App::hitTest(QPointF c) const
{
	const auto leaves = doc().flatten(m_editScene);
	for (int k = leaves.size() - 1; k >= 0; k--) { // topmost first
		const Instance &i = doc().instances[leaves[k].id];
		if (!leaves[k].visible || !kindInfo(doc().sources.value(i.sourceId).kind).video)
			continue;
		const Transform t = doc().transform(i);
		if (QRectF(t.x, t.y, t.w, t.h).contains(c))
			return i.id;
	}
	return {};
}

int App::hitHandle(QPointF c) const
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return -1;
	const Transform t = doc().transform(doc().instances[id]);
	const double r = 9 / canvasScale();
	if (m_cropMode) {
		const double cr = 12 / canvasScale();
		const bool nl = std::abs(c.x() - t.x) <= cr, nr = std::abs(c.x() - (t.x + t.w)) <= cr;
		const bool nt = std::abs(c.y() - t.y) <= cr, nb = std::abs(c.y() - (t.y + t.h)) <= cr;
		const bool inX = c.x() > t.x - cr && c.x() < t.x + t.w + cr, inY = c.y() > t.y - cr && c.y() < t.y + t.h + cr;
		if (nl && nt) return 14;
		if (nr && nt) return 15;
		if (nl && nb) return 16;
		if (nr && nb) return 17;
		if (nl && inY) return 10;
		if (nt && inX) return 11;
		if (nr && inY) return 12;
		if (nb && inX) return 13;
		return -1;
	}
	const QPointF corners[] = {{t.x, t.y}, {t.x + t.w, t.y}, {t.x, t.y + t.h}, {t.x + t.w, t.y + t.h}};
	for (int k = 0; k < 4; k++)
		if (std::abs(c.x() - corners[k].x()) <= r && std::abs(c.y() - corners[k].y()) <= r)
			return k;
	return -1;
}

Qt::CursorShape App::cursorAt(QPointF p) const
{
	if (m_tool == "text" || textEditing())
		return Qt::IBeamCursor;
	if (m_tool != "select")
		return Qt::CrossCursor;
	const int h = hitHandle(toCanvas(p));
	if (h == 10 || h == 12) return Qt::SizeHorCursor;
	if (h == 11 || h == 13) return Qt::SizeVerCursor;
	if (h == 14 || h == 17) return Qt::SizeFDiagCursor;
	if (h == 15 || h == 16) return Qt::SizeBDiagCursor;
	if (h == 0 || h == 3) return Qt::SizeFDiagCursor;
	if (h == 1 || h == 2) return Qt::SizeBDiagCursor;
	return hitTest(toCanvas(p)).isEmpty() ? Qt::ArrowCursor : Qt::SizeAllCursor;
}

void App::canvasPress(QPointF p, Qt::KeyboardModifiers mods, bool dbl)
{
	endTextEdit(); // clicking anywhere finishes typing
	const QPointF c = toCanvas(p);
	if (m_tool == "laser") { // works while live, never touches the show
		m_drag = Drag::Laser;
		m_engine->setLaserLife(m_laserTrailMs);
		if (QColor::isValidColorName(m_toolFill))
			m_engine->setLaserColor(QColor(m_toolFill));
		m_engine->laserPoint(c, true);
		return;
	}
	if (m_tool != "select") {
		if (!doc().scenes.contains(m_editScene))
			return;
		if (liveLocked() && !(mods & Qt::AltModifier) && m_editScene == m_engine->programScene())
			return showToast("Live lock is on. Hold Alt to add graphics.");
		m_drag = Drag::Create;
		m_createPts = {c};
		return;
	}
	m_dragStart = c;
	m_dragBase = doc();
	m_handle = hitHandle(c);
	const bool locked = liveLocked() && !(mods & Qt::AltModifier);
	if (m_handle >= 0 && locked) {
		m_drag = Drag::None;
		return showToast("Live lock is on. Hold Alt to resize.");
	}
	if (m_handle >= 10) {
		if (doc().instances[singleSelection()].locked)
			return showToast("Layer is locked");
		m_drag = Drag::Crop;
		m_dragIds = {singleSelection()};
		return;
	}
	if (m_cropMode && hitTest(c) != singleSelection())
		setCropMode(false);
	if (m_handle >= 0) {
		if (doc().instances[singleSelection()].locked)
			return showToast("Layer is locked");
		m_drag = Drag::Resize;
		m_dragIds = {singleSelection()};
		return;
	}
	const QString leaf = hitTest(c);
	if (leaf.isEmpty()) {
		m_selection.clear();
		m_drag = Drag::None;
		updateOverlay();
		emit docChanged();
		return;
	}
	// Click selects the top-level group (or keeps the selected level); each double-click goes
	// one level deeper toward the layer under the pointer.
	QStringList path{leaf};
	for (const Instance *p2 = doc().parentOf(leaf); p2; p2 = doc().parentOf(p2->id))
		path.prepend(p2->id);
	int level = -1; // deepest selected level along the path
	for (int k = 0; k < path.size(); ++k)
		if (m_selection.contains(path[k]))
			level = k;
	const QString target = dbl ? path[std::min(int(path.size()) - 1, level + 1)] : path[std::max(0, level)];
	if (dbl)
		m_selection = {target};
	if (mods & Qt::ShiftModifier) {
		selectLayer(target, true);
		m_drag = Drag::None;
		return;
	}
	if (!m_selection.contains(target))
		m_selection = {target};
	m_poolSelection.clear();
	m_dragIds.clear();
	for (const QString &id : m_selection) {
		bool locked = false;
		for (const QString &d : doc().descendants(id))
			locked |= doc().instances[d].locked;
		if (!locked)
			m_dragIds << id;
	}
	m_drag = m_dragIds.isEmpty() ? Drag::None : Drag::Move;
	if (m_dragIds.isEmpty())
		showToast("Layer is locked");
	else if (locked) {
		m_drag = Drag::None; // selecting is fine while live; moving needs Alt
		showToast("Live lock is on. Hold Alt to move layers.");
	}
	updateOverlay();
	emit docChanged();
}

void App::canvasMove(QPointF p, Qt::KeyboardModifiers mods)
{
	QElapsedTimer perf;
	perf.start();
	struct PerfScope {
		App *a; QElapsedTimer &t;
		~PerfScope() { const qint64 us = t.nsecsElapsed() / 1000; a->m_perfMoves++; a->m_perfTotalUs += us; a->m_perfMaxUs = std::max(a->m_perfMaxUs, us); }
	} perfScope{this, perf};
	if (m_drag == Drag::None)
		return;
	const QPointF c = toCanvas(p);
	if (m_drag == Drag::Laser)
		return m_engine->laserPoint(c, false);
	if (m_drag == Drag::Create) {
		if (m_tool == "pencil") {
			if (QLineF(m_createPts.last(), c).length() >= 1.5)
				m_createPts << c;
			m_engine->setInk(m_createPts, m_toolStroke, std::max(2, m_toolStrokeWidth));
		} else if (m_tool == "text") {
			// Drag out the text size: the height of the drag is the line height.
			m_createPts = {m_createPts.first(), c};
			const QPointF a = m_createPts.first();
			const double size = std::max(12.0, std::abs(c.y() - a.y()));
			drawTextGhost(QPointF(a.x(), c.y() < a.y() ? a.y() - size : a.y()), size);
		} else {
			QPointF e = c;
			if (mods & Qt::ShiftModifier) { // square / circle / 45-degree arrow
				const QPointF d = c - m_createPts.first();
				if (m_tool == "arrow") {
					const double ang = std::round(std::atan2(d.y(), d.x()) / (M_PI / 4)) * (M_PI / 4), len = std::hypot(d.x(), d.y());
					e = m_createPts.first() + QPointF(std::cos(ang), std::sin(ang)) * len;
				} else {
					const double m = std::max(std::abs(d.x()), std::abs(d.y()));
					e = m_createPts.first() + QPointF(d.x() < 0 ? -m : m, d.y() < 0 ? -m : m);
				}
			}
			m_createPts = {m_createPts.first(), e};
			// Preview the real shape (fill, stroke, arrowhead) as you drag, drawn like the layer.
			const double sw = m_toolStrokeWidth;
			if (m_tool == "arrow") {
				const QPointF a = m_createPts[0], b = m_createPts[1];
				const double pad = std::max(20.0, std::max(1.0, sw) * 3.8) / 2 + 2;
				const QRectF r = QRectF(a, b).normalized().adjusted(-pad, -pad, pad, pad);
				const QVariantMap st{{"shape", "arrow"}, {"stroke", m_toolStroke}, {"strokeWidth", std::max(1.0, sw)},
						     {"points", QVariantList{QVariantList{(a.x() - r.x()) / r.width(), (a.y() - r.y()) / r.height()},
									      QVariantList{(b.x() - r.x()) / r.width(), (b.y() - r.y()) / r.height()}}}};
				m_engine->setGhost(renderShape(st, int(std::ceil(r.width())), int(std::ceil(r.height()))), r.topLeft());
			} else {
				const QRectF r = QRectF(m_createPts[0], m_createPts[1]).normalized();
				if (r.width() >= 2 && r.height() >= 2) {
					const QVariantMap st{{"shape", m_tool}, {"fill", m_toolFill}, {"stroke", sw > 0 ? m_toolStroke : QString()}, {"strokeWidth", sw}, {"radius", 0}};
					m_engine->setGhost(renderShape(st, int(std::round(r.width())), int(std::round(r.height()))), QPointF(std::round(r.x()), std::round(r.y())));
				}
			}
		}
		return;
	}
	if (m_drag == Drag::Crop) {
		// Edges move by the drag, in source pixels at the layer's scale.
		const QString id = m_dragIds.value(0);
		const Instance &bi = m_dragBase.instances[id];
		const QSize ns = m_engine->sourceSize(bi.sourceId);
		if (ns.isEmpty())
			return;
		const Transform bt = m_dragBase.transform(bi);
		const Crop bc = m_dragBase.crop(bi);
		const double sx = bt.w / std::max(1, ns.width() - bc.l - bc.r), sy = bt.h / std::max(1, ns.height() - bc.t - bc.b);
		const int dl = int(std::lround((c.x() - m_dragStart.x()) / sx)), dt = int(std::lround((c.y() - m_dragStart.y()) / sy));
		Crop nc = bc;
		const int h = m_handle;
		if (h == 10 || h == 14 || h == 16) nc.l = bc.l + dl;
		if (h == 12 || h == 15 || h == 17) nc.r = bc.r - dl;
		if (h == 11 || h == 14 || h == 15) nc.t = bc.t + dt;
		if (h == 13 || h == 16 || h == 17) nc.b = bc.b - dt;
		m_store.gesture([&](Doc &d) { cropKeepingScale(d, id, nc, ns); });
		updateOverlay();
		return;
	}
	const Doc &base = m_dragBase;
	m_guides.clear();
	if (m_drag == Drag::Move) {
		double dx = c.x() - m_dragStart.x(), dy = c.y() - m_dragStart.y();
		if (!(mods & Qt::ControlModifier)) {
			// Snap the union box's edges/center to the canvas and to other layers.
			QSet<QString> moving;
			double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
			for (const QString &id : m_dragIds) {
				for (const QString &d : base.descendants(id))
					moving.insert(d);
				const Transform b = base.bounds(id);
				x0 = std::min(x0, b.x), y0 = std::min(y0, b.y), x1 = std::max(x1, b.x + b.w), y1 = std::max(y1, b.y + b.h);
			}
			QVector<double> xs{0, base.canvasW / 2.0, double(base.canvasW)}, ys{0, base.canvasH / 2.0, double(base.canvasH)};
			for (const auto &leaf : base.flatten(m_editScene)) {
				if (moving.contains(leaf.id))
					continue;
				const Transform t = base.transform(base.instances[leaf.id]);
				xs << t.x << t.x + t.w / 2 << t.x + t.w;
				ys << t.y << t.y + t.h / 2 << t.y + t.h;
			}
			const double th = 8 / canvasScale();
			auto snap = [&](QVector<double> edges, const QVector<double> &lines, bool vertical) {
				double best = 1e9, at = 0;
				for (double e : edges)
					for (double l : lines)
						if (std::abs(l - e) < th && std::abs(l - e) < std::abs(best))
							best = l - e, at = l;
				if (best < 1e8) {
					m_guides << OverlayGuide{vertical, at};
					return best;
				}
				return 0.0;
			};
			dx += snap({x0 + dx, (x0 + x1) / 2 + dx, x1 + dx}, xs, true);
			dy += snap({y0 + dy, (y0 + y1) / 2 + dy, y1 + dy}, ys, false);
		}
		const QStringList ids = m_dragIds;
		m_store.gesture([&](Doc &d) {
			QSet<QString> seen;
			for (const QString &id : ids)
				d.moveBy(id, dx, dy, seen);
		});
	} else {
		const QString id = m_dragIds.first();
		const Transform t0 = base.transform(base.instances[id]);
		const bool left = m_handle == 0 || m_handle == 2, top = m_handle == 0 || m_handle == 1;
		const double ax = left ? t0.x + t0.w : t0.x, ay = top ? t0.y + t0.h : t0.y; // opposite corner
		double w = std::max(8.0, std::abs(c.x() - ax)), h = std::max(8.0, std::abs(c.y() - ay));
		if (!(mods & Qt::ShiftModifier)) {
			const double aspect = t0.w / std::max(1.0, t0.h);
			if (w / h > aspect) h = w / aspect;
			else w = h * aspect;
		}
		Transform t = t0;
		t.x = std::round(left ? ax - w : ax), t.y = std::round(top ? ay - h : ay), t.w = std::round(w), t.h = std::round(h);
		m_store.gesture([&](Doc &d) { d.setTransform(id, t); });
	}
}

void App::canvasRelease()
{
	if (m_perfMoves > 5)
		blog(LOG_INFO, "[perf] drag: %d moves, avg %.2f ms, max %.2f ms per move; engine frame %.2f ms; lagged %u",
		     m_perfMoves, m_perfTotalUs / 1000.0 / m_perfMoves, m_perfMaxUs / 1000.0, obs_get_average_frame_time_ns() / 1e6, obs_get_lagged_frames());
	if (m_perfMoves > 5)
		blog(LOG_INFO, "[perf]   per move: engine apply %.2f ms, edit scene %.2f ms, hotkeys %.2f ms, UI update %.2f ms",
		     m_perfApplyUs / 1000.0 / m_perfMoves, m_perfEditUs / 1000.0 / m_perfMoves, m_perfHotkeyUs / 1000.0 / m_perfMoves, m_perfQmlUs / 1000.0 / m_perfMoves);
	m_perfApplyUs = m_perfEditUs = m_perfHotkeyUs = m_perfQmlUs = 0;
	m_perfMoves = 0, m_perfTotalUs = 0, m_perfMaxUs = 0;
	if (m_drag == Drag::Laser) {
		m_drag = Drag::None;
		return;
	}
	if (m_drag == Drag::Create) {
		m_drag = Drag::None;
		finishCreate();
		return;
	}
	if (m_drag == Drag::Move)
		m_store.endGesture(m_dragIds.size() > 1 ? QString("Move %1 layers").arg(m_dragIds.size()) : "Move " + doc().instances.value(m_dragIds.value(0)).name);
	else if (m_drag == Drag::Resize)
		m_store.endGesture("Resize " + doc().instances.value(m_dragIds.value(0)).name);
	else if (m_drag == Drag::Crop)
		m_store.endGesture("Crop " + doc().instances.value(m_dragIds.value(0)).name);
	m_drag = Drag::None;
	m_guides.clear();
	updateOverlay();
}

// ---------- graphic tools ----------

void App::setTool(const QString &t)
{
	static const QStringList tools = {"select", "text", "rect", "ellipse", "arrow", "pencil", "laser"};
	if (!tools.contains(t) || t == m_tool)
		return;
	m_tool = t;
	if (t != "select")
		m_selection.clear(), updateOverlay(), emit docChanged();
	emit toolChanged();
}
void App::setToolFill(const QString &c)
{
	m_toolFill = c;
	setUiValue("tools/fill", c);
	if (m_engine && QColor::isValidColorName(c))
		m_engine->setLaserColor(QColor(c)); // the laser draws in the fill color
	emit toolChanged();
}
void App::setToolStroke(const QString &c) { m_toolStroke = c; setUiValue("tools/stroke", c); emit toolChanged(); }
void App::setLaserTrailMs(int ms)
{
	m_laserTrailMs = std::clamp(ms, 150, 3000);
	setUiValue("tools/laserTrailMs", QString::number(m_laserTrailMs));
	if (m_engine)
		m_engine->setLaserLife(m_laserTrailMs);
	emit toolChanged();
}
void App::setToolStrokeWidth(int w) { m_toolStrokeWidth = std::clamp(w, 0, 64); setUiValue("tools/strokeWidth", QString::number(m_toolStrokeWidth)); emit toolChanged(); }

// Mouse up with a drawing tool: make the layer. A click without a drag gets a default size.
void App::finishCreate()
{
	m_engine->setInk({}, "#ffffff", 1);
	m_engine->setGhost({}, {});
	m_dropBoxes.clear();
	const QVector<QPointF> pts = m_createPts;
	m_createPts.clear();
	if (pts.isEmpty() || !doc().scenes.contains(m_editScene))
		return updateOverlay();
	const double sw = m_toolStrokeWidth;
	const QPointF a = pts.first(), b = pts.last();
	const bool clicked = QLineF(a, b).length() < 6 && pts.size() < 3;
	QString kind = "shape", label;
	QVariantMap st;
	Transform t;
	auto boxAround = [&](QPointF c, double w, double h) { t.x = std::round(c.x() - w / 2), t.y = std::round(c.y() - h / 2), t.w = w, t.h = h; };
	if (m_tool == "text") {
		kind = "text";
		label = "Text";
		st = {{"text", ""}, {"textColor", m_toolStroke}, {"font", m_toolFont}, {"bold", m_toolBold}, {"italic", m_toolItalic}};
		// An empty box at the size dragged out (a click: 96 px), typed into right away.
		const double size = clicked ? 96 : std::max(12.0, std::round(std::abs(b.y() - a.y())));
		const double top = clicked ? a.y() - size / 2 : (b.y() < a.y() ? a.y() - size : a.y());
		t = {std::round(a.x()), std::round(top), 4, size, 0};
		m_engine->setGhost({}, {});
	} else if (m_tool == "rect" || m_tool == "ellipse") {
		label = m_tool == "rect" ? "Rectangle" : "Ellipse";
		st = {{"shape", m_tool}, {"fill", m_toolFill}, {"stroke", sw > 0 ? m_toolStroke : QString()}, {"strokeWidth", sw}, {"radius", 0}};
		if (clicked) boxAround(a, m_tool == "rect" ? 400 : 300, m_tool == "rect" ? 240 : 300);
		else { const QRectF r = QRectF(a, b).normalized(); t = {std::round(r.x()), std::round(r.y()), std::max(4.0, std::round(r.width())), std::max(4.0, std::round(r.height())), 0}; }
	} else if (m_tool == "arrow" || m_tool == "pencil") {
		label = m_tool == "arrow" ? "Arrow" : "Drawing";
		QVector<QPointF> line = m_tool == "arrow" ? QVector<QPointF>{a, clicked ? a + QPointF(300, 0) : b} : pts;
		// The layer box: the stroke's bounds, padded for its width (and the arrowhead).
		const double pad = (m_tool == "arrow" ? std::max(20.0, std::max(1.0, sw) * 3.8) : std::max(1.0, sw)) / 2 + 2;
		// (min/max by hand: QRectF ignores zero-size rects when uniting them)
		double x0 = line.first().x(), y0 = line.first().y(), x1 = x0, y1 = y0;
		for (const QPointF &q : line)
			x0 = std::min(x0, q.x()), y0 = std::min(y0, q.y()), x1 = std::max(x1, q.x()), y1 = std::max(y1, q.y());
		const QRectF r(QPointF(x0 - pad, y0 - pad), QPointF(x1 + pad, y1 + pad));
		t = {std::floor(r.x()), std::floor(r.y()), std::ceil(r.width()), std::ceil(r.height()), 0};
		QVariantList norm;
		for (const QPointF &q : line)
			norm << QVariant(QVariantList{(q.x() - t.x) / t.w, (q.y() - t.y) / t.h});
		st = {{"shape", m_tool == "arrow" ? "arrow" : "path"}, {"stroke", m_toolStroke}, {"strokeWidth", std::max(1.0, sw)}, {"points", norm}};
	} else {
		return updateOverlay();
	}
	QString iid;
	m_store.commit("Add " + label.toLower(), [&](Doc &d) {
		const QString sid = d.addSource(kind, label, st).id;
		iid = d.placeSource(m_editScene, sid).id;
		d.setTransform(iid, t);
	});
	m_selection = {iid};
	m_poolSelection.clear();
	// Like design tools: a shape or text is placed once, then back to selecting (pencil keeps drawing).
	if (m_tool == "text") {
		m_textEditId = iid;
		m_caretOn = true;
		m_caretTimer.start();
		layoutEditedText();
	}
	if (m_tool != "pencil")
		m_tool = "select", emit toolChanged();
	updateOverlay();
	emit docChanged();
}

// ---------- text tool: drag out a size, then type ----------

// Fonts for text layers. A layer's box is (lines x linePx) tall; the engine renders the text with
// an em of linePx and scales it to fit, so the visible em is linePx * linePx / lineSpacing.
static QFont textFont(const QString &face, bool bold, bool italic, double px)
{
	QFont f(face);
	f.setPixelSize(std::max(1, int(std::lround(px))));
	f.setBold(bold);
	f.setItalic(italic);
	return f;
}

void App::drawTextGhost(QPointF at, double size)
{
	const QFontMetricsF base(textFont(m_toolFont, m_toolBold, m_toolItalic, size));
	const double em = size * size / std::max(1.0, base.lineSpacing());
	const QFont f = textFont(m_toolFont, m_toolBold, m_toolItalic, em);
	const QFontMetricsF fm(f);
	const int w = int(std::ceil(fm.horizontalAdvance("A"))) + 4, h = int(std::ceil(size));
	QImage img(std::max(1, w), std::max(1, h), QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	QPainter p(&img);
	p.setRenderHint(QPainter::TextAntialiasing);
	QColor c = QColor::isValidColorName(m_toolStroke) ? QColor(m_toolStroke) : QColor(Qt::white);
	c.setAlphaF(0.8f);
	p.setPen(c);
	p.setFont(f);
	p.drawText(QPointF(1, (h - fm.height()) / 2 + fm.ascent()), "A");
	p.end();
	m_engine->setGhost(img, at);
}

void App::layoutEditedText()
{
	if (!doc().instances.contains(m_textEditId))
		return;
	const Instance &inst = doc().instances[m_textEditId];
	const Source &src = doc().sources[inst.sourceId];
	const QString text = src.settings.value("text").toString();
	const QStringList lines = text.split('\n');
	Transform t = doc().transform(inst);
	const double linePx = std::max(4.0, t.h / std::max<qsizetype>(1, lines.size()) );
	const QFont f = textFont(src.settings.value("font", "Segoe UI").toString(), src.settings.value("bold", true).toBool(), src.settings.value("italic").toBool(), linePx);
	const QFontMetricsF fm(f);
	const double k = linePx / std::max(1.0, fm.lineSpacing());
	double wmax = 0;
	for (const QString &l : lines)
		wmax = std::max(wmax, fm.horizontalAdvance(l));
	Transform nt = t;
	nt.w = std::max(4.0, std::ceil(wmax * k));
	nt.h = std::round(linePx * lines.size());
	if (nt.w != t.w || nt.h != t.h) {
		const QString id = m_textEditId;
		m_store.amend([&](Doc &d) { d.setTransform(id, nt); });
		updateOverlay();
	}
	// Caret at the end of the last line.
	if (m_caretOn) {
		const double lastW = fm.horizontalAdvance(lines.last()) * k;
		const int cw = std::max(2, int(std::lround(linePx / 28))), ch = std::max(4, int(std::lround(linePx * 0.86)));
		QImage caret(cw, ch, QImage::Format_ARGB32);
		const QString col = src.settings.value("textColor", "#ffffff").toString();
		caret.fill(QColor::isValidColorName(col) ? QColor(col) : QColor(Qt::white));
		m_engine->setGhost(caret, QPointF(nt.x + std::min(lastW, nt.w) + 2, nt.y + linePx * (lines.size() - 1) + (linePx - ch) / 2));
	} else {
		m_engine->setGhost({}, {});
	}
}

bool App::textKey(QKeyEvent *e)
{
	if (!textEditing())
		return false;
	if (!doc().instances.contains(m_textEditId)) {
		endTextEdit();
		return false;
	}
	const bool ctrl = e->modifiers() & Qt::ControlModifier;
	if (e->key() == Qt::Key_Escape || (ctrl && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter))) {
		endTextEdit();
		return true;
	}
	if (ctrl || (e->modifiers() & Qt::AltModifier)) { // shortcuts: stop typing, let them run
		endTextEdit();
		return false;
	}
	const QString sid = doc().instances[m_textEditId].sourceId;
	QString text = doc().sources[sid].settings.value("text").toString();
	const QString before = text;
	if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
		text += '\n';
	else if (e->key() == Qt::Key_Backspace)
		text.chop(1);
	else if (!e->text().isEmpty() && e->text()[0].isPrint())
		text += e->text();
	else
		return true; // arrows, Delete...: swallowed while typing
	if (text != before) {
		// Keep the per-line height while lines come and go.
		const Transform t = doc().transform(doc().instances[m_textEditId]);
		const double linePx = t.h / std::max<qsizetype>(1, before.count('\n') + 1);
		Transform nt = t;
		nt.h = std::round(linePx * (text.count('\n') + 1));
		const QString id = m_textEditId;
		m_store.amend([&](Doc &d) {
			d.sources[sid].settings["text"] = text;
			d.setTransform(id, nt);
		});
	}
	m_caretOn = true;
	m_caretTimer.start();
	layoutEditedText();
	emit docChanged();
	return true;
}

void App::endTextEdit()
{
	if (!textEditing())
		return;
	m_caretTimer.stop();
	m_caretOn = false;
	m_engine->setGhost({}, {});
	const QString id = m_textEditId;
	m_textEditId.clear();
	if (!doc().instances.contains(id))
		return;
	const QString sid = doc().instances[id].sourceId;
	if (doc().sources.value(sid).settings.value("text").toString().trimmed().isEmpty()) {
		// Nothing typed: no layer.
		m_store.amend([&](Doc &d) {
			d.deleteInstance(id);
			d.sources.remove(sid);
			d.sourceOrder.removeAll(sid);
		});
		m_selection.removeAll(id);
		updateOverlay();
		emit docChanged();
	}
}

void App::updateOverlay()
{
	if (m_cropMode && (m_selection.size() != 1 || !doc().instances.contains(m_selection.first())))
		m_cropMode = false;
	QVector<OverlayBox> boxes;
	for (const QString &id : m_selection) {
		if (!doc().instances.contains(id) || doc().instances[id].sceneId != m_editScene)
			continue;
		const Instance &i = doc().instances[id];
		const Transform b = doc().bounds(id);
		OverlayBox ob{QRectF(b.x, b.y, b.w, b.h), b.rot, m_selection.size() == 1 && i.kind != "group", false, i.kind == "group", i.links.contains(Group::Transform)};
		ob.crop = m_cropMode && m_selection.size() == 1;
		boxes << ob;
	}
	boxes << m_dropBoxes;
	m_engine->setOverlay(boxes, m_guides);
}
