// LOBS document model (C++ port of app/src/model). Plain value types; no libobs or UI.
// The JSON format matches the web prototype's .lobs.json.
#pragma once

#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace lobs {

struct Transform {
	double x = 0, y = 0, w = 0, h = 0, rot = 0;
	bool flipH = false, flipV = false; // mirrored within the layer's box
	bool operator==(const Transform &) const = default;
};
struct Crop {
	int l = 0, t = 0, r = 0, b = 0;
	double radius = 0; // rounded corners, % of the layer's shorter side (50 = circle / pill)
	bool operator==(const Crop &) const = default;
};
struct Filter {
	QString id, type;
	bool enabled = true;
	QVariantMap settings;
	bool operator==(const Filter &) const = default;
};
struct Visibility {
	bool visible = true;
	double opacity = 1;
	bool operator==(const Visibility &) const = default;
};

enum class Group { Transform, Crop, Filters, Visibility };
inline const Group kGroups[] = {Group::Transform, Group::Crop, Group::Filters, Group::Visibility};
QString groupKey(Group g);
inline size_t qHash(Group g, size_t seed = 0) { return ::qHash(int(g), seed); }

struct PropMap {
	Transform transform;
	Crop crop;
	QVector<Filter> filters;
	Visibility visibility;
	bool operator==(const PropMap &) const = default;
};

struct AudioSettings {
	double volume = 1;
	bool muted = false;
	QString monitor = "off"; // off | monitor_only | monitor_and_output
	QVector<bool> tracks = {true, false, false, false, false, false};
	QVector<Filter> filters; // audio effects, in processing order (see audio_fx.h)
	bool mono = false;       // downmix to mono (e.g. a mic that only comes in on one side)
	double balance = 0.5;    // 0 = left, 0.5 = center, 1 = right
	int syncMs = 0;          // delay against video, ms (OBS's Sync Offset)
	bool operator==(const AudioSettings &) const = default;
};

// A scene transition (how Show switches scenes). type is the kind (cut, fade, swipe, slide,
// fade_to_color, luma_wipe, stinger); settings are its options, named as OBS names them.
struct Transition {
	QString id, name, type;
	int durationMs = 300;
	QVariantMap settings;
	bool operator==(const Transition &) const = default;
};

struct Source {
	QString id, name, kind, bin, color;
	QVariantMap settings;
	bool hasAudio = false;
	AudioSettings audio;
	bool operator==(const Source &) const = default;
};

struct Instance {
	QString id, sceneId, kind = "source", sourceId, name;
	QStringList children; // groups only, top-first
	bool locked = false;
	PropMap props;
	QHash<Group, QString> links; // group -> link id
	bool operator==(const Instance &) const = default;
};

struct Link {
	QString id;
	Group group = Group::Transform;
	PropMap value; // only `group` is meaningful
	bool operator==(const Link &) const = default;
};

struct Scene {
	QString id, name, folder;
	bool favorite = false;
	QStringList layers; // top-level instance ids, top-first
	bool operator==(const Scene &) const = default;
};

struct KindInfo {
	QString label, icon;
	bool video, audio;
	int w, h; // default size; 0 = canvas
};
const KindInfo &kindInfo(const QString &kind);
QStringList allKinds();

struct Doc {
	int canvasW = 1920, canvasH = 1080;
	QHash<QString, Source> sources;
	QStringList sourceOrder;
	QHash<QString, Scene> scenes;
	QStringList sceneOrder;
	QHash<QString, Instance> instances;
	QHash<QString, Link> links;
	QStringList bins;
	// Audio Mixer arrangement, separate from Sources: pinned sources first, then this order;
	// audio sources in neither go at the end, in Sources order.
	QStringList mixerOrder, mixerPinned;
	// Backgrounds were unlocked before 0.2.1; a show opened in a later version locks them once
	// (then the user can unlock them and it stays that way).
	bool backgroundsLocked = true;
	// Transitions for Show: the list you pick from, and the one in use.
	QVector<Transition> transitions = {{"tr_fade", "Fade", "fade", 300, {}}, {"tr_cut", "Cut", "cut", 0, {}}};
	QString currentTransition = "tr_fade";
	const Transition *transition() const
	{
		for (const Transition &t : transitions)
			if (t.id == currentTransition)
				return &t;
		return transitions.isEmpty() ? nullptr : &transitions.first();
	}
	// The show's saved colors (its palette), shown in every color picker.
	QStringList swatches = {"#ffffff", "#a3a3a3", "#525252", "#000000", "#ef4444", "#f97316",
				"#facc15", "#22c55e", "#14b8a6", "#0ea5e9", "#2563eb", "#ec4899"};

	// --- creation ---
	Source &addSource(const QString &kind, const QString &name, const QVariantMap &settings = {});
	Scene &addScene(const QString &name);
	Instance &placeSource(const QString &sceneId, const QString &sourceId);
	PropMap defaultProps(const Source *src) const;

	// --- linked property access ---
	Transform transform(const Instance &i) const;
	Crop crop(const Instance &i) const;
	QVector<Filter> filters(const Instance &i) const;
	Visibility visibility(const Instance &i) const;
	void setTransform(const QString &id, const Transform &v);
	void setCrop(const QString &id, const Crop &v);
	void setFilters(const QString &id, const QVector<Filter> &v);
	void setVisibility(const QString &id, const Visibility &v);
	QList<const Instance *> linkedPeers(const Instance &i, Group g) const;
	void unlink(const QString &instId, Group g);
	void linkTo(const QString &instId, const QString &targetId, Group g, bool keepMine);

	// --- tree ---
	const Instance *parentOf(const QString &id) const;
	QStringList &siblings(const QString &id);
	QStringList descendants(const QString &id) const; // including self
	struct Leaf {
		QString id;
		bool visible;
		double opacity;
	};
	QVector<Leaf> flatten(const QString &sceneId) const; // bottom-first
	Transform bounds(const QString &id) const;            // group-aware
	void moveBy(const QString &id, double dx, double dy, QSet<QString> &seen);

	// --- edits ---
	void deleteInstance(const QString &id);
	void deleteSource(const QString &id);
	void deleteScene(const QString &id);
	void moveLayer(const QString &id, int delta); // within its sibling list (negative = up)
	QStringList copyToScene(const QStringList &ids, const QString &sceneId, bool linked);
	QString groupLayers(const QStringList &ids);
	void ungroup(const QString &groupId);

	// --- io ---
	QJsonObject toJson() const;
	static Doc fromJson(const QJsonObject &o);
	// Drops references to scenes, layers and links that don't exist (a hand-edited or damaged
	// file). Returns how many it dropped.
	int repair();
	static Doc sample();

private:
	PropMap *propsFor(const QString &instId, Group g);
	const PropMap &propsFor(const Instance &i, Group g) const;
	void gcLinks();

public:
	bool operator==(const Doc &) const = default;
};

QString uid(const QString &prefix);
QString uniqueName(const QString &base, const QStringList &taken);

} // namespace lobs
