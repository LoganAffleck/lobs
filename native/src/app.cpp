#include "app_internal.h"
#include "model/audio_fx.h"
#include "model/video_fx.h"
#include "permissions.h"
#include "plugins.h"

#include <obs.h>

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <functional>

using namespace lobs;

QString pluginTypeLabel(const QString &type)
{
	const char *name = type.startsWith("obs:") ? obs_source_get_display_name(type.mid(4).toUtf8().constData()) : nullptr;
	return name ? QString::fromUtf8(name) : QString();
}

QString filterLabel(const QString &type)
{
	if (type.startsWith("obs:")) {
		const QString l = pluginTypeLabel(type);
		return l.isEmpty() ? type.mid(4) + " (plugin not loaded)" : l;
	}
	const VfxType *t = videoFx(type);
	return t ? t->label : type;
}

void registerPluginKinds()
{
	for (const QString &id : lobs::plugins::inputTypes()) {
		const QByteArray i = id.toUtf8();
		const uint32_t flags = obs_get_source_output_flags(i.constData());
		if (flags & (OBS_SOURCE_CAP_DISABLED | OBS_SOURCE_DEPRECATED))
			continue;
		const char *name = obs_source_get_display_name(i.constData());
		lobs::pluginKinds()["obs:" + id] = {name ? QString::fromUtf8(name) : id, "puzzle", bool(flags & OBS_SOURCE_VIDEO), bool(flags & OBS_SOURCE_AUDIO), 0, 0};
	}
}

static Doc loadDoc(const QString &path)
{
	QFile f(path);
	if (f.open(QIODevice::ReadOnly)) {
		const QJsonDocument j = QJsonDocument::fromJson(f.readAll());
		if (j.isObject() && j["version"].toInt() == 1) {
			Doc d = Doc::fromJson(j.object());
			d.repair();
			return d;
		}
	}
	if (QFileInfo(path).fileName() == "lobs-selftest.lobs.json")
		return Doc::sample();
	Doc d;
	const QString scene = d.addScene("Scene").id;
	const QString bg = d.addSource("color", "Background", {{"color", "#000000"}, {"background", true}}).id;
	d.placeSource(scene, bg);
	d.addSource("mic", "Mic");
	d.addSource("desktop_audio", "Desktop Audio");
	return d;
}

QVariantList App::devicesForKind(const QString &kind) { return m_engine->listDevices({}, kind); }

QString App::sourceUsingDevice(const QString &kind, const QString &device) const
{
	if (device.isEmpty())
		return {};
	for (const Source &s : doc().sources)
		if (s.kind == kind && s.settings.value("device").toString() == device)
			return s.id;
	return {};
}

QString firstRealDevice(Engine *engine, const QString &kind)
{
	for (const QVariant &v : engine->listDevices({}, kind)) {
		const QVariantMap m = v.toMap();
		if (!m["name"].toString().contains("Virtual", Qt::CaseInsensitive) && !m["value"].toString().isEmpty() && !m["inUse"].toBool())
			return m["value"].toString();
	}
	return {};
}

static bool isOtherPlatformDevice(const QString &kind, const QString &dev)
{
#ifdef Q_OS_WIN
	static const QRegularExpression uuid("^[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}$", QRegularExpression::CaseInsensitiveOption);
	return uuid.match(dev).hasMatch() || (kind == "window" && !dev.isEmpty() && dev.toULongLong() > 0);
#else
	return dev.startsWith("\\\\?\\") || dev.contains('#') || dev.startsWith('{') || (kind == "window" && dev.contains(':'));
#endif
}

Doc withDefaultDevices(Doc d, Engine *engine)
{
	for (Source &s : d.sources) {
		if (!QStringList{"camera", "display", "window", "mic", "desktop_audio"}.contains(s.kind))
			continue;
		if (isOtherPlatformDevice(s.kind, s.settings.value("device").toString()))
			s.settings.remove("device");
		if (s.kind != "window" && s.settings.value("device").toString().isEmpty())
			if (const QString dev = firstRealDevice(engine, s.kind); !dev.isEmpty())
				s.settings["device"] = dev;
	}
	return d;
}

void App::fillMissingDevices()
{
	const Doc filled = withDefaultDevices(doc(), m_engine);
	if (filled.sources != doc().sources)
		m_store.amend([&](Doc &d) { d.sources = filled.sources; });
}

App::App(Engine *engine, const QString &docPath, QObject *parent)
	: QObject(parent), m_engine(engine), m_store(withDefaultDevices(loadDoc(docPath), engine)), m_docPath(docPath)
{
	registerPluginKinds();
	connect(&m_store, &Store::changed, this, &App::onStoreChanged);
	for (const QString &id : QSettings().value("ui/layers/collapsed").toStringList())
		m_collapsedGroups.insert(id);
	connect(this, &App::docChanged, this, &App::refreshModels);
	m_inspectorTimer.setSingleShot(true);
	m_inspectorTimer.setInterval(16);
	connect(&m_inspectorTimer, &QTimer::timeout, this, [this] {
		QElapsedTimer pt;
		pt.start();
		pushInspector(true);
		m_perfInspectorUs += pt.nsecsElapsed() / 1000;
	});
	connect(engine, &Engine::statsChanged, this, &App::engineChanged);
	connect(engine, &Engine::statusChanged, this, &App::engineChanged);
	connect(engine, &Engine::recordingChanged, this, &App::engineChanged);
	m_toastTimer.setSingleShot(true);
	connect(&m_toastTimer, &QTimer::timeout, this, [this]() { m_toast.clear(); emit toastChanged(); });
	connect(engine, &Engine::streamingChanged, this, &App::engineChanged);
	connect(engine, &Engine::streamStatsChanged, this, &App::streamStatsChanged);
	connect(engine, &Engine::streamError, this, [this](const QString &m) { showToast(m); });
	connect(engine, &Engine::streamReconnected, this, [this](int) { showToast("Reconnected"); });
	connect(engine, &Engine::remuxFinished, this, [this](bool ok, const QString &mp4) { showToast(ok ? "MP4 ready: " + QFileInfo(mp4).fileName() : "Couldn't make the MP4 copy"); });
	connect(&m_levelTimer, &QTimer::timeout, this, [this]() {
		const QVariantMap next = m_engine->audioLevels();
		bool changed = next.size() != m_levels.size();
		if (!changed) {
			for (auto it = next.begin(); it != next.end(); ++it) {
				const QVariantList a = it.value().toList(), b = m_levels.value(it.key()).toList();
				if (a.size() != b.size() || (a.size() >= 2 && (std::abs(a[0].toDouble() - b[0].toDouble()) >= 0.1 || std::abs(a[1].toDouble() - b[1].toDouble()) >= 0.1))) {
					changed = true;
					break;
				}
			}
		}
		if (changed) {
			m_levels = next;
			emit levelsChanged();
		}
	});
	m_levelTimer.start(50);
	connect(&m_thumbTimer, &QTimer::timeout, this, [this]() { m_thumbTick++; emit thumbTickChanged(); });
	connect(&m_placeTimer, &QTimer::timeout, this, [this]() { resolvePlacements(++m_placeTries >= 40); });
	m_hotkeys = new Hotkeys([this](const QString &a, bool pressed) { runHotkey(a, pressed); }, this);
	QTimer::singleShot(0, this, &App::rebuildHotkeys);
	connect(engine, &Engine::replayChanged, this, &App::engineChanged);
	connect(engine, &Engine::vcamChanged, this, &App::engineChanged);
	connect(engine, &Engine::replaySaved, this, [this](const QString &path) { showToast("Replay saved: " + QFileInfo(path).fileName()); });
	if (!QDir::cleanPath(QFileInfo(m_docPath).absolutePath()).startsWith(QDir::cleanPath(QDir::tempPath())))
		m_projectFile = QSettings().value("project/file").toString();
	if (!m_projectFile.isEmpty() && !QFile::exists(m_projectFile))
		m_projectFile.clear();
	watchAssets();
	connect(&m_assetWatch, &QFileSystemWatcher::directoryChanged, this, [this]() { emit assetsChanged(); });
	QTimer::singleShot(0, this, &App::adoptExternalFiles);
	m_pixelPerfect = QSettings().value("ui/preview/pixelPerfect", true).toBool();
	m_clickToShow = QSettings().value("ui/scenes/clickToShow", true).toBool();
	m_toolFill = uiValue("tools/fill", "#ef4444");
	if (m_toolFill == "#7c5cff")
		m_toolFill = "#ef4444";
	m_toolStroke = uiValue("tools/stroke", "#ffffff");
	m_toolStrokeWidth = uiValue("tools/strokeWidth", "6").toInt();
	m_laserTrailMs = std::clamp(uiValue("tools/laserTrailMs", "500").toInt(), 150, 3000);
	m_toolFont = uiValue("tools/font", lobs::defaultTextFont());
	m_toolBold = uiValue("tools/bold", "1") == "1";
	m_toolItalic = uiValue("tools/italic", "0") == "1";
	connect(&m_eyedropper, &Eyedropper::picked, this, &App::screenColorPicked);
	connect(&m_eyedropper, &Eyedropper::canceled, this, &App::screenColorCanceled);
	connect(m_engine, &Engine::bandwidthTestProgress, this, &App::bandwidthProgress);
	connect(m_engine, &Engine::bandwidthTestDone, this, &App::bandwidthDone);
	connect(m_engine, &Engine::videoProbed, this, &App::videoProbed);
	m_caretTimer.setInterval(530);
	connect(&m_caretTimer, &QTimer::timeout, this, [this]() { m_caretOn = !m_caretOn; layoutEditedText(); });
	m_engine->setPixelPerfect(m_pixelPerfect);
	connect(&m_statusTimer, &QTimer::timeout, this, &App::refreshSourceStatus);
	connect(&m_sizeTimer, &QTimer::timeout, this, &App::sizeNewLayers);
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

QVariantMap App::takePerfCounters()
{
	const QVariantMap m{{"changes", m_perfChanges}, {"applyMs", m_perfApplyUs / 1000.0}, {"editSceneMs", m_perfEditUs / 1000.0},
			    {"hotkeysMs", m_perfHotkeyUs / 1000.0}, {"qmlMs", m_perfQmlUs / 1000.0}, {"inspectorMs", m_perfInspectorUs / 1000.0}};
	m_perfApplyUs = m_perfEditUs = m_perfHotkeyUs = m_perfQmlUs = m_perfInspectorUs = m_perfChanges = 0;
	return m;
}

void App::onStoreChanged()
{
	++m_perfChanges;
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
	if (!m_saveTimer.isActive())
		m_unsavedSince.start();
	m_saveTimer.start(std::max<qint64>(0, std::min<qint64>(400, 2000 - m_unsavedSince.elapsed())));
	pt.restart();
	rebuildHotkeys();
	m_perfHotkeyUs += pt.nsecsElapsed() / 1000;
	pt.restart();
	emit docChanged();
	m_perfQmlUs += pt.nsecsElapsed() / 1000;
}

bool writeShowFile(const QString &path, const Doc &d)
{
	QSaveFile f(path);
	if (!f.open(QIODevice::WriteOnly))
		return false;
	const QByteArray data = QJsonDocument(d.toJson()).toJson();
	if (f.write(data) != data.size())
		return f.cancelWriting(), false;
	return f.commit();
}

void App::refreshModels()
{
	auto push = [&](const char *key, const QVariant &v, void (App::*signal)()) {
		QVariant &old = m_modelCache[QString::fromLatin1(key)];
		if (old.isValid() && old == v)
			return;
		old = v;
		emit (this->*signal)();
	};
	const bool dragging = m_drag != Drag::None;
	if (dragging)
		m_modelsStale = true;
	if (!dragging) {
		m_modelsStale = false;
		push("scenes", QVariant::fromValue(scenes()), &App::scenesChanged);
		push("pool", QVariant::fromValue(pool()), &App::poolChanged);
	}
	push("layers", QVariant::fromValue(layers()), &App::layersChanged);
	if (m_drag != Drag::None) {
		if (!m_inspectorTimer.isActive())
			m_inspectorTimer.start();
	} else {
		pushInspector(false);
	}
	push("history", QVariant::fromValue(history()), &App::historyChanged);
	if (dragging)
		return;
	push("mixer", QVariant::fromValue(mixer()), &App::mixerChanged);
	push("transitions", QVariant::fromValue(transitions()), &App::transitionsChanged);
	push("hotkeyActions", QVariant::fromValue(hotkeyActions()), &App::hotkeyActionsChanged);
}

void App::pushInspector(bool dragging)
{
	auto pushTransform = [this](const QVariantMap &tr) {
		QVariant &old = m_modelCache["inspectorTransform"];
		if (!old.isValid() || old.toMap() != tr) {
			old = tr;
			emit inspectorTransformChanged();
		}
	};
	const QString id = singleSelection();
	if (dragging && !id.isEmpty()) {
		const Transform t = doc().bounds(id);
		pushTransform({{"x", t.x}, {"y", t.y}, {"w", t.w}, {"h", t.h}, {"rot", t.rot}});
		return;
	}
	const QVariantMap m = inspector();
	QVariantMap tr;
	for (const char *k : {"x", "y", "w", "h", "rot"})
		if (m.contains(QLatin1String(k)))
			tr[QLatin1String(k)] = m.value(QLatin1String(k));
	pushTransform(tr);
	QVariant &old = m_modelCache["inspector"];
	if (!old.isValid() || old.toMap() != m) {
		old = m;
		emit inspectorChanged();
	}
}

void App::autosave()
{
	if (m_autosaveBusy) {
		m_autosaveAgain = true;
		return;
	}
	m_autosaveBusy = true;
	const Doc snapshot = doc();
	const QString path = m_docPath;
	QThreadPool::globalInstance()->start([this, snapshot, path] {
		QDir().mkpath(QFileInfo(path).absolutePath());
		const bool ok = writeShowFile(path, snapshot);
		QMetaObject::invokeMethod(this, [this, ok] {
			m_autosaveBusy = false;
			if (!ok && !m_autosaveFailed)
				showToast("Couldn't autosave. Check that there's space on the drive.");
			m_autosaveFailed = !ok;
			if (std::exchange(m_autosaveAgain, false))
				autosave();
		});
	});
}

void App::showToast(const QString &msg)
{
	m_toast = msg;
	emit toastChanged();
	m_toastTimer.start(2200);
}

QVariantList App::scenes() const
{
	QVariantList out;
	for (const QString &id : doc().sceneOrder) {
		const Scene &s = doc().scenes[id];
		out << QVariantMap{{"id", id}, {"name", s.name}, {"count", s.layers.size()}, {"favorite", s.favorite},
				   {"editing", id == doc().mainScene(m_editScene)}, {"program", id == m_engine->programScene()}};
	}
	return out;
}

bool isAssetKind(const QString &kind) { return kind == "image" || kind == "media" || kind == "text" || kind == "color" || kind == "shape"; }

void pruneUnusedAssetSources(Doc &d)
{
	QSet<QString> used;
	for (const Instance &i : d.instances)
		used.insert(i.sourceId);
	for (const QString &id : QStringList(d.sourceOrder))
		if (isAssetKind(d.sources[id].kind) && !used.contains(id))
			d.deleteSource(id);
}

static QString poolCategory(const QString &kind)
{
	if (kind == "camera") return "Cameras";
	if (kind == "display" || kind == "window" || kind == "game") return "Screen capture";
	if (kind == "mic" || kind == "desktop_audio") return "Audio";
	return "Media";
}

QVariantList App::pool() const
{
	QVariantList out;
	struct Group { QString name, bin, category; };
	QVector<Group> groups;
	QSet<QString> inScene;
	QHash<QString, QStringList> usedIn;
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
				continue;
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

QVariantList App::layers() const { return layerRows(false); }
QVariantList App::allLayers() const { return layerRows(true); }

QVariantList App::layerRows(bool everything) const
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
			const bool collapsed = i.kind == "group" && m_collapsedGroups.contains(id) && !hasSelectedDescendant(id);
			out << QVariantMap{{"id", id}, {"name", i.name}, {"depth", depth}, {"group", i.kind == "group"},
					   {"collapsed", collapsed}, {"childCount", i.kind == "group" ? i.children.size() : 0},
					   {"icon", i.kind == "group" ? (collapsed ? "folder" : "folder-open") : (s ? kindInfo(s->kind).icon : "square")}, {"color", s ? s->color : "#555"},
					   {"visible", doc().visibility(i).visible}, {"locked", i.locked}, {"linked", linked},
					   {"fx", i.kind == "group" ? 0 : doc().filters(i).size()}, {"selected", m_selection.contains(id)},
					   {"background", isBackgroundLayer(doc(), id)}};
			if (i.kind == "group" && (!collapsed || everything))
				walk(i.children, depth + 1);
		}
	};
	walk(doc().scenes[m_editScene].layers, 0);
	return out;
}

bool App::hasSelectedDescendant(const QString &groupId) const
{
	for (const QString &id : m_selection)
		if (id != groupId && doc().descendants(groupId).contains(id))
			return true;
	return false;
}

void App::setGroupCollapsed(const QString &id, bool collapsed)
{
	if (!doc().instances.contains(id) || doc().instances[id].kind != "group")
		return;
	if (collapsed) {
		m_collapsedGroups.insert(id);
		if (hasSelectedDescendant(id))
			m_selection = {id}, updateOverlay(), emit docChanged();
	} else {
		m_collapsedGroups.remove(id);
	}
	QSettings().setValue("ui/layers/collapsed", QStringList(m_collapsedGroups.begin(), m_collapsedGroups.end()));
	refreshModels();
}

QString App::singleSelection() const
{
	return m_selection.size() == 1 ? m_selection.first() : QString();
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
	return doc().scenes.value(doc().mainScene(m_editScene)).name;
}

void App::editScene(const QString &id)
{
	if (!doc().scenes.contains(id))
		return;
	if (editingVertical() && doc().scenes[id].verticalOf.isEmpty() && doc().mainScene(m_editScene) != id) {
		QString v = doc().verticalLayout(id);
		if (v.isEmpty())
			m_store.commit("Vertical layout", [&](Doc &d) { v = d.addVerticalLayout(id); });
		return editScene(v);
	}
	m_editScene = id;
	m_selection.clear();
	m_engine->setEditScene(id);
	updateOverlay();
	emit docChanged();
}

void App::setVerticalOn(bool on)
{
	if (on == doc().verticalOn)
		return;
	m_store.commit(on ? "Vertical canvas on" : "Vertical canvas off", [&](Doc &d) {
		d.verticalOn = on;
		if (on)
			for (const QString &id : d.sceneOrder)
				d.addVerticalLayout(id);
	});
	if (!on && editingVertical())
		editScene(doc().mainScene(m_editScene));
}

void App::setEditingVertical(bool vertical)
{
	const QString main = doc().mainScene(m_editScene);
	if (!vertical)
		return editScene(main);
	if (!doc().verticalOn)
		return;
	QString v = doc().verticalLayout(main);
	if (v.isEmpty())
		m_store.commit("Vertical layout", [&](Doc &d) { v = d.addVerticalLayout(main); });
	editScene(v);
}

void App::take()
{
	m_engine->setProgramScene(doc().mainScene(m_editScene), true);
	showToast("On air: " + editSceneName());
	emit docChanged();
}

bool isBackgroundLayer(const Doc &d, const QString &id)
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

void placeBackground(Doc &d, const QString &sceneId)
{
	const QString sid = d.addSource("color", "Background", {{"color", "#000000"}, {"background", true}}).id;
	Instance &bg = d.placeSource(sceneId, sid);
	bg.locked = true;
	const QString iid = bg.id;
	QStringList &layers = d.scenes[sceneId].layers;
	layers.removeAll(iid);
	layers.append(iid);
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

void App::addBackground()
{
	if (!doc().scenes.contains(m_editScene) || !liveSceneEditAllowed(m_editScene))
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
	if (doc().sceneOrder.size() <= 1 && doc().sceneOrder.contains(id))
		return showToast("A show needs at least one scene");
	if (!liveSceneEditAllowed(id))
		return;
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

void App::setGridVisible(bool on)
{
	m_engine->setAllScenesShowing(on);
	on ? m_thumbTimer.start(1000) : m_thumbTimer.stop();
	if (on) { m_thumbTick++; emit thumbTickChanged(); }
}

void App::setClickToShow(bool on)
{
	if (on == m_clickToShow)
		return;
	m_clickToShow = on;
	setUiFlag("scenes/clickToShow", on);
	emit clickToShowChanged();
}

void App::pickScene(const QString &id)
{
	editScene(id);
	if (m_clickToShow)
		take();
}

void App::setPixelPerfect(bool on)
{
	m_pixelPerfect = on;
	m_engine->setPixelPerfect(on);
	setUiFlag("preview/pixelPerfect", on);
	updateOverlay();
	emit previewScaleChanged();
}

Filter duckFilter(const QString &voiceId)
{
	QVariantMap st = audioFxDefaults("duck");
	st.insert({{"threshold", -30}, {"ratio", 6}, {"attack_time", 10}, {"release_time", 400}, {"sidechain_source", voiceId}});
	return Filter{uid("afx_"), "duck", true, st};
}

void App::addSource(const QString &kind, const QString &name, const QString &extra, bool place, const QVariantMap &more)
{
	if (place && !liveSceneEditAllowed(m_editScene))
		return;
	QVariantMap settings;
	if (kind == "text") settings = {{"text", extra.isEmpty() ? "Text" : extra}, {"size", 96}};
	else if (kind == "color") settings = {{"color", extra.isEmpty() ? "#1d1b2e" : extra}};
	else if (kind == "image" || kind == "media") settings = {{"file", extra}};
	else if (kind == "browser") {
		QString url = extra.trimmed();
		if (!url.isEmpty() && !url.contains("://") && !QFileInfo(url).isAbsolute())
			url.prepend("https://");
		settings = {{"url", url}};
		for (auto it = more.begin(); it != more.end(); ++it)
			if (it.key() != "url" && it.value().isValid())
				settings[it.key()] = it.value();
	}
	else if (kind == "camera" || kind == "display" || kind == "window" || kind == "mic" || kind == "desktop_audio") {
		if (const QString dev = !extra.isEmpty() ? extra : kind == "window" ? QString() : firstRealDevice(m_engine, kind); !dev.isEmpty())
			settings = {{"device", dev}};
	}
	QString sid, iid;
	const KindInfo &k = kindInfo(kind);
	m_store.commit("Add " + (name.isEmpty() ? k.label : name), [&](Doc &d) {
		sid = d.addSource(kind, name.trimmed().isEmpty() ? k.label : name.trimmed(), settings).id;
		if (kind == "browser" && settings.value("controlAudio", true).toBool())
			d.sources[sid].audio.monitor = "monitor_and_output";
		if (more.value("mono").toBool())
			d.sources[sid].audio.mono = true;
		if (kind == "game" && std::any_of(d.sources.cbegin(), d.sources.cend(), [](const Source &o) { return o.kind == "desktop_audio" && !o.audio.muted; }))
			d.sources[sid].audio.muted = true;
		if (d.sources[sid].hasAudio && kind != "mic" && d.sources.contains(d.duckUnder))
			d.sources[sid].audio.filters << duckFilter(d.duckUnder);
		if (place && k.video && d.scenes.contains(m_editScene))
			iid = d.placeSource(m_editScene, sid).id;
	});
	if (kind.startsWith("obs:"))
		settleObsLists("source", sid, -1);
	if (!iid.isEmpty()) {
		m_selection = {iid}, m_poolSelection.clear();
		if (k.w <= 0)
			waitForSize(iid);
	} else {
		m_poolSelection = sid, m_selection.clear();
	}
	updateOverlay();
	emit docChanged();
}

void App::waitForSize(const QString &instanceId)
{
	m_sizeWhenKnown.insert(instanceId, QDateTime::currentMSecsSinceEpoch() + 30000);
	if (!m_sizeTimer.isActive())
		m_sizeTimer.start(50);
}

void App::sizeNewLayers()
{
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	for (auto it = m_sizeWhenKnown.begin(); it != m_sizeWhenKnown.end();) {
		const auto inst = doc().instances.constFind(it.key());
		const QSize ns = inst != doc().instances.constEnd() ? m_engine->sourceSize(inst->sourceId) : QSize();
		if (inst == doc().instances.constEnd() || now > it.value()) {
			it = m_sizeWhenKnown.erase(it);
			continue;
		}
		if (ns.isEmpty()) {
			++it;
			continue;
		}
		const QString id = it.key();
		it = m_sizeWhenKnown.erase(it);
		const Source src = doc().sources.value(inst->sourceId);
		if (doc().transform(*inst) != doc().defaultProps(&src).transform)
			continue;
		const Crop c = doc().crop(*inst);
		const double w = std::max(1, ns.width() - c.l - c.r), h = std::max(1, ns.height() - c.t - c.b);
		const double W = editCanvas().width(), H = editCanvas().height(), k = std::min({1.0, W / w, H / h});
		Transform t = doc().transform(*inst);
		t.w = std::round(w * k), t.h = std::round(h * k);
		t.x = std::round((W - t.w) / 2), t.y = std::round((H - t.h) / 2);
		m_store.amend([&](Doc &d) { d.setTransform(id, t); });
		updateOverlay();
	}
	if (m_sizeWhenKnown.isEmpty())
		m_sizeTimer.stop();
}

bool App::nestsSceneInItself(const QString &sourceId, const QString &sceneId)
{
	const Source &s = doc().sources[sourceId];
	if (s.kind != "scene" || !doc().sceneReaches(s.settings.value("scene").toString(), sceneId))
		return false;
	showToast("A scene can't contain itself, even through another scene");
	return true;
}

void App::addSceneSource(const QString &sceneId)
{
	if (!doc().scenes.contains(sceneId))
		return;
	QString sid;
	for (const QString &id : doc().sourceOrder)
		if (doc().sources[id].kind == "scene" && doc().sources[id].settings.value("scene").toString() == sceneId)
			sid = id;
	if (doc().sceneReaches(sceneId, m_editScene))
		return (void)showToast("A scene can't contain itself, even through another scene");
	if (!liveSceneEditAllowed(m_editScene))
		return;
	if (sid.isEmpty())
		m_store.commit("Add scene " + doc().scenes[sceneId].name + " as a source",
			       [&](Doc &d) { sid = d.addSource("scene", d.scenes[sceneId].name, {{"scene", sceneId}}).id; });
	placeSource(sid);
}

void App::placeSource(const QString &id)
{
	if (!liveSceneEditAllowed(m_editScene))
		return;
	if (!doc().sources.contains(id) || !doc().scenes.contains(m_editScene))
		return;
	if (!kindInfo(doc().sources[id].kind).video)
		return showToast("Audio sources can't go on the canvas");
	if (nestsSceneInItself(id, m_editScene))
		return;
	QString iid;
	m_store.commit("Place " + doc().sources[id].name, [&](Doc &d) { iid = d.placeSource(m_editScene, id).id; });
	if (kindInfo(doc().sources[id].kind).w <= 0 && doc().sources[id].kind != "scene")
		waitForSize(iid);
	m_selection = {iid};
	m_poolSelection.clear();
	updateOverlay();
	emit docChanged();
}

void App::renameSource(const QString &id, const QString &name)
{
	if (!name.trimmed().isEmpty())
		m_store.commit("Rename source", [&](Doc &d) {
			const QString old = d.sources[id].name;
			for (Instance &i : d.instances)
				if (i.sourceId == id && i.name == old)
					i.name = name.trimmed();
			d.sources[id].name = name.trimmed();
		});
}

void App::deleteSource(const QString &id)
{
	if (!liveSourceEditAllowed(id))
		return;
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

QVariantList App::devices(const QString &sourceId, bool fresh)
{
	if (!doc().sources.contains(sourceId))
		return {};
	const QString kind = doc().sources[sourceId].kind;
	if (!fresh && m_deviceCache.contains(kind))
		return m_deviceCache.value(kind);
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	DeviceList &c = m_sourceDevices[sourceId];
	if (fresh || now - c.at > 5000) {
		c.items = m_engine->listDevices(sourceId, doc().sources[sourceId].kind);
		c.at = now;
	}
	return c.items;
}

void App::setSourceSetting(const QString &id, const QString &key, const QVariant &value)
{
	if (!liveSourceEditAllowed(id))
		return;
	m_store.commit("Edit " + doc().sources.value(id).name, [&](Doc &d) { d.sources[id].settings[key] = value; });
}

void App::refreshSourceStatus()
{
	updateBlockedLayers();
	if (m_sourceTick % 30 == 0)
		m_deviceCache.clear();
	++m_sourceTick;
	QVariantMap out;
	for (const QString &id : doc().sourceOrder) {
		const Source &s = doc().sources[id];
		if (isAssetKind(s.kind))
			continue;
		const QString dev = s.settings.value("device").toString();
		QString detail, status, statusText;
		if (!m_engine->kindAvailable(s.kind)) {
			status = "missing", statusText = "Not on this OS";
			detail = s.kind == "game" ? "Game capture is Windows-only" : "Not available here";
		} else if (!Permissions::allows(s.kind)) {
			const QString p = Permissions::forKind(s.kind);
			status = "permission", statusText = "Needs access";
			detail = "Allow " + Permissions::title(p) + " for LOBS";
		} else if (QStringList{"camera", "display", "window", "mic", "desktop_audio"}.contains(s.kind) || (s.kind == "game" && !dev.isEmpty())) {
			if (!m_deviceCache.contains(s.kind))
				m_deviceCache[s.kind] = m_engine->listDevices({}, s.kind);
			const QVariantList &devs = m_deviceCache[s.kind];
			QString devName;
			for (const QVariant &v : devs)
				if (v.toMap()["value"].toString() == dev)
					devName = v.toMap()["name"].toString();
			if (dev.isEmpty() || dev == "default")
#ifdef Q_OS_WIN
				detail = s.kind == "mic" || s.kind == "desktop_audio" ? "Windows default device" : "No device chosen";
#else
				detail = s.kind == "mic" || s.kind == "desktop_audio" ? "System default device" : "No device chosen";
#endif
			else if (devName.isEmpty() && !devs.isEmpty() && s.kind != "window" && s.kind != "game")
				status = "missing", statusText = "Not connected";
			else
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
	const QVariantList prevMissing = m_missingCache;
	const bool missingChanged = missingAssets() != prevMissing;
	if (out != m_sourceStatus || missingChanged) {
		m_sourceStatus = out;
		emit sourceStatusChanged();
	}
	emit sourceTickChanged();
}

void App::toggleInScene(const QString &sourceId)
{
	if (!liveSceneEditAllowed(m_editScene))
		return;
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

void App::selectLayer(const QString &id, bool additive)
{
	if (additive)
		m_selection.contains(id) ? (void)m_selection.removeAll(id) : m_selection.append(id);
	else
		m_selection = {id};
	m_selectAnchor = id;
	m_poolSelection.clear();
	updateOverlay();
	emit docChanged();
}

void App::selectLayerRange(const QString &id)
{
	QStringList rows;
	QSet<QString> background;
	for (const QVariant &v : layers()) {
		const QVariantMap r = v.toMap();
		rows << r["id"].toString();
		if (r["background"].toBool())
			background.insert(rows.last());
	}
	const int a = int(rows.indexOf(m_selectAnchor)), b = int(rows.indexOf(id));
	if (a < 0 || b < 0)
		return selectLayer(id, false);
	QStringList range = rows.mid(std::min(a, b), std::abs(a - b) + 1);
	range.removeIf([&](const QString &r) { return background.contains(r) && r != id; });
	QStringList sel;
	for (const QString &r : range) {
		bool inSelectedGroup = false;
		for (const QString &g : range)
			inSelectedGroup |= g != r && doc().instances[g].kind == "group" && doc().descendants(g).contains(r);
		if (!inSelectedGroup)
			sel << r;
	}
	m_selection = sel;
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
	if (isBackgroundLayer(doc(), id) || !liveEditAllowed({id}))
		return;
	const QStringList &sib = doc().scenes.value(doc().instances.value(id).sceneId).layers;
	if (delta > 0 && sib.contains(id) && !sib.isEmpty() && isBackgroundLayer(doc(), sib.last()) && sib.indexOf(id) + delta >= sib.size() - 1)
		delta = int(sib.size()) - 2 - int(sib.indexOf(id));
	if (delta == 0)
		return;
	m_store.commit(delta < 0 ? "Move layer up" : "Move layer down", [&](Doc &d) { d.moveLayer(id, delta); });
}

void App::moveSelected(int delta)
{
	if (m_selection.isEmpty() || !liveEditAllowed(m_selection))
		return;
	const QStringList ids = m_selection;
	const QString label = delta <= -100 ? "Bring to front" : delta >= 100 ? "Send to back" : delta < 0 ? "Bring forward" : "Send backward";
	m_store.commit(label, [&](Doc &d) {
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
			if (step > 0 && !sib.isEmpty() && isBackgroundLayer(d, sib.last()))
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
	if (!liveEditAllowed({id, target}))
		return;
	if (mode == "into" && cur.instances[target].kind != "group")
		return;
	if (isBackgroundLayer(cur, id) || (isBackgroundLayer(cur, target) && mode != "above"))
		return;
	const QString label = mode == "group" ? "Group layers" : mode == "into" ? "Move into group" : "Move layer";
	QString gid;
	m_store.commit(label, [&](Doc &d) {
		const Instance *oldParent = d.parentOf(id);
		const QString oldParentId = oldParent ? oldParent->id : QString();
		d.siblings(id).removeAll(id);
		if (mode == "into") {
			d.instances[target].children.prepend(id);
		} else {
			QStringList &to = d.siblings(target);
			to.insert(to.indexOf(target) + (mode == "below" ? 1 : 0), id);
			if (mode == "group")
				gid = d.groupLayers({id, target});
		}
		if (!oldParentId.isEmpty() && d.instances.contains(oldParentId) && d.instances[oldParentId].children.isEmpty())
			d.deleteInstance(oldParentId);
	});
	m_selection = {gid.isEmpty() ? id : gid};
	updateOverlay();
	emit docChanged();
}

void App::renameLayer(const QString &id, const QString &name)
{
	if (!liveEditAllowed({id}))
		return;
	if (!name.trimmed().isEmpty())
		m_store.commit("Rename layer", [&](Doc &d) { d.instances[id].name = name.trimmed(); });
}

void App::deleteSelected()
{
	if (m_selection.isEmpty() || !liveEditAllowed(m_selection))
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
	if (!liveEditAllowed(m_selection))
		return;
	QString gid;
	const QStringList ids = m_selection;
	if (!ids.isEmpty()) {
		Doc probe = doc();
		const QStringList level = probe.siblings(ids.first());
		for (const QString &id : ids)
			if (!level.contains(id))
				return showToast("To group layers, pick layers at the same level");
	}
	m_store.commit("Group layers", [&](Doc &d) { gid = d.groupLayers(ids); });
	if (!gid.isEmpty())
		m_selection = {gid}, updateOverlay(), emit docChanged();
}

void App::ungroupSelected()
{
	const QString id = singleSelection();
	if (!liveEditAllowed({id}))
		return;
	if (!id.isEmpty() && doc().instances[id].kind == "group")
		m_store.commit("Ungroup", [&](Doc &d) { d.ungroup(id); });
}

void App::duplicateSelected()
{
	if (!liveEditAllowed(m_selection))
		return;
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

void App::copyLayers()
{
	QStringList ids;
	for (const QString &id : m_selection) {
		bool inSelectedGroup = false;
		for (const QString &other : m_selection)
			inSelectedGroup |= other != id && doc().descendants(other).contains(id);
		if (doc().instances.contains(id) && !inSelectedGroup && !isBackgroundLayer(doc(), id))
			ids << id;
	}
	if (ids.isEmpty())
		return;
	m_clipIds = ids;
	m_clipDoc = doc();
	emit clipboardChanged();
	showToast(ids.size() == 1 ? "Copied " + doc().instances[ids[0]].name : QString("Copied %1 layers").arg(ids.size()));
}

void App::cutLayers()
{
	if (m_selection.isEmpty() || !liveEditAllowed(m_selection))
		return;
	copyLayers();
	if (m_clipIds.isEmpty())
		return;
	deleteSelected();
	showToast(m_clipIds.size() == 1 ? "Cut " + m_clipDoc.instances[m_clipIds[0]].name : QString("Cut %1 layers").arg(m_clipIds.size()));
}

void App::pasteLayers(bool linked)
{
	if (m_clipIds.isEmpty() || !doc().scenes.contains(m_editScene) || !liveSceneEditAllowed(m_editScene))
		return;
	QStringList made;
	const QString scene = m_editScene;
	m_store.commit(linked ? "Paste linked" : "Paste", [&](Doc &d) {
		QStringList ids, temps;
		std::function<void(const QString &)> restore = [&](const QString &id) {
			Instance i = m_clipDoc.instances.value(id);
			i.props.transform = m_clipDoc.transform(i), i.props.crop = m_clipDoc.crop(i);
			i.props.filters = m_clipDoc.filters(i), i.props.visibility = m_clipDoc.visibility(i);
			i.links.clear();
			d.instances[id] = i;
			temps << id;
			if (i.kind != "group" && !d.sources.contains(i.sourceId) && m_clipDoc.sources.contains(i.sourceId)) {
				d.sources[i.sourceId] = m_clipDoc.sources[i.sourceId];
				d.sourceOrder << i.sourceId;
			}
			for (const QString &c : i.children)
				restore(c);
		};
		for (const QString &id : m_clipIds) {
			if (!d.instances.contains(id)) {
				if (!m_clipDoc.instances.contains(id))
					continue;
				restore(id);
			}
			ids << id;
		}
		made = d.copyToScene(ids, scene, linked && temps.isEmpty());
		for (const QString &t : temps)
			d.instances.remove(t);
		QSet<QString> seen;
		for (int k = 0; k < made.size(); ++k)
			if (d.instances.value(ids.value(k)).sceneId == scene)
				d.moveBy(made[k], 20, 20, seen);
	});
	if (made.isEmpty())
		return;
	m_selection = made;
	updateOverlay();
	emit docChanged();
}

void App::copySelectedTo(const QString &sceneId, bool linked)
{
	if (!liveSceneEditAllowed(sceneId))
		return;
	if (m_selection.isEmpty() || !doc().scenes.contains(sceneId))
		return;
	const QStringList ids = m_selection;
	m_store.commit(QString("Copy to %1 (%2)").arg(doc().scenes[sceneId].name, linked ? "linked" : "independent"),
		       [&](Doc &d) { d.copyToScene(ids, sceneId, linked); });
	showToast(QString("Copied to %1 as %2").arg(doc().scenes[sceneId].name, linked ? "linked" : "independent"));
}

bool App::liveSceneEditAllowed(const QString &sceneId)
{
	if (!liveLocked() || sceneId != m_engine->programScene() || (QGuiApplication::queryKeyboardModifiers() & Qt::AltModifier))
		return true;
	showToast("On-air layers are locked. Hold Alt or click Unlock to edit.");
	return false;
}

bool App::liveSourceEditAllowed(const QString &sourceId)
{
	if (!liveLocked() || (QGuiApplication::queryKeyboardModifiers() & Qt::AltModifier))
		return true;
	const QString program = m_engine->programScene();
	for (const Instance &i : doc().instances)
		if (i.sourceId == sourceId && i.sceneId == program)
			return showToast("On-air sources are locked. Hold Alt or click Unlock to edit."), false;
	return true;
}

bool App::liveEditAllowed(const QStringList &ids)
{
	if (!liveLocked() || (QGuiApplication::queryKeyboardModifiers() & Qt::AltModifier))
		return true;
	const QString program = m_engine->programScene();
	for (const QString &id : ids)
		if (doc().instances.value(id).sceneId == program && !program.isEmpty()) {
			showToast("On-air layers are locked. Hold Alt or click Unlock to edit.");
			return false;
		}
	return true;
}

void App::nudge(int dx, int dy)
{
	if (!liveEditAllowed(m_selection))
		return;
	if (m_selection.isEmpty())
		return;
	const QStringList ids = m_selection;
	m_store.commit("Nudge", [&](Doc &d) {
		QSet<QString> seen;
		for (const QString &id : ids)
			d.moveBy(id, dx, dy, seen);
	});
}

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

void App::forgetProject()
{
	if (m_projectFile.isEmpty())
		return;
	m_projectFile.clear();
	QSettings().remove("project/file");
	watchAssets();
	emit assetsChanged();
	emit projectChanged();
}

void App::newDocument()
{
	if (live())
		return showToast("Stop streaming and recording to start a new show");
	m_store.replace(Doc(), "New document");
	forgetProject();
	m_poolSelection.clear();
	m_editScene.clear();
	addScene();
	m_cleanDoc = doc();
}

void App::loadSample()
{
	if (live())
		return showToast("Stop streaming and recording to load the sample show");
	m_store.replace(withDefaultDevices(Doc::sample(), m_engine), "Load sample show");
	forgetProject();
	editScene(doc().sceneOrder.value(0));
	m_cleanDoc = doc();
}

bool App::saveProject()
{
	if (m_projectFile.isEmpty())
		return false;
	if (!writeShowFile(m_projectFile, doc()))
		return showToast("Couldn't save " + m_projectFile), false;
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
	static const QRegularExpression reserved(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\..*)?$)", QRegularExpression::CaseInsensitiveOption);
	if (reserved.match(n).hasMatch())
		return "\"" + n + "\" is reserved by Windows. Choose another name.";
	const QString f = cleanFolder(folder);
	if (f.isEmpty() || !QDir::isAbsolutePath(f))
		return "Choose where to keep it.";
	if ((f + "/" + n + "/" + n + ".lobs.json").size() > 200)
		return "That name is too long for this folder. Use a shorter name or folder.";
	if (QFileInfo::exists(f + "/" + n))
		return "There's already a folder named \"" + n + "\" there.";
	return {};
}

bool App::createProject(const QString &name, const QString &folder)
{
	if (!checkNewProject(name, folder).isEmpty())
		return false;
	if (live())
		return showToast("Stop streaming and recording to start a new project"), false;
	const QString dir = cleanFolder(folder) + "/" + name.trimmed();
	const QString file = dir + "/" + name.trimmed() + ".lobs.json";
	if (!m_projectFile.isEmpty() && !saveProject())
		return false;
	Doc fresh;
	const QString scene = fresh.addScene("Scene").id;
	placeBackground(fresh, scene);
	if (!QDir().mkpath(dir + "/assets") || !writeShowFile(file, fresh)) {
		QDir(dir).removeRecursively();
		return showToast("Couldn't create " + QDir::toNativeSeparators(dir)), false;
	}
	m_store.replace(fresh, "New project");
	m_poolSelection.clear();
	m_editScene.clear();
	editScene(scene);
	m_projectFile = file;
	rememberProject();
	watchAssets();
	adoptExternalFiles();
	emit assetsChanged();
	emit projectChanged();
	showToast("Created " + name.trimmed());
	return true;
}

void App::rememberProject()
{
	QSettings settings;
	settings.setValue("project/file", m_projectFile);
	QStringList recent = settings.value("project/recent").toStringList();
	recent.removeAll(m_projectFile);
	recent.prepend(m_projectFile);
	settings.setValue("project/recent", recent.mid(0, 10));
	emit recentShowsChanged();
}

QStringList App::recentShows() const
{
	QStringList out;
	for (const QString &f : QSettings().value("project/recent").toStringList())
		if (QFileInfo::exists(f))
			out << f;
	return out;
}

void App::clearRecentShows()
{
	QSettings().remove("project/recent");
	emit recentShowsChanged();
}

bool App::saveAs(const QUrl &url)
{
	const QString path = url.toLocalFile();
	if (!writeShowFile(path, doc()))
		return showToast("Couldn't save " + path), false;
	m_projectFile = path;
	rememberProject();
	watchAssets();
	adoptExternalFiles();
	emit assetsChanged();
	showToast("Saved " + QFileInfo(path).fileName());
	emit projectChanged();
	return true;
}

bool App::open(const QUrl &url)
{
	if (live())
		return showToast("Stop streaming and recording to open another show"), false;
	QFile f(url.toLocalFile());
	if (!f.open(QIODevice::ReadOnly))
		return showToast("Couldn't open " + url.toLocalFile()), false;
	const QJsonDocument j = QJsonDocument::fromJson(f.readAll());
	if (j.isObject() && isObsCollection(j.object()))
		return f.close(), importObs(url.toLocalFile());
	if (!j.isObject() || j["version"].toInt() != 1)
		return showToast("Not a LOBS document"), false;
	if (!m_projectFile.isEmpty() && !saveProject())
		return false;
	Doc opened = Doc::fromJson(j.object());
	if (const int n = opened.repair())
		showToast(QString("Repaired %1 broken reference%2 in this show").arg(n).arg(n == 1 ? "" : "s"));
	m_store.replace(withDefaultDevices(opened, m_engine), "Open " + QFileInfo(f).fileName());
	m_projectFile = url.toLocalFile();
	rememberProject();
	watchAssets();
	adoptExternalFiles();
	emit assetsChanged();
	editScene(doc().sceneOrder.value(0));
	return true;
}

QString App::dataFolder() const
{
	return QDir::toNativeSeparators(QFileInfo(m_docPath).absolutePath());
}

bool App::uiFlag(const QString &key, bool fallback) const { return QSettings().value("ui/" + key, fallback).toBool(); }
void App::setUiFlag(const QString &key, bool on) { QSettings().setValue("ui/" + key, on); }
QString App::uiValue(const QString &key, const QString &fallback) const { return QSettings().value("ui/" + key, fallback).toString(); }
void App::setUiValue(const QString &key, const QString &value) { QSettings().setValue("ui/" + key, value); }

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
