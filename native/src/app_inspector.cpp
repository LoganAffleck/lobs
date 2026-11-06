#include "app_internal.h"
#include "frontend.h"
#include "model/video_fx.h"
#include "plugins.h"

#include <obs.h>
#include <util/base.h>

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QSet>
#include <QUrl>
#include <algorithm>
#include <cmath>

using namespace lobs;

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
		const Source src = d.sources.value(i.sourceId);
		const PropMap def = d.defaultProps(i.kind == "group" ? nullptr : &src);
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
			m["sourceKind"] = s.kind;
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
			if (!ns.isEmpty())
				m["visibleAspect"] = double(ns.width() - c.l - c.r) / std::max(1, ns.height() - c.t - c.b);
			QVariantList fs;
			for (const Filter &f : d.filters(i)) {
				QVariantList params;
				if (const VfxType *t = videoFx(f.type))
					for (const VfxParam &p : t->params)
						params << QVariantMap{{"key", p.key}, {"label", p.label}, {"kind", p.kind}, {"value", videoFxValue(f.settings, p)},
								      {"min", p.min}, {"max", p.max}, {"decimals", p.decimals}, {"scale", p.scale}, {"suffix", p.suffix},
								      {"options", p.options}, {"fileFilter", p.fileFilter}, {"visible", videoFxParamActive(f.settings, *t, p)}};
				fs << QVariantMap{{"type", f.type}, {"label", filterLabel(f.type)}, {"enabled", f.enabled}, {"params", params},
						  {"obs", f.type.startsWith("obs:")}, {"loaded", !f.type.startsWith("obs:") || !pluginTypeLabel(f.type).isEmpty()}, {"layer", i.id}};
			}
			m["filters"] = fs;
			m["source"] = sourceInfo(s);
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

QVariantList App::filterTypes() const
{
	QVariantList out;
	for (const VfxType &t : videoFxTypes())
		out << QVariantMap{{"name", t.label}, {"value", t.type}};
	for (const QString &id : lobs::plugins::filterTypes()) {
		const uint32_t flags = obs_get_source_output_flags(id.toUtf8().constData());
		if ((flags & OBS_SOURCE_VIDEO) && !(flags & (OBS_SOURCE_CAP_DISABLED | OBS_SOURCE_DEPRECATED)))
			out << QVariantMap{{"name", pluginTypeLabel("obs:" + id) + " (plugin)"}, {"value", "obs:" + id}};
	}
	return out;
}

QVariantList App::plugins() const
{
	const auto names = [](const QStringList &ids) {
		QStringList n;
		for (const QString &id : ids) {
			const QString l = pluginTypeLabel("obs:" + id);
			n << (l.isEmpty() ? id : l);
		}
		return n;
	};
	QVariantList out;
	for (const lobs::plugins::Plugin &p : lobs::plugins::list())
		out << QVariantMap{{"id", p.name}, {"name", p.name}, {"title", p.title}, {"version", p.version}, {"folder", QDir::toNativeSeparators(p.folder)},
				   {"status", p.status}, {"reason", p.reason}, {"sources", names(p.sources)}, {"filters", names(p.filters)},
				   {"transitions", names(p.transitions)}, {"other", p.other}, {"sourceIds", p.sources}, {"filterIds", p.filters}};
	return out;
}

bool App::pluginsNeedRestart() const { return lobs::plugins::restartNeeded(); }
QString App::pluginsFolder() const { return QDir::toNativeSeparators(lobs::plugins::folder()); }
void App::openPluginsFolder() const { QDesktopServices::openUrl(QUrl::fromLocalFile(lobs::plugins::folder())); }
void App::copyText(const QString &text) const { QGuiApplication::clipboard()->setText(text); }
QVariantList App::pluginDocks() const { return lobs::frontend::docks(); }
QStringList App::pluginTools() const { return lobs::frontend::toolsItems(); }
void App::triggerPluginTool(const QString &name) const { lobs::frontend::triggerTool(name); }

QString App::addPlugin(const QString &pathOrUrl)
{
	const QUrl url(pathOrUrl);
	const QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
	QStringList names;
	const QString err = lobs::plugins::install(path, &names);
	blog(err.isEmpty() ? LOG_INFO : LOG_WARNING, "[lobs] add plugin %s: %s", path.toUtf8().constData(),
	     (err.isEmpty() ? names.join(", ") : err).toUtf8().constData());
	emit pluginsChanged();
	return err;
}

QString App::removePlugin(const QString &name)
{
	const QString err = lobs::plugins::remove(name);
	emit pluginsChanged();
	return err;
}

void App::setPluginEnabled(const QString &name, bool on)
{
	lobs::plugins::setEnabled(name, on);
	emit pluginsChanged();
}

QVariantList App::pluginSourceTypes() const
{
	registerPluginKinds();
	QVariantList out;
	for (auto it = lobs::pluginKinds().constBegin(); it != lobs::pluginKinds().constEnd(); ++it)
		out << QVariantMap{{"kind", it.key()}, {"label", it->label}, {"icon", it->icon}, {"video", it->video}};
	std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) { return a.toMap()["label"].toString().localeAwareCompare(b.toMap()["label"].toString()) < 0; });
	return out;
}

static obs_source_t *propertiesTarget(const Engine *engine, const Doc &d, const QString &scope, const QString &id, int index)
{
	if (scope == "source")
		return engine->inputSource(id);
	if (scope == "filter" && d.instances.contains(id)) {
		const QVector<Filter> fs = d.filters(d.instances[id]);
		return index >= 0 && index < fs.size() ? engine->layerFilter(id, fs[index].id) : nullptr;
	}
	if (scope == "audio" && d.sources.contains(id)) {
		const QVector<Filter> &fs = d.sources[id].audio.filters;
		return index >= 0 && index < fs.size() ? engine->audioFilter(id, fs[index].id) : nullptr;
	}
	return nullptr;
}

QVariantList App::obsProperties(const QString &scope, const QString &id, int index) const
{
	return lobs::plugins::describe(propertiesTarget(m_engine, doc(), scope, id, index));
}

void App::setObsProperty(const QString &scope, const QString &id, int index, const QString &key, const QVariant &value, bool final)
{
	obs_source_t *target = propertiesTarget(m_engine, doc(), scope, id, index);
	if (!target)
		return;
	const QVariant v = lobs::plugins::toObsValue(target, key, value);
	const QVariantMap also = lobs::plugins::modified(target, key, v);
	const auto put = [&](QVariantMap &st) {
		st[key] = v;
		for (auto it = also.begin(); it != also.end(); ++it)
			st[it.key()] = it.value();
	};
	const QString editKey = QString("obsprop:%1:%2:%3:%4").arg(scope, id).arg(index).arg(key);
	if (scope == "source") {
		if (!liveSourceEditAllowed(id))
			return;
		liveEdit(editKey, "Edit " + doc().sources.value(id).name, [&](Doc &d) { put(d.sources[id].settings); }, final);
	} else if (scope == "filter") {
		if (!liveEditAllowed({id}))
			return;
		liveEdit(editKey, "Edit filter", [&](Doc &d) {
			QVector<Filter> fs = d.filters(d.instances[id]);
			if (index >= 0 && index < fs.size()) {
				put(fs[index].settings);
				d.setFilters(id, fs);
			}
		}, final);
	} else if (scope == "audio") {
		liveEdit(editKey, "Edit audio effect", [&](Doc &d) {
			QVector<Filter> &fs = d.sources[id].audio.filters;
			if (index >= 0 && index < fs.size())
				put(fs[index].settings);
		}, final);
	}
}

void App::settleObsLists(const QString &scope, const QString &id, int index)
{
	const QVariantMap fix = lobs::plugins::settleLists(propertiesTarget(m_engine, doc(), scope, id, index));
	if (fix.isEmpty())
		return;
	m_store.amend([&](Doc &d) {
		const auto put = [&](QVariantMap &st) {
			for (auto it = fix.begin(); it != fix.end(); ++it)
				st[it.key()] = it.value();
		};
		if (scope == "source" && d.sources.contains(id)) {
			put(d.sources[id].settings);
		} else if (scope == "filter" && d.instances.contains(id)) {
			QVector<Filter> fs = d.filters(d.instances[id]);
			if (index >= 0 && index < fs.size()) {
				put(fs[index].settings);
				d.setFilters(id, fs);
			}
		} else if (scope == "audio" && d.sources.contains(id)) {
			QVector<Filter> &fs = d.sources[id].audio.filters;
			if (index >= 0 && index < fs.size())
				put(fs[index].settings);
		}
	});
}

void App::clickObsProperty(const QString &scope, const QString &id, int index, const QString &key)
{
	if (lobs::plugins::clickButton(propertiesTarget(m_engine, doc(), scope, id, index), key))
		emit docChanged();
}

void App::setTransform(const QString &field, double value)
{
	const QString id = singleSelection();
	if (id.isEmpty())
		return;
	if (!liveEditAllowed({id}))
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
	if (!liveEditAllowed({id}))
		return;
	const int mode = final ? 2 : 1;
	if (doc().instances[id].kind == "group") {
		if (field != "x" && field != "y")
			return;
		const Doc &base = m_store.gestureBase() ? *m_store.gestureBase() : doc();
		const Transform b = base.bounds(id);
		applyEdit(m_store, "Move group", [&](Doc &d) {
			QSet<QString> seen;
			d.moveBy(id, field == "x" ? value - b.x : 0, field == "y" ? value - b.y : 0, seen);
		}, mode);
		return;
	}
	const Doc &base = m_store.gestureBase() ? *m_store.gestureBase() : doc();
	const bool page = (field == "w" || field == "h") && pageFollowsLayer(base, id);
	const Transform t = withField(base.transform(base.instances[id]), field, value, keepAspect && !page);
	applyEdit(m_store, "Set " + field, [&](Doc &d) { d.setTransform(id, t); if (page) sizePage(d, id, t); }, mode);
}

bool App::pageFollowsLayer(const Doc &d, const QString &id)
{
	const Instance &i = d.instances.value(id);
	const Source &s = d.sources.value(i.sourceId);
	if (s.kind != "browser" || !s.settings.value("responsive", true).toBool())
		return false;
	const Crop c = d.crop(i);
	return !c.l && !c.t && !c.r && !c.b;
}

void App::sizePage(Doc &d, const QString &id, const Transform &t)
{
	QVariantMap &st = d.sources[d.instances[id].sourceId].settings;
	st["width"] = int(std::max(16.0, std::round(t.w)));
	st["height"] = int(std::max(16.0, std::round(t.h)));
}

void App::setSize(const QString &field, double value, bool keepAspect)
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return;
	if (!liveEditAllowed({id}))
		return;
	const bool page = pageFollowsLayer(doc(), id);
	const Transform t = withField(doc().transform(doc().instances[id]), field, value, keepAspect && !page);
	m_store.commit("Resize", [&](Doc &d) { d.setTransform(id, t); if (page) sizePage(d, id, t); });
}

void cropKeepingScale(Doc &d, const QString &id, Crop nc, QSize ns)
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
	t.x += (t.flipH ? nc.r - oc.r : nc.l - oc.l) * sx;
	t.y += (t.flipV ? nc.b - oc.b : nc.t - oc.t) * sy;
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
	if (!liveEditAllowed({id}))
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
	m_cropAspects[id] = std::max(0.0, ratio);
}

double App::cropAspect() const
{
	const QString id = singleSelection();
	if (id.isEmpty() || !doc().instances.contains(id))
		return 0;
	return m_cropAspects.value(id, 0);
}

void App::setCropAspect(double ratio)
{
	const QString id = singleSelection();
	if (id.isEmpty())
		return;
	if (ratio > 0)
		return cropToAspect(ratio);
	m_cropAspects[id] = 0;
	emit docChanged();
}

void App::cancelCrop()
{
	const QString id = singleSelection();
	if (m_cropMode && !id.isEmpty() && m_cropEntry.instances.contains(id) && doc().instances.contains(id)) {
		const Crop c = m_cropEntry.crop(m_cropEntry.instances[id]);
		const Transform t = m_cropEntry.transform(m_cropEntry.instances[id]);
		if (!(c == doc().crop(doc().instances[id])) || !(t == doc().transform(doc().instances[id])))
			m_store.commit("Cancel crop", [&](Doc &d) { d.setCrop(id, c); d.setTransform(id, t); });
	}
	setCropMode(false);
}

void App::setCornerRadius(double pct, bool final)
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return;
	if (!liveEditAllowed({id}))
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
	if (on)
		m_cropEntry = doc();
	updateOverlay();
	emit docChanged();
}

void App::scrubCrop(const QString &field, int value, bool final)
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return;
	if (!liveEditAllowed({id}))
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
	if (!liveEditAllowed({id}))
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
			v.opacity = std::clamp(value, 0.0, 1.0);
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
	if (!liveEditAllowed({id}))
		return;
	const Instance &i = doc().instances[id];
	const QSize ns = m_engine->sourceSize(i.sourceId);
	const Crop c = doc().crop(i);
	Transform t = doc().transform(i);
	const double W = editCanvas().width(), H = editCanvas().height();
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
	if (!liveEditAllowed(m_selection))
		return;
	const QString id = singleSelection();
	if (id.isEmpty() || !(videoFx(type) || !pluginTypeLabel(type).isEmpty()))
		return;
	QVector<Filter> fs = doc().filters(doc().instances[id]);
	fs << Filter{uid("flt_"), type, true, videoFxDefaults(type)};
	m_store.commit("Add " + filterLabel(type), [&](Doc &d) { d.setFilters(id, fs); });
	if (type.startsWith("obs:"))
		settleObsLists("filter", id, int(fs.size()) - 1);
}

void App::removeFilter(int index)
{
	if (!liveEditAllowed(m_selection))
		return;
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
	if (!liveEditAllowed(m_selection))
		return;
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
	if (!liveEditAllowed(m_selection))
		return;
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
	if (!liveEditAllowed(m_selection))
		return;
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
	if (!liveEditAllowed(m_selection))
		return;
	const QString id = singleSelection();
	Group g;
	if (id.isEmpty() || !groupFromName(group, g))
		return;
	const Instance &i = doc().instances[id];
	const Source src = doc().sources.value(i.sourceId);
	const PropMap def = doc().defaultProps(i.kind == "group" ? nullptr : &src);
	const bool wasLinked = i.links.contains(g);
	m_store.commit(wasLinked ? "Unlink and reset " + group : "Reset " + group, [&](Doc &d) {
		d.unlink(id, g);
		switch (g) {
		case Group::Transform: if (i.kind != "group") d.setTransform(id, def.transform); break;
		case Group::Crop: d.setCrop(id, {}); break;
		case Group::Filters: d.setFilters(id, {}); break;
		case Group::Visibility: d.setVisibility(id, {}); break;
		}
	});
	if (wasLinked)
		showToast("Unlinked " + group + " and reset it here only");
}

void App::unlinkGroup(const QString &group)
{
	if (!liveEditAllowed(m_selection))
		return;
	const QString id = singleSelection();
	for (Group g : kGroups)
		if (groupKey(g) == group && !id.isEmpty())
			m_store.commit("Unlink " + group, [&](Doc &d) { d.unlink(id, g); });
}

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
