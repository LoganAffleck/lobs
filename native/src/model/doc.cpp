#include "doc.h"

#include <QJsonArray>
#include <QRandomGenerator>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <functional>

namespace lobs {

QString uid(const QString &prefix)
{
	static const char *abc = "abcdefghijklmnopqrstuvwxyz0123456789";
	QString s = prefix;
	for (int i = 0; i < 8; i++)
		s += QChar(abc[QRandomGenerator::global()->bounded(36)]);
	return s;
}

QString uniqueName(const QString &base, const QStringList &taken)
{
	if (!taken.contains(base))
		return base;
	int i = 2;
	while (taken.contains(QString("%1 %2").arg(base).arg(i)))
		i++;
	return QString("%1 %2").arg(base).arg(i);
}

QString groupKey(Group g)
{
	switch (g) {
	case Group::Transform: return "transform";
	case Group::Crop: return "crop";
	case Group::Filters: return "filters";
	case Group::Visibility: return "visibility";
	}
	return {};
}

static Group groupFromKey(const QString &k)
{
	for (Group g : kGroups)
		if (groupKey(g) == k)
			return g;
	return Group::Transform;
}

const KindInfo &kindInfo(const QString &kind)
{
	static const QHash<QString, KindInfo> info = {
		{"camera", {"Camera", "video", true, false, 1280, 720}},
		{"display", {"Display Capture", "monitor", true, false, 0, 0}},
		{"window", {"Window Capture", "app-window", true, false, 0, 0}},
		{"game", {"Game Capture", "gamepad-2", true, false, 0, 0}},
		{"image", {"Image", "image", true, false, 800, 450}},
		{"media", {"Media File", "film", true, true, 1280, 720}},
		{"browser", {"Browser", "globe", true, true, 1920, 1080}},
		{"color", {"Color", "palette", true, false, 0, 0}},
		{"text", {"Text", "type", true, false, 600, 120}},
		{"shape", {"Shape", "square", true, false, 400, 240}},
		{"mic", {"Microphone", "mic", false, true, -1, -1}},
		{"desktop_audio", {"Desktop Audio", "volume-2", false, true, -1, -1}},
		{"scene", {"Scene", "clapperboard", true, false, 0, 0}}, // another scene, shown whole (settings: scene id)
	};
	static const KindInfo unknown{"Unknown", "?", true, false, 640, 360};
	static const KindInfo missingPlugin{"Plugin source (not loaded)", "puzzle", true, false, 0, 0};
	auto it = info.find(kind);
	if (it != info.end())
		return *it;
	if (kind.startsWith("obs:")) {
		const auto p = pluginKinds().constFind(kind);
		return p == pluginKinds().constEnd() ? missingPlugin : *p;
	}
	return unknown;
}

QHash<QString, KindInfo> &pluginKinds()
{
	static QHash<QString, KindInfo> kinds;
	return kinds;
}

QStringList allKinds()
{
	return {"camera", "display", "window", "game", "image", "media", "browser", "color", "text", "mic", "desktop_audio"};
}

static const char *kTags[] = {"#e5484d", "#f76b15", "#ffc53d", "#46a758", "#12a594", "#0090ff", "#8e4ec6", "#d6409f"};

// ---------- creation ----------

Source &Doc::addSource(const QString &kind, const QString &name, const QVariantMap &settings)
{
	Source s;
	s.id = uid("src_");
	QStringList taken;
	for (const auto &x : sources)
		taken << x.name;
	s.name = uniqueName(name, taken);
	s.kind = kind;
	s.settings = settings;
	s.color = kTags[sourceOrder.size() % 8];
	s.hasAudio = kindInfo(kind).audio;
	sourceOrder << s.id;
	return sources[s.id] = s;
}

Scene &Doc::addScene(const QString &name)
{
	Scene s;
	s.id = uid("scn_");
	QStringList taken;
	for (const auto &x : scenes)
		taken << x.name;
	s.name = uniqueName(name, taken);
	sceneOrder << s.id;
	return scenes[s.id] = s;
}

QString Doc::verticalLayout(const QString &sceneId) const
{
	for (const Scene &s : scenes)
		if (s.verticalOf == sceneId)
			return s.id;
	return {};
}

QString Doc::mainScene(const QString &sceneId) const
{
	const QString of = scenes.value(sceneId).verticalOf;
	return of.isEmpty() ? sceneId : of;
}

QSize Doc::canvasOf(const QString &sceneId) const
{
	return scenes.value(sceneId).verticalOf.isEmpty() ? QSize(canvasW, canvasH) : QSize(verticalW, verticalH);
}

PropMap Doc::defaultProps(const Source *src) const
{
	const KindInfo &k = src ? kindInfo(src->kind) : kindInfo("display");
	const double w = k.w > 0 ? k.w : canvasW, h = k.h > 0 ? k.h : canvasH;
	PropMap p;
	p.transform = {std::round((canvasW - w) / 2), std::round((canvasH - h) / 2), w, h, 0};
	return p;
}

Instance &Doc::placeSource(const QString &sceneId, const QString &sourceId)
{
	const Source &src = sources[sourceId];
	Instance i;
	i.id = uid("ins_");
	i.sceneId = sceneId;
	i.sourceId = sourceId;
	i.name = src.name;
	i.props = defaultProps(&src);
	// In a vertical layout, the default box (made for the main canvas) is fitted into the tall one.
	if (const QSize c = canvasOf(sceneId); c != QSize(canvasW, canvasH)) {
		Transform &t = i.props.transform;
		const double k = std::min(c.width() / std::max(1.0, t.w), c.height() / std::max(1.0, t.h));
		t.w = std::round(t.w * std::min(1.0, k)), t.h = std::round(t.h * std::min(1.0, k));
		t.x = std::round((c.width() - t.w) / 2), t.y = std::round((c.height() - t.h) / 2);
	}
	scenes[sceneId].layers.prepend(i.id);
	return instances[i.id] = i;
}

// ---------- linked property access ----------

const PropMap &Doc::propsFor(const Instance &i, Group g) const
{
	auto l = i.links.find(g);
	if (l != i.links.end()) {
		auto it = links.find(*l);
		if (it != links.end())
			return it->value;
	}
	return i.props;
}

PropMap *Doc::propsFor(const QString &instId, Group g)
{
	Instance &i = instances[instId];
	auto l = i.links.find(g);
	if (l != i.links.end() && links.contains(*l))
		return &links[*l].value;
	return &i.props;
}

Transform Doc::transform(const Instance &i) const { return propsFor(i, Group::Transform).transform; }
Crop Doc::crop(const Instance &i) const { return propsFor(i, Group::Crop).crop; }
QVector<Filter> Doc::filters(const Instance &i) const { return propsFor(i, Group::Filters).filters; }
Visibility Doc::visibility(const Instance &i) const { return propsFor(i, Group::Visibility).visibility; }
void Doc::setTransform(const QString &id, const Transform &v) { propsFor(id, Group::Transform)->transform = v; }
void Doc::setCrop(const QString &id, const Crop &v) { propsFor(id, Group::Crop)->crop = v; }
void Doc::setFilters(const QString &id, const QVector<Filter> &v) { propsFor(id, Group::Filters)->filters = v; }
void Doc::setVisibility(const QString &id, const Visibility &v) { propsFor(id, Group::Visibility)->visibility = v; }

static void copyGroup(PropMap &dst, const PropMap &src, Group g)
{
	switch (g) {
	case Group::Transform: dst.transform = src.transform; break;
	case Group::Crop: dst.crop = src.crop; break;
	case Group::Filters: dst.filters = src.filters; break;
	case Group::Visibility: dst.visibility = src.visibility; break;
	}
}

QList<const Instance *> Doc::linkedPeers(const Instance &i, Group g) const
{
	QList<const Instance *> out;
	auto l = i.links.find(g);
	if (l == i.links.end())
		return out;
	for (const auto &o : instances)
		if (o.id != i.id && o.links.value(g) == *l)
			out << &o;
	return out;
}

void Doc::unlink(const QString &instId, Group g)
{
	Instance &i = instances[instId];
	if (!i.links.contains(g))
		return;
	copyGroup(i.props, propsFor(i, g), g);
	i.links.remove(g);
	gcLinks();
}

void Doc::linkTo(const QString &instId, const QString &targetId, Group g, bool keepMine)
{
	const PropMap winner = keepMine ? propsFor(instances[instId], g) : propsFor(instances[targetId], g);
	Instance &target = instances[targetId];
	QString linkId = target.links.value(g);
	if (linkId.isEmpty()) {
		linkId = uid("lnk_");
		links[linkId] = Link{linkId, g, target.props};
		target.links[g] = linkId;
	}
	Instance &inst = instances[instId];
	const QString old = inst.links.value(g);
	if (!old.isEmpty() && old != linkId)
		for (auto &o : instances)
			if (o.links.value(g) == old)
				o.links[g] = linkId;
	inst.links[g] = linkId;
	copyGroup(links[linkId].value, winner, g);
	gcLinks();
}

int Doc::repair()
{
	int dropped = 0;
	auto keep = [&](QStringList &ids, auto exists) {
		const qsizetype n = ids.size();
		ids.erase(std::remove_if(ids.begin(), ids.end(), [&](const QString &id) { return !exists(id); }), ids.end());
		dropped += int(n - ids.size());
	};
	keep(sceneOrder, [&](const QString &id) { return scenes.contains(id); });
	keep(sourceOrder, [&](const QString &id) { return sources.contains(id); });
	keep(mixerOrder, [&](const QString &id) { return sources.contains(id); });
	keep(mixerPinned, [&](const QString &id) { return sources.contains(id); });
	for (Scene &s : scenes)
		keep(s.layers, [&](const QString &id) { return instances.contains(id); });
	for (Instance &i : instances) {
		keep(i.children, [&](const QString &id) { return instances.contains(id); });
		for (auto it = i.links.begin(); it != i.links.end();)
			if (!links.contains(*it))
				it = i.links.erase(it), ++dropped;
			else
				++it;
	}
	if (dropped)
		gcLinks();
	return dropped;
}

void Doc::gcLinks()
{
	QHash<QString, QList<QPair<QString, Group>>> users;
	for (const auto &i : instances)
		for (auto it = i.links.begin(); it != i.links.end(); ++it)
			users[*it] << qMakePair(i.id, it.key());
	for (const QString &id : links.keys()) {
		const auto u = users.value(id);
		if (u.size() <= 1) {
			// A link with one member is just a local value.
			for (const auto &[iid, g] : u) {
				copyGroup(instances[iid].props, links[id].value, g);
				instances[iid].links.remove(g);
			}
			links.remove(id);
		}
	}
}

// ---------- tree ----------

const Instance *Doc::parentOf(const QString &id) const
{
	for (const auto &i : instances)
		if (i.children.contains(id))
			return &i;
	return nullptr;
}

QStringList &Doc::siblings(const QString &id)
{
	if (const Instance *p = parentOf(id))
		return instances[p->id].children;
	return scenes[instances[id].sceneId].layers;
}

QStringList Doc::descendants(const QString &id) const
{
	QStringList out{id};
	const Instance &i = instances[id];
	if (i.kind == "group")
		for (const QString &c : i.children)
			out << descendants(c);
	return out;
}

QVector<Doc::Leaf> Doc::flatten(const QString &sceneId) const
{
	QVector<Leaf> out;
	std::function<void(const QStringList &, bool, double)> walk = [&](const QStringList &ids, bool vis, double op) {
		for (int k = ids.size() - 1; k >= 0; k--) {
			auto it = instances.find(ids[k]);
			if (it == instances.end())
				continue;
			const Visibility v = visibility(*it);
			if (it->kind == "group")
				walk(it->children, vis && v.visible, op * v.opacity);
			else
				out.push_back({it->id, vis && v.visible, op * v.opacity});
		}
	};
	walk(scenes.value(sceneId).layers, true, 1.0);
	return out;
}

Transform Doc::bounds(const QString &id) const
{
	const Instance &i = instances[id];
	if (i.kind != "group")
		return transform(i);
	double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
	for (const QString &d : descendants(id)) {
		const Instance &l = instances[d];
		if (l.kind == "group")
			continue;
		const Transform t = transform(l);
		x0 = std::min(x0, t.x), y0 = std::min(y0, t.y), x1 = std::max(x1, t.x + t.w), y1 = std::max(y1, t.y + t.h);
	}
	if (x0 > x1)
		return {};
	return {x0, y0, x1 - x0, y1 - y0, 0};
}

void Doc::moveBy(const QString &id, double dx, double dy, QSet<QString> &seen)
{
	for (const QString &d : descendants(id)) {
		const Instance &l = instances[d];
		if (l.kind == "group")
			continue;
		const QString key = l.links.value(Group::Transform, l.id); // don't double-move linked transforms
		if (seen.contains(key))
			continue;
		seen.insert(key);
		Transform t = transform(l);
		t.x = std::round(t.x + dx);
		t.y = std::round(t.y + dy);
		setTransform(d, t);
	}
}

// ---------- edits ----------

void Doc::deleteInstance(const QString &id)
{
	if (!instances.contains(id))
		return;
	siblings(id).removeAll(id);
	for (const QString &d : descendants(id))
		instances.remove(d);
	gcLinks();
}

void Doc::deleteSource(const QString &id)
{
	for (const QString &iid : instances.keys())
		if (instances.contains(iid) && instances[iid].sourceId == id)
			deleteInstance(iid);
	sources.remove(id);
	sourceOrder.removeAll(id);
}

QString Doc::addVerticalLayout(const QString &sceneId)
{
	if (const QString v = verticalLayout(sceneId); !v.isEmpty())
		return v;
	const Scene &main = scenes[sceneId];
	Scene v;
	v.id = uid("scn_");
	v.name = main.name + " (Vertical)";
	v.verticalOf = sceneId;
	// The horizontal picture scaled to the vertical canvas's width and centered: a start to adjust.
	const double s = double(verticalW) / canvasW, dy = (verticalH - canvasH * s) / 2;
	for (const QString &l : main.layers) {
		const Instance &src = instances[l];
		if (src.kind == "group")
			continue; // groups come back as their layers when edited; v1 copies plain layers
		Instance in = src;
		in.id = uid("ins_");
		in.sceneId = v.id;
		in.links.clear(); // its own layout
		Transform t = transform(src);
		t.x = std::round(t.x * s), t.y = std::round(t.y * s + dy), t.w = std::round(t.w * s), t.h = std::round(t.h * s);
		in.props = {t, crop(src), filters(src), visibility(src)}; // what it shows now (links resolved)
		instances[in.id] = in;
		v.layers << in.id;
	}
	scenes[v.id] = v; // not in sceneOrder: it isn't listed
	return v.id;
}

void Doc::deleteScene(const QString &id)
{
	if (const QString v = verticalLayout(id); !v.isEmpty())
		deleteScene(v); // its vertical layout goes with it
	// Sources that showed this scene inside others go with it.
	for (const QString &sid : QStringList(sourceOrder))
		if (sources[sid].kind == "scene" && sources[sid].settings.value("scene").toString() == id)
			deleteSource(sid);
	for (const QString &l : QStringList(scenes[id].layers))
		deleteInstance(l);
	scenes.remove(id);
	sceneOrder.removeAll(id);
}

bool Doc::sceneReaches(const QString &from, const QString &to) const
{
	// Through the scene sources placed in `from` (in groups too), and theirs, and so on.
	QSet<QString> seen;
	std::function<bool(const QString &)> walk = [&](const QString &sceneId) {
		if (sceneId == to)
			return true;
		if (seen.contains(sceneId) || !scenes.contains(sceneId))
			return false;
		seen.insert(sceneId);
		for (const auto &i : instances) {
			if (i.sceneId != sceneId || i.kind == "group")
				continue;
			const auto s = sources.constFind(i.sourceId);
			if (s != sources.constEnd() && s->kind == "scene" && walk(s->settings.value("scene").toString()))
				return true;
		}
		return false;
	};
	return walk(from);
}

void Doc::moveLayer(const QString &id, int delta)
{
	QStringList &arr = siblings(id);
	const int from = arr.indexOf(id);
	const int to = std::clamp(from + delta, 0, int(arr.size()) - 1);
	if (from >= 0 && to != from)
		arr.move(from, to);
}

QStringList Doc::copyToScene(const QStringList &ids, const QString &sceneId, bool linked)
{
	std::function<QString(const QString &)> copy = [&](const QString &id) -> QString {
		Instance n = instances[id];
		const Instance src = n;
		n.id = uid("ins_");
		n.sceneId = sceneId;
		if (src.kind == "group") {
			QStringList kids;
			for (const QString &c : src.children)
				kids << copy(c);
			n.children = kids;
		}
		if (!linked) { // a linked group links its own properties too, like any layer (LOBS-011)
			for (Group g : kGroups)
				copyGroup(n.props, propsFor(src, g), g);
			n.links.clear();
			instances[n.id] = n;
			return n.id;
		}
		instances[n.id] = n;
		for (Group g : kGroups)
			linkTo(n.id, src.id, g, false);
		return n.id;
	};
	QStringList made;
	for (int k = ids.size() - 1; k >= 0; k--) {
		const QString nid = copy(ids[k]);
		scenes[sceneId].layers.prepend(nid);
		made.prepend(nid);
	}
	return made;
}

QString Doc::groupLayers(const QStringList &ids)
{
	if (ids.isEmpty())
		return {};
	QStringList &arr = siblings(ids.first());
	QStringList members;
	for (const QString &id : arr)
		if (ids.contains(id))
			members << id;
	if (members.isEmpty())
		return {};
	Instance g;
	g.id = uid("ins_");
	g.kind = "group";
	g.sceneId = instances[members.first()].sceneId;
	QStringList taken;
	for (const auto &i : instances)
		taken << i.name;
	g.name = uniqueName("Group", taken);
	g.children = members;
	g.props = defaultProps(nullptr);
	const int at = arr.indexOf(members.first());
	for (const QString &m : members)
		arr.removeAll(m);
	arr.insert(at, g.id);
	instances[g.id] = g;
	return g.id;
}

void Doc::ungroup(const QString &groupId)
{
	const Instance g = instances[groupId];
	if (g.kind != "group")
		return;
	QStringList &arr = siblings(groupId);
	const int at = arr.indexOf(groupId);
	arr.removeAt(at);
	for (int k = 0; k < g.children.size(); k++)
		arr.insert(at + k, g.children[k]);
	instances.remove(groupId);
	gcLinks();
}

// ---------- json (same shape as the web prototype) ----------

static QJsonObject propsJson(const PropMap &p)
{
	QJsonArray fs;
	for (const Filter &f : p.filters)
		fs << QJsonObject{{"id", f.id}, {"type", f.type}, {"enabled", f.enabled}, {"settings", QJsonObject::fromVariantMap(f.settings)}};
	return QJsonObject{
		{"transform", QJsonObject{{"x", p.transform.x}, {"y", p.transform.y}, {"w", p.transform.w}, {"h", p.transform.h}, {"rot", p.transform.rot},
				   {"flipH", p.transform.flipH}, {"flipV", p.transform.flipV}}},
		{"crop", QJsonObject{{"l", p.crop.l}, {"t", p.crop.t}, {"r", p.crop.r}, {"b", p.crop.b}, {"radius", p.crop.radius}}},
		{"filters", fs},
		{"visibility", QJsonObject{{"visible", p.visibility.visible}, {"opacity", p.visibility.opacity}}},
	};
}

static PropMap propsFrom(const QJsonObject &o)
{
	PropMap p;
	const QJsonObject t = o["transform"].toObject(), c = o["crop"].toObject(), v = o["visibility"].toObject();
	p.transform = {t["x"].toDouble(), t["y"].toDouble(), t["w"].toDouble(), t["h"].toDouble(), t["rot"].toDouble(), t["flipH"].toBool(), t["flipV"].toBool()};
	p.crop = {c["l"].toInt(), c["t"].toInt(), c["r"].toInt(), c["b"].toInt(), c["radius"].toDouble()};
	for (const auto &fv : o["filters"].toArray()) {
		const QJsonObject f = fv.toObject();
		p.filters << Filter{f["id"].toString(), f["type"].toString(), f["enabled"].toBool(true), f["settings"].toObject().toVariantMap()};
	}
	p.visibility = {v["visible"].toBool(true), v["opacity"].toDouble(1)};
	return p;
}

QJsonObject Doc::toJson() const
{
	QJsonObject srcs, scns, insts, lnks;
	for (const auto &s : sources) {
		QJsonObject o{{"id", s.id}, {"name", s.name}, {"kind", s.kind}, {"bin", s.bin}, {"color", s.color}, {"settings", QJsonObject::fromVariantMap(s.settings)}};
		if (s.hasAudio) {
			QJsonArray tr;
			for (bool b : s.audio.tracks)
				tr << b;
			QJsonArray afx;
			for (const Filter &f : s.audio.filters)
				afx << QJsonObject{{"id", f.id}, {"type", f.type}, {"enabled", f.enabled}, {"settings", QJsonObject::fromVariantMap(f.settings)}};
			o["audio"] = QJsonObject{{"volume", s.audio.volume}, {"muted", s.audio.muted}, {"monitor", s.audio.monitor}, {"tracks", tr}, {"filters", afx},
				       {"mono", s.audio.mono}, {"balance", s.audio.balance}, {"syncMs", s.audio.syncMs}};
		}
		srcs[s.id] = o;
	}
	for (const auto &s : scenes)
		{
			QJsonObject sc{{"id", s.id}, {"name", s.name}, {"folder", s.folder}, {"favorite", s.favorite}, {"layers", QJsonArray::fromStringList(s.layers)}};
			if (!s.verticalOf.isEmpty())
				sc["verticalOf"] = s.verticalOf;
			scns[s.id] = sc;
		}
	for (const auto &i : instances) {
		QJsonObject l;
		for (auto it = i.links.begin(); it != i.links.end(); ++it)
			l[groupKey(it.key())] = *it;
		QJsonObject o{{"id", i.id}, {"sceneId", i.sceneId}, {"kind", i.kind}, {"name", i.name}, {"locked", i.locked}, {"props", propsJson(i.props)}, {"links", l}};
		if (i.kind == "group")
			o["children"] = QJsonArray::fromStringList(i.children);
		else
			o["sourceId"] = i.sourceId;
		insts[i.id] = o;
	}
	for (const auto &k : links)
		lnks[k.id] = QJsonObject{{"id", k.id}, {"group", groupKey(k.group)}, {"value", propsJson(k.value)[groupKey(k.group)]}};
	return QJsonObject{{"version", 1}, {"canvas", QJsonObject{{"w", canvasW}, {"h", canvasH}}}, {"vertical", QJsonObject{{"on", verticalOn}, {"w", verticalW}, {"h", verticalH}}}, {"sources", srcs}, {"sourceOrder", QJsonArray::fromStringList(sourceOrder)},
			   {"scenes", scns}, {"sceneOrder", QJsonArray::fromStringList(sceneOrder)}, {"instances", insts}, {"links", lnks}, {"bins", QJsonArray::fromStringList(bins)}, {"mixerOrder", QJsonArray::fromStringList(mixerOrder)}, {"backgroundsLocked", backgroundsLocked}, {"mixerPinned", QJsonArray::fromStringList(mixerPinned)}, {"swatches", QJsonArray::fromStringList(swatches)},
			   {"transitions", [&] {
				    QJsonArray a;
				    for (const Transition &t : transitions)
					    a << QJsonObject{{"id", t.id}, {"name", t.name}, {"type", t.type}, {"durationMs", t.durationMs}, {"settings", QJsonObject::fromVariantMap(t.settings)}};
				    return a;
			    }()},
			   {"currentTransition", currentTransition}};
}

static QStringList strings(const QJsonValue &v)
{
	QStringList out;
	for (const auto &x : v.toArray())
		out << x.toString();
	return out;
}

Doc Doc::fromJson(const QJsonObject &o)
{
	Doc d;
	d.canvasW = o["canvas"].toObject()["w"].toInt(1920);
	d.canvasH = o["canvas"].toObject()["h"].toInt(1080);
	const QJsonObject vo = o["vertical"].toObject();
	d.verticalOn = vo["on"].toBool(false);
	d.verticalW = vo["w"].toInt(1080);
	d.verticalH = vo["h"].toInt(1920);
	if (o.contains("swatches")) // older shows keep the default palette
		d.swatches = strings(o["swatches"]);
	if (o.contains("transitions")) { // older shows get Fade and Cut
		d.transitions.clear();
		for (const auto &v : o["transitions"].toArray()) {
			const QJsonObject t = v.toObject();
			d.transitions.push_back({t["id"].toString(), t["name"].toString(), t["type"].toString(), t["durationMs"].toInt(300), t["settings"].toObject().toVariantMap()});
		}
		d.currentTransition = o["currentTransition"].toString(d.transitions.isEmpty() ? QString() : d.transitions.first().id);
	}
	for (const auto &v : o["sources"].toObject()) {
		const QJsonObject s = v.toObject();
		Source x{s["id"].toString(), s["name"].toString(), s["kind"].toString(), s["bin"].toString(), s["color"].toString(), s["settings"].toObject().toVariantMap()};
		if (s.contains("audio")) {
			const QJsonObject a = s["audio"].toObject();
			x.hasAudio = true;
			x.audio.volume = a["volume"].toDouble(1);
			x.audio.muted = a["muted"].toBool();
			x.audio.monitor = a["monitor"].toString("off");
			x.audio.mono = a["mono"].toBool();
			x.audio.balance = a["balance"].toDouble(0.5);
			x.audio.syncMs = a["syncMs"].toInt();
			x.audio.tracks.clear();
			for (const auto &t : a["tracks"].toArray())
				x.audio.tracks << t.toBool();
			for (const auto &fv : a["filters"].toArray()) {
				const QJsonObject f = fv.toObject();
				x.audio.filters << Filter{f["id"].toString(), f["type"].toString(), f["enabled"].toBool(true), f["settings"].toObject().toVariantMap()};
			}
		}
		d.sources[x.id] = x;
	}
	d.sourceOrder = strings(o["sourceOrder"]);
	for (const auto &v : o["scenes"].toObject()) {
		const QJsonObject s = v.toObject();
		d.scenes[s["id"].toString()] = Scene{s["id"].toString(), s["name"].toString(), s["folder"].toString(), s["favorite"].toBool(), strings(s["layers"]), s["verticalOf"].toString()};
	}
	d.sceneOrder = strings(o["sceneOrder"]);
	for (const auto &v : o["instances"].toObject()) {
		const QJsonObject s = v.toObject();
		Instance i;
		i.id = s["id"].toString();
		i.sceneId = s["sceneId"].toString();
		i.kind = s["kind"].toString("source");
		i.sourceId = s["sourceId"].toString();
		i.name = s["name"].toString();
		i.locked = s["locked"].toBool();
		i.children = strings(s["children"]);
		i.props = propsFrom(s["props"].toObject());
		const QJsonObject l = s["links"].toObject();
		for (auto it = l.begin(); it != l.end(); ++it)
			i.links[groupFromKey(it.key())] = it->toString();
		d.instances[i.id] = i;
	}
	for (const auto &v : o["links"].toObject()) {
		const QJsonObject s = v.toObject();
		Link k;
		k.id = s["id"].toString();
		k.group = groupFromKey(s["group"].toString());
		QJsonObject wrapper;
		wrapper[groupKey(k.group)] = s["value"];
		PropMap p = propsFrom(wrapper);
		copyGroup(k.value, p, k.group);
		d.links[k.id] = k;
	}
	d.bins = strings(o["bins"]);
	d.mixerOrder = strings(o["mixerOrder"]);
	d.mixerPinned = strings(o["mixerPinned"]);
	if (!o["backgroundsLocked"].toBool()) { // saved before backgrounds started locked: lock them, once
		for (Instance &i : d.instances)
			if (i.kind != "group" && d.sources.value(i.sourceId).settings.value("background").toBool())
				i.locked = true;
	}
	d.backgroundsLocked = true;
	return d;
}

Doc Doc::sample()
{
	Doc d;
	Source &cam = d.addSource("camera", "Facecam");
	const QString camId = cam.id;
	const QString dispId = d.addSource("display", "Display 1").id;
	const QString bgId = d.addSource("color", "Background", {{"color", "#1d1b2e"}}).id;
	const QString titleId = d.addSource("text", "Title", {{"text", "Starting soon"}, {"size", 96}}).id;
	d.addSource("mic", "Mic");
	d.addSource("desktop_audio", "Desktop Audio");
	d.bins = {"Overlays"};
	d.sources[bgId].bin = d.sources[titleId].bin = "Overlays";

	const QString start = d.addScene("Starting Soon").id;
	d.placeSource(start, bgId);
	Instance &t = d.placeSource(start, titleId);
	t.props.transform = {660, 480, 600, 120, 0};

	const QString chat = d.addScene("Just Chatting").id;
	d.placeSource(chat, dispId);
	Instance &c1 = d.placeSource(chat, camId);
	c1.props.transform = {1200, 560, 640, 360, 0};
	c1.props.filters = {Filter{uid("flt_"), "color", true, {{"brightness", 0.0}, {"contrast", 0.05}, {"saturation", 0.25}, {"gamma", 0.0}, {"hue", 0.0}}}};
	const QString c1id = c1.id;

	const QString play = d.addScene("Gameplay").id;
	d.placeSource(play, bgId);
	Instance &c2 = d.placeSource(play, camId);
	c2.props.transform = {1500, 40, 384, 384, 0};
	c2.props.crop = {560, 140, 560, 140};
	d.linkTo(c2.id, c1id, Group::Filters, false); // same grade everywhere, different framing

	d.addScene("BRB");
	return d;
}

} // namespace lobs
