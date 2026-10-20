// Audio effects catalog. Audio filters live on the pool source (audio is shared by every
// scene), use libobs's own filters, and keep OBS's setting keys so OBS chains import as-is.
// Unknown OBS filters are kept as type "obs:<id>" and passed through untouched.
#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector>

namespace lobs {

struct FxParam {
	QString key, label, suffix;
	double min, max, def;
	int decimals;
};

struct FxType {
	QString type, obsId, label;
	QVector<FxParam> params;
	QVariantMap fixed; // settings without a control (e.g. which noise model)
};

inline const QVector<FxType> &audioFxTypes()
{
	static const QVector<FxType> types = {
		{"noise_suppress", "noise_suppress_filter_v2", "Noise Suppression", {{"suppress_level", "Suppression", " dB", -60, 0, -30, 0}}, {{"method", "rnnoise"}}},
		{"noise_gate", "noise_gate_filter", "Noise Gate",
		 {{"close_threshold", "Close below", " dB", -96, 0, -32, 1}, {"open_threshold", "Open above", " dB", -96, 0, -26, 1},
		  {"attack_time", "Attack", " ms", 0, 500, 25, 0}, {"hold_time", "Hold", " ms", 0, 2000, 200, 0}, {"release_time", "Release", " ms", 0, 2000, 150, 0}},
		 {}},
		{"gain", "gain_filter", "Gain", {{"db", "Gain", " dB", -30, 30, 0, 1}}, {}},
		{"eq", "basic_eq_filter", "3-Band EQ", {{"low", "Low", " dB", -20, 20, 0, 1}, {"mid", "Mid", " dB", -20, 20, 0, 1}, {"high", "High", " dB", -20, 20, 0, 1}}, {}},
		{"compressor", "compressor_filter", "Compressor",
		 {{"threshold", "Threshold", " dB", -60, 0, -18, 1}, {"ratio", "Ratio", ":1", 1, 32, 10, 1}, {"attack_time", "Attack", " ms", 1, 500, 6, 0},
		  {"release_time", "Release", " ms", 1, 1000, 60, 0}, {"output_gain", "Output gain", " dB", -32, 32, 0, 1}},
		 {}},
		{"expander", "expander_filter", "Expander",
		 {{"threshold", "Threshold", " dB", -60, 0, -40, 1}, {"ratio", "Ratio", ":1", 1, 20, 2, 1}, {"attack_time", "Attack", " ms", 1, 100, 10, 0},
		  {"release_time", "Release", " ms", 1, 1000, 50, 0}, {"output_gain", "Output gain", " dB", -32, 32, 0, 1}},
		 {{"presets", "expander"}}},
		{"limiter", "limiter_filter", "Limiter", {{"threshold", "Threshold", " dB", -60, 0, -6, 1}, {"release_time", "Release", " ms", 1, 1000, 60, 0}}, {}},
		{"nvafx", "nvidia_audiofx_filter", "NVIDIA Audio Effects", {{"intensity", "Strength", "", 0, 1, 1, 2}}, {}},
	};
	return types;
}

inline const FxType *audioFx(const QString &type)
{
	for (const FxType &t : audioFxTypes())
		if (t.type == type)
			return &t;
	return nullptr;
}

inline const FxType *audioFxForObs(const QString &obsId)
{
	for (const FxType &t : audioFxTypes())
		if (t.obsId == obsId)
			return &t;
	return nullptr;
}

// libobs filter id for a stored audio filter type (catalog or pass-through), or empty.
inline QString audioFxObsId(const QString &type)
{
	if (type.startsWith("obs:"))
		return type.mid(4);
	const FxType *t = audioFx(type);
	return t ? t->obsId : QString();
}

// Defaults for a new filter of `type`.
inline QVariantMap audioFxDefaults(const QString &type)
{
	QVariantMap m;
	if (const FxType *t = audioFx(type)) {
		m = t->fixed;
		for (const FxParam &p : t->params)
			m[p.key] = p.def;
	}
	return m;
}

} // namespace lobs
