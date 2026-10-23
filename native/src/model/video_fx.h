// Video filter catalog. Each layer filter is a libobs filter; the catalog says which one, the
// inspector controls for each setting, and how a stored setting maps to OBS's key. Settings
// are stored under OBS's own keys except where an older LOBS name predates the catalog.
#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace lobs {

struct VfxParam {
	QString key, label;
	QString kind; // "slider", "color" (#rrggbb), "choice", "file", "toggle"
	QVariant def;
	double min = 0, max = 1, scale = 1; // slider; scale is for display only (0..1 shown as %)
	int decimals = 2;
	QString suffix;
	QVariantList options; // choice: [{name, value}]
	QString fileFilter;   // file: dialog name filter
	QString showIf;       // "key=a,b": shown and applied only when another setting is one of these
	QString obsKey;       // OBS's key when LOBS stores the setting under another name
	QVariant missing;     // value for a filter saved before this setting existed (default: def)

	VfxParam &when(const QString &cond) { return showIf = cond, *this; }
	VfxParam &obs(const QString &k) { return obsKey = k, *this; }
	VfxParam &percent() { return scale = 100, decimals = 0, suffix = "%", *this; }
	VfxParam &ifMissing(const QVariant &v) { return missing = v, *this; }
};

struct VfxType {
	QString type, obsId, label;
	QVector<VfxParam> params;
};

namespace vfx {
inline VfxParam slider(const QString &key, const QString &label, double min, double max, double def, int decimals = 2, const QString &suffix = {})
{
	VfxParam p{key, label, "slider", def};
	p.min = min, p.max = max, p.decimals = decimals, p.suffix = suffix;
	return p;
}
inline VfxParam color(const QString &key, const QString &label, const QString &def) { return {key, label, "color", def}; }
inline VfxParam toggle(const QString &key, const QString &label, bool def) { return {key, label, "toggle", def}; }
inline VfxParam file(const QString &key, const QString &label, const QString &filter)
{
	VfxParam p{key, label, "file", QString()};
	p.fileFilter = filter;
	return p;
}
// Options as "value|Name" pairs.
inline VfxParam choice(const QString &key, const QString &label, const QString &def, const QStringList &opts)
{
	VfxParam p{key, label, "choice", def};
	for (const QString &o : opts)
		p.options << QVariantMap{{"value", o.section('|', 0, 0)}, {"name", o.section('|', 1)}};
	return p;
}
} // namespace vfx

inline const QVector<VfxType> &videoFxTypes()
{
	using namespace vfx;
	static const QStringList keyColors = {"green|Green", "blue|Blue", "magenta|Magenta", "custom|Custom color"};
	static const QVector<VfxType> types = {
		{"color", "color_filter_v2", "Color Correction",
		 {slider("gamma", "Gamma", -3, 3, 0), slider("contrast", "Contrast", -4, 4, 0), slider("brightness", "Brightness", -1, 1, 0),
		  slider("saturation", "Saturation", -1, 5, 0), slider("hue", "Hue shift", -180, 180, 0, 0, "°").obs("hue_shift"),
		  slider("opacity", "Opacity", 0, 1, 1).percent(), color("color_multiply", "Color multiply", "#ffffff"),
		  color("color_add", "Color add", "#000000")}},
		{"chroma_key", "chroma_key_filter_v2", "Chroma Key",
		 // Before key color types, LOBS always keyed a custom color.
		 {choice("key_color_type", "Key color", "green", keyColors).ifMissing("custom"), color("key_color", "Custom color", "#00ff00").when("key_color_type=custom"),
		  slider("similarity", "Similarity", 1, 1000, 400, 0), slider("smoothness", "Smoothness", 1, 1000, 80, 0),
		  slider("spill", "Spill reduction", 1, 1000, 100, 0), slider("opacity", "Opacity", 0, 1, 1).percent(),
		  slider("contrast", "Contrast", -4, 4, 0), slider("brightness", "Brightness", -1, 1, 0), slider("gamma", "Gamma", -1, 1, 0)}},
		{"color_key", "color_key_filter_v2", "Color Key",
		 {choice("key_color_type", "Key color", "green", {"green|Green", "blue|Blue", "red|Red", "magenta|Magenta", "custom|Custom color"}),
		  color("key_color", "Custom color", "#00ff00").when("key_color_type=custom"), slider("similarity", "Similarity", 1, 1000, 80, 0),
		  slider("smoothness", "Smoothness", 1, 1000, 50, 0), slider("opacity", "Opacity", 0, 1, 1).percent(),
		  slider("contrast", "Contrast", -4, 4, 0), slider("brightness", "Brightness", -1, 1, 0), slider("gamma", "Gamma", -1, 1, 0)}},
		{"luma_key", "luma_key_filter_v2", "Luma Key",
		 {slider("luma_max", "Luma max", 0, 1, 1, 3), slider("luma_max_smooth", "Luma max smooth", 0, 1, 0, 3),
		  slider("luma_min", "Luma min", 0, 1, 0, 3), slider("luma_min_smooth", "Luma min smooth", 0, 1, 0, 3)}},
		{"mask", "mask_filter_v2", "Image Mask/Blend",
		 {choice("type", "Type", "mask_color_filter.effect",
			 {"mask_color_filter.effect|Alpha mask (color channel)", "mask_alpha_filter.effect|Alpha mask (alpha channel)",
			  "blend_mul_filter.effect|Blend (multiply)", "blend_add_filter.effect|Blend (add)", "blend_sub_filter.effect|Blend (subtract)"}),
		  file("image_path", "Image", "Images (*.png *.jpg *.jpeg *.bmp *.tga *.gif *.webp)"), color("color", "Color", "#ffffff"),
		  slider("opacity", "Opacity", 0, 1, 1).percent(), toggle("stretch", "Stretch image (ignore aspect)", false)}},
		{"sharpness", "sharpness_filter_v2", "Sharpen", {slider("amount", "Sharpness", 0, 1, 0.08).obs("sharpness")}},
		{"scroll", "scroll_filter", "Scroll",
		 {slider("speed_x", "Horizontal speed", -500, 500, 0, 0), slider("speed_y", "Vertical speed", -500, 500, 0, 0),
		  toggle("limit_cx", "Limit width", false), slider("cx", "Width", 1, 8192, 100, 0, " px").when("limit_cx=true"),
		  toggle("limit_cy", "Limit height", false), slider("cy", "Height", 1, 8192, 100, 0, " px").when("limit_cy=true"), toggle("loop", "Loop", true)}},
		{"scale", "scale_filter", "Scaling/Aspect Ratio",
		 // "canvas" becomes the canvas size; OBS's own value for it is localized text.
		 {choice("sampling", "Scale filtering", "bicubic",
			 {"point|Point", "bilinear|Bilinear", "bicubic|Bicubic", "lanczos|Lanczos", "area|Area"}),
		  choice("resolution", "Resolution", "",
			 {"|None", "canvas|Canvas size", "16:9|16:9", "16:10|16:10", "4:3|4:3", "1:1|1:1", "1920x1080|1920×1080", "1600x900|1600×900",
			  "1280x720|1280×720", "960x540|960×540", "854x480|854×480", "640x360|640×360"}),
		  toggle("undistort", "Undistort center (for ultrawide)", false).when("sampling=bicubic,lanczos")}},
		{"lut", "clut_filter", "Apply LUT",
		 {file("path", "LUT file", "LUTs (*.cube *.png)").obs("image_path"), slider("amount", "Amount", 0, 1, 1).obs("clut_amount")}},
		{"render_delay", "gpu_delay", "Render Delay", {slider("delay_ms", "Delay", 0, 500, 0, 0, " ms")}},
	};
	return types;
}

inline const VfxType *videoFx(const QString &type)
{
	for (const VfxType &t : videoFxTypes())
		if (t.type == type)
			return &t;
	return nullptr;
}

// Catalog filter for an OBS filter id (any version: "color_filter" matches "color_filter_v2").
inline const VfxType *videoFxForObs(const QString &obsId)
{
	const auto unversioned = [](const QString &id) {
		const int v = id.lastIndexOf("_v");
		bool num = false;
		return v > 0 && (id.mid(v + 2).toInt(&num), num) ? id.left(v) : id;
	};
	for (const VfxType &t : videoFxTypes())
		if (unversioned(t.obsId) == unversioned(obsId))
			return &t;
	return nullptr;
}

// A stored setting's value, falling back to the param's default (or its "saved before" value).
inline QVariant videoFxValue(const QVariantMap &settings, const VfxParam &p)
{
	if (settings.contains(p.key))
		return settings.value(p.key);
	return p.missing.isValid() ? p.missing : p.def;
}

// Whether a param applies given the filter's other settings (see VfxParam::showIf).
inline bool videoFxParamActive(const QVariantMap &settings, const VfxType &t, const VfxParam &p)
{
	if (p.showIf.isEmpty())
		return true;
	const QString key = p.showIf.section('=', 0, 0);
	const QStringList vals = p.showIf.section('=', 1).split(',');
	for (const VfxParam &q : t.params)
		if (q.key == key)
			return vals.contains(videoFxValue(settings, q).toString());
	return true;
}

inline QVariantMap videoFxDefaults(const QString &type)
{
	QVariantMap m;
	if (const VfxType *t = videoFx(type))
		for (const VfxParam &p : t->params)
			m[p.key] = p.def;
	return m;
}

} // namespace lobs
