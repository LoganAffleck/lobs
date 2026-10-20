#include "obs_import.h"
#include "audio_fx.h"

#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <optional>

namespace lobs {

namespace {

// OBS stores colors as 0xAABBGGRR.
QString abgrToHex(qint64 v)
{
	const uint n = uint(v);
	return QString("#%1%2%3").arg(n & 255, 2, 16, QChar('0')).arg((n >> 8) & 255, 2, 16, QChar('0')).arg((n >> 16) & 255, 2, 16, QChar('0'));
}

QString keyColor(const QJsonObject &s)
{
	const QString t = s["key_color_type"].toString("green");
	if (t == "blue") return "#0000ff";
	if (t == "magenta") return "#ff00ff";
	if (t == "custom") return abgrToHex(s["key_color"].toInteger(0x00ff00));
	return "#00ff00";
}

// LOBS kind + settings for an OBS input, or an empty kind if LOBS has no equivalent.
QString convertInput(const QString &id, const QJsonObject &s, QVariantMap &out)
{
	const auto dev = [&](const char *key) {
		if (const QString v = s[key].toString(); !v.isEmpty())
			out["device"] = v;
	};
	if (id == "dshow_input") return dev("video_device_id"), "camera";
	if (id == "monitor_capture") return dev("monitor_id"), "display";
	if (id == "window_capture") return dev("window"), "window";
	if (id == "game_capture") {
		out["mode"] = s["capture_mode"].toString("any_fullscreen");
		dev("window");
		return "game";
	}
	if (id == "image_source") return out["file"] = s["file"].toString(), "image";
	if (id == "ffmpeg_source") {
		out["file"] = s["local_file"].toString();
		out["loop"] = s["looping"].toBool();
		return "media";
	}
	if (id == "browser_source") {
		out["url"] = s["is_local_file"].toBool() ? s["local_file"].toString() : s["url"].toString();
		// OBS's defaults when the keys are absent; LOBS otherwise sizes pages to the canvas.
		out["width"] = s["width"].toInt(800);
		out["height"] = s["height"].toInt(600);
		if (s.contains("css"))
			out["css"] = s["css"].toString();
		return "browser";
	}
	if (id.startsWith("color_source")) return out["color"] = abgrToHex(s["color"].toInteger(0xFFFFFFFF)), "color";
	if (id.startsWith("text_gdiplus") || id.startsWith("text_ft2_source")) {
		const QJsonObject font = s["font"].toObject();
		out["text"] = s["text"].toString();
		out["font"] = font["face"].toString("Arial");
		out["size"] = font["size"].toInt(256);
		out["textColor"] = abgrToHex(s.contains("color") ? s["color"].toInteger() : 0xFFFFFF);
		out["bold"] = (font["flags"].toInt() & 1) != 0;
		out["italic"] = (font["flags"].toInt() & 2) != 0;
		return "text";
	}
	if (id == "wasapi_input_capture") return dev("device_id"), "mic";
	if (id == "wasapi_output_capture" || id == "wasapi_process_output_capture") return dev("device_id"), "desktop_audio";
	return {};
}

// LOBS filter for an OBS filter, or nullopt (audio filters, unsupported effects).
std::optional<Filter> convertFilter(const QString &id, const QJsonObject &s)
{
	Filter f{uid("flt_"), {}, true, {}};
	if (id.startsWith("color_filter")) {
		f.type = "color";
		f.settings = {{"brightness", s["brightness"].toDouble()}, {"contrast", s["contrast"].toDouble()}, {"saturation", s["saturation"].toDouble()},
			      {"gamma", s["gamma"].toDouble()}, {"hue", s["hue_shift"].toDouble()}};
	} else if (id.startsWith("chroma_key_filter")) {
		f.type = "chroma_key";
		f.settings = {{"key_color", keyColor(s)}, {"similarity", s["similarity"].toInt(400)}, {"smoothness", s["smoothness"].toInt(80)}};
	} else if (id.startsWith("sharpness_filter")) {
		f.type = "sharpness";
		f.settings = {{"amount", s["sharpness"].toDouble(0.08)}};
	} else if (id == "clut_filter") {
		f.type = "lut";
		f.settings = {{"path", s["image_path"].toString()}, {"amount", s["clut_amount"].toDouble(1)}};
	} else {
		return std::nullopt;
	}
	return f;
}

// Things the earlier OBS-sync prototype created inside OBS; never user content.
bool isLobsHelper(const QString &name, const QString &id) { return name.startsWith("[LOBS]") || name.endsWith("[LOBS]") || id == "lobs_instance"; }

} // namespace

bool isObsCollection(const QJsonObject &o) { return o.contains("sources") && o.contains("scene_order") && o["sources"].isArray(); }

ObsImport importObsCollection(const QJsonObject &o)
{
	ObsImport r;
	Doc &d = r.doc;
	if (const QJsonObject res = o["resolution"].toObject(); res["x"].toInt() > 0 && res["y"].toInt() > 0)
		d.canvasW = res["x"].toInt(), d.canvasH = res["y"].toInt();

	QHash<QString, QJsonObject> byUuid, byName;
	for (const QJsonValue v : o["sources"].toArray()) {
		const QJsonObject s = v.toObject();
		byName[s["name"].toString()] = s;
		if (s.contains("uuid"))
			byUuid[s["uuid"].toString()] = s;
	}
	for (const QJsonValue v : o["groups"].toArray()) {
		const QJsonObject s = v.toObject();
		byName[s["name"].toString()] = s;
		if (s.contains("uuid"))
			byUuid[s["uuid"].toString()] = s;
	}

	QHash<QString, QString> sourceFor;                  // OBS name -> LOBS source id
	QHash<QString, QVector<Filter>> filtersFor;         // LOBS source id -> filters
	QHash<QString, Crop> cropFilterFor;                 // LOBS source id -> crop from a Crop/Pad filter
	QHash<QString, QString> firstInstanceOf;            // LOBS source id -> first placed layer
	QSet<QString> noted;
	const auto note = [&](const QString &msg) {
		if (!noted.contains(msg))
			noted.insert(msg), r.skipped << msg;
	};

	// One pool source per OBS input, created on first use so unused helpers stay out.
	const auto sourceOf = [&](const QJsonObject &s) -> QString {
		const QString name = s["name"].toString(), id = s["id"].toString();
		if (auto it = sourceFor.find(name); it != sourceFor.end())
			return *it;
		QVariantMap settings;
		const QString kind = convertInput(id, s["settings"].toObject(), settings);
		if (kind.isEmpty()) {
			note(QString("%1 (%2): no LOBS equivalent yet").arg(name, id));
			return sourceFor[name] = QString();
		}
		Source &src = d.addSource(kind, name, settings);
		src.audio.volume = s["volume"].toDouble(1);
		src.audio.muted = s["muted"].toBool();
		src.audio.mono = (s["flags"].toInt() & 2) != 0;        // OBS_SOURCE_FLAG_FORCE_MONO
		src.audio.balance = s["balance"].toDouble(0.5);
		src.audio.syncMs = int(s["sync"].toInteger() / 1000000); // OBS stores ns
		const int mon = s["monitoring_type"].toInt();
		src.audio.monitor = mon == 1 ? "monitor_only" : mon == 2 ? "monitor_and_output" : "off";
		const int mixers = s["mixers"].toInt(src.hasAudio ? 1 : 0);
		for (int t = 0; t < src.audio.tracks.size(); ++t)
			src.audio.tracks[t] = (mixers >> t) & 1;
		QVector<Filter> fs;
		Crop cf;
		for (const QJsonValue fv : s["filters"].toArray()) {
			const QJsonObject f = fv.toObject();
			const QString fid = f["id"].toString();
			const QJsonObject fset = f["settings"].toObject();
			if (fid == "crop_filter") {
				if (fset["relative"].toBool(true))
					cf = {fset["left"].toInt(), fset["top"].toInt(), fset["right"].toInt(), fset["bottom"].toInt()};
				else
					note(QString("%1: absolute-size Crop/Pad filter (crop it in LOBS instead)").arg(name));
				continue;
			}
			if (auto lf = convertFilter(fid, fset)) {
				lf->enabled = f["enabled"].toBool(true);
				fs << *lf;
			} else if (!audioFxForObs(fid)) {
				note(QString("%1: %2 filter has no LOBS controls yet (kept as-is)").arg(name, f["name"].toString(fid)));
			}
		}
		filtersFor[src.id] = fs;
		// Audio effects stay on the source; settings keep OBS's keys.
		for (const QJsonValue fv : s["filters"].toArray()) {
			const QJsonObject f = fv.toObject();
			const QString fid = f["id"].toString();
			if (convertFilter(fid, {}) || fid == "crop_filter")
				continue; // a video filter, handled above
			QVariantMap fset = f["settings"].toObject().toVariantMap();
			QString type;
			if (const FxType *t = audioFxForObs(fid)) {
				type = t->type;
				QVariantMap merged = audioFxDefaults(type);
				for (auto it = fset.begin(); it != fset.end(); ++it)
					merged[it.key()] = *it;
				fset = merged;
			} else {
				type = "obs:" + fid; // kept and passed through; shown without controls
				fset["_label"] = f["name"].toString(fid);
			}
			src.audio.filters << Filter{uid("afx_"), type, f["enabled"].toBool(true), fset};
		}
		cropFilterFor[src.id] = cf;
		return sourceFor[name] = src.id;
	};

	// Scenes, in OBS's order.
	QHash<QString, QString> sceneIdFor; // OBS scene name -> LOBS scene id
	for (const QJsonValue v : o["scene_order"].toArray()) {
		const QString sceneName = v.toObject()["name"].toString();
		const QJsonObject sc = byName.value(sceneName);
		if (isLobsHelper(sceneName, {}) || sc.isEmpty())
			continue;
		QJsonArray items = sc["settings"].toObject()["items"].toArray();
		// Expand groups into their children; positions are composed with the group's.
		struct Item { QJsonObject it; double ox = 0, oy = 0, sx = 1, sy = 1; };
		QVector<Item> flat;
		for (const QJsonValue iv : items) {
			const QJsonObject it = iv.toObject();
			const QJsonObject src = it.contains("source_uuid") ? byUuid.value(it["source_uuid"].toString()) : byName.value(it["name"].toString());
			if (src["id"].toString() == "group") {
				const double gx = it["pos"].toObject()["x"].toDouble(), gy = it["pos"].toObject()["y"].toDouble();
				const double gsx = it["scale"].toObject()["x"].toDouble(1), gsy = it["scale"].toObject()["y"].toDouble(1);
				for (const QJsonValue cv : src["settings"].toObject()["items"].toArray())
					flat << Item{cv.toObject(), gx, gy, gsx, gsy};
				note(QString("Group \"%1\" in %2 was flattened into separate layers").arg(it["name"].toString(), sceneName));
			} else {
				flat << Item{it};
			}
		}
		// Skip scenes that only held LOBS sync helpers.
		const bool onlyHelpers = !flat.isEmpty() && std::all_of(flat.begin(), flat.end(), [&](const Item &x) {
			const QJsonObject src = byName.value(x.it["name"].toString());
			return isLobsHelper(x.it["name"].toString(), src["id"].toString());
		});
		if (onlyHelpers)
			continue;

		Scene &scene = d.addScene(sceneName);
		const QString sceneId = scene.id;
		sceneIdFor[sceneName] = sceneId;
		// OBS lists items bottom-first; placeSource puts each new layer on top.
		for (const Item &x : flat) {
			const QJsonObject &it = x.it;
			const QJsonObject src = it.contains("source_uuid") ? byUuid.value(it["source_uuid"].toString()) : byName.value(it["name"].toString());
			const QString srcName = src.isEmpty() ? it["name"].toString() : src["name"].toString();
			if (src.isEmpty() || isLobsHelper(srcName, src["id"].toString()))
				continue;
			if (src["id"].toString() == "scene") {
				note(QString("Scene \"%1\" nested inside %2 (nested scenes aren't supported yet)").arg(srcName, sceneName));
				continue;
			}
			const QString sid = sourceOf(src);
			if (sid.isEmpty())
				continue;
			if (!kindInfo(d.sources[sid].kind).video)
				continue; // audio inputs are pool-wide in LOBS: always in the mix, never placed
			Instance &inst = d.placeSource(sceneId, sid);
			inst.locked = it["locked"].toBool();
			const Crop cf = cropFilterFor.value(sid);
			inst.props.crop = {it["crop_left"].toInt() + cf.l, it["crop_top"].toInt() + cf.t, it["crop_right"].toInt() + cf.r, it["crop_bottom"].toInt() + cf.b};
			inst.props.visibility.visible = it["visible"].toBool(true);
			inst.props.filters = filtersFor.value(sid);
			inst.props.transform.rot = it["rot"].toDouble();

			ObsPlacement p;
			p.instId = inst.id;
			const QJsonObject pos = it["pos"].toObject(), scale = it["scale"].toObject(), bounds = it["bounds"].toObject();
			p.posX = x.ox + pos["x"].toDouble() * x.sx;
			p.posY = x.oy + pos["y"].toDouble() * x.sy;
			p.scaleX = scale["x"].toDouble(1) * x.sx;
			p.scaleY = scale["y"].toDouble(1) * x.sy;
			p.align = it["align"].toInt(5);
			p.boundsType = it["bounds_type"].toInt();
			p.boundsAlign = it["bounds_align"].toInt();
			p.boundsW = bounds["x"].toDouble() * x.sx;
			p.boundsH = bounds["y"].toDouble() * x.sy;
			// Captures are usually full-screen; other kinds have a typical size.
			const QString kind = d.sources[sid].kind;
			const KindInfo &ki = kindInfo(kind);
			const bool screenish = kind == "display" || kind == "game" || kind == "window" || ki.w <= 0;
			p.fallbackW = screenish ? d.canvasW : ki.w, p.fallbackH = screenish ? d.canvasH : ki.h;
			inst.props.transform = resolveObsPlacement(p, 0, 0, inst.props.crop);
			inst.props.transform.rot = it["rot"].toDouble();
			inst.props.transform.flipH = p.scaleX < 0; // OBS mirrors with a negative scale
			inst.props.transform.flipV = p.scaleY < 0;
			r.pending << p;

			// OBS filters belong to the source, so every placement shares them: link them.
			if (const QString first = firstInstanceOf.value(sid); first.isEmpty())
				firstInstanceOf[sid] = inst.id;
			else if (!inst.props.filters.isEmpty())
				d.linkTo(inst.id, first, Group::Filters, false);
		}
	}

	// Global audio devices (Settings > Audio in OBS) become pool sources too.
	const auto globalAudio = [&](const char *key) {
		const QJsonObject s = o[key].toObject();
		if (!s.isEmpty() && !isLobsHelper(s["name"].toString(), s["id"].toString()))
			sourceOf(s);
	};
	for (const char *k : {"DesktopAudioDevice1", "DesktopAudioDevice2", "AuxAudioDevice1", "AuxAudioDevice2", "AuxAudioDevice3", "AuxAudioDevice4"})
		globalAudio(k);
	// Audio inputs that only appear as scene items.
	for (const QJsonValue v : o["sources"].toArray()) {
		const QJsonObject s = v.toObject();
		QVariantMap tmp;
		const QString kind = convertInput(s["id"].toString(), s["settings"].toObject(), tmp);
		if ((kind == "mic" || kind == "desktop_audio") && !isLobsHelper(s["name"].toString(), s["id"].toString())) {
			bool used = false;
			for (const QString &sc : sceneIdFor.keys())
				for (const QJsonValue iv : byName.value(sc)["settings"].toObject()["items"].toArray())
					used |= iv.toObject()["name"].toString() == s["name"].toString();
			if (used)
				sourceOf(s);
		}
	}

	r.programScene = sceneIdFor.value(o["current_program_scene"].toString(o["current_scene"].toString()));
	if (r.programScene.isEmpty() && !d.sceneOrder.isEmpty())
		r.programScene = d.sceneOrder.first();
	return r;
}

Transform resolveObsPlacement(const ObsPlacement &p, int naturalW, int naturalH, const Crop &c)
{
	// Content size before any bounds: the cropped source, scaled.
	const double cw = std::max(0, naturalW - c.l - c.r), ch = std::max(0, naturalH - c.t - c.b);
	const bool known = cw > 0 && ch > 0;
	const auto alignFactor = [](int align, bool horizontal) {
		if (horizontal) return (align & 1) ? 0.0 : (align & 2) ? 1.0 : 0.5;
		return (align & 4) ? 0.0 : (align & 8) ? 1.0 : 0.5;
	};
	Transform t;
	if (p.boundsType == 0) {
		// Unknown size: a placeholder until the engine reports it (the caller re-resolves).
		const double nw = known ? cw : std::max(1, p.fallbackW - c.l - c.r), nh = known ? ch : std::max(1, p.fallbackH - c.t - c.b);
		const double w = nw * std::abs(p.scaleX), h = nh * std::abs(p.scaleY);
		t.w = w, t.h = h;
		// A negative scale mirrors the layer around `pos`, so its box extends the other way.
		const double ax = alignFactor(p.align, true), ay = alignFactor(p.align, false);
		t.x = p.posX - (p.scaleX < 0 ? 1 - ax : ax) * w;
		t.y = p.posY - (p.scaleY < 0 ? 1 - ay : ay) * h;
	} else {
		// The bounding box sits at pos by `align`; the content is fitted inside it by `boundsAlign`.
		const double bw = p.boundsW, bh = p.boundsH;
		const double bx = p.posX - alignFactor(p.align, true) * bw, by = p.posY - alignFactor(p.align, false) * bh;
		double w = bw, h = bh;
		if (known && p.boundsType != 1) {
			const double sx = bw / cw, sy = bh / ch;
			double k = 1;
			switch (p.boundsType) {
			case 2: k = std::min(sx, sy); break;                   // scale inner
			case 3: k = std::max(sx, sy); break;                   // scale outer
			case 4: k = sx; break;                                 // to width
			case 5: k = sy; break;                                 // to height
			case 6: k = std::min({1.0, sx, sy}); break;            // max size only
			}
			w = cw * k, h = ch * k;
		}
		t.w = w, t.h = h;
		t.x = bx + alignFactor(p.boundsAlign, true) * (bw - w);
		t.y = by + alignFactor(p.boundsAlign, false) * (bh - h);
	}
	t.x = std::round(t.x), t.y = std::round(t.y), t.w = std::round(t.w), t.h = std::round(t.h);
	return t;
}

} // namespace lobs
