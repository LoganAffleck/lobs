#include "app_internal.h"
#include "model/audio_fx.h"
#include "plugins.h"

#include <obs.h>

#include <QClipboard>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

using namespace lobs;

static QStringList mixerArrangement(const Doc &d)
{
	QStringList out;
	auto take = [&](const QString &id) {
		if (d.sources.value(id).hasAudio && !out.contains(id))
			out << id;
	};
	for (const QString &id : d.mixerOrder)
		if (d.mixerPinned.contains(id))
			take(id);
	for (const QString &id : d.mixerPinned)
		take(id);
	for (const QString &id : d.mixerOrder)
		take(id);
	for (const QString &id : d.sourceOrder)
		take(id);
	return out;
}

void App::moveMixer(const QString &id, int toIndex)
{
	QStringList order = mixerArrangement(doc());
	const int from = int(order.indexOf(id));
	if (from < 0)
		return;
	toIndex = std::clamp(toIndex, 0, int(order.size()) - 1);
	const int pinnedCount = int(std::count_if(order.begin(), order.end(), [&](const QString &s) { return doc().mixerPinned.contains(s); }));
	toIndex = doc().mixerPinned.contains(id) ? std::min(toIndex, pinnedCount - 1) : std::max(toIndex, pinnedCount);
	if (toIndex == from)
		return;
	order.move(from, toIndex);
	m_store.commit("Reorder mixer", [&](Doc &d) { d.mixerOrder = order; });
}

void App::setMixerPinned(const QString &id, bool pinned)
{
	if (!doc().sources.value(id).hasAudio || doc().mixerPinned.contains(id) == pinned)
		return;
	const QStringList order = mixerArrangement(doc());
	m_store.commit((pinned ? "Pin " : "Unpin ") + doc().sources.value(id).name, [&](Doc &d) {
		d.mixerOrder = order;
		if (pinned)
			d.mixerPinned << id;
		else
			d.mixerPinned.removeAll(id);
	});
}

QVariantList App::mixer() const
{
	QVariantList out;
	QSet<QString> placed;
	for (const Instance &i : doc().instances)
		placed.insert(i.sourceId);
	for (const QString &id : mixerArrangement(doc())) {
		const Source &s = doc().sources[id];
		if (!s.hasAudio)
			continue;
		const bool global = !kindInfo(s.kind).video;
		if (!global && !placed.contains(id))
			continue;
		QVariantList tracks;
		for (int t = 0; t < std::min<qsizetype>(5, s.audio.tracks.size()); t++)
			tracks << s.audio.tracks[t];
		QStringList warn;
		if (s.kind == "desktop_audio" && s.audio.monitor == "monitor_and_output")
			warn << "Monitoring desktop audio causes an echo.";
		if (s.kind == "mic" && s.audio.monitor == "monitor_and_output")
			warn << "You're monitoring your mic.";
		if (s.kind == "game" && !s.audio.muted && std::any_of(doc().sources.cbegin(), doc().sources.cend(), [](const Source &o) { return o.kind == "desktop_audio" && !o.audio.muted; }))
			warn << "Desktop audio hears this game too. Mute one so it isn't doubled.";
		if (s.audio.monitor == "monitor_only")
			warn << "Monitor only: not in the stream or recording.";
		if (s.kind == "browser" && s.audio.monitor == "off")
			warn << "Viewers hear this page but you don't. Turn on monitoring to hear it.";
		QVariantList fx;
		for (const Filter &f : s.audio.filters) {
			const FxType *t = audioFx(f.type);
			QVariantList params;
			if (t)
				for (const FxParam &p : t->params) {
					const QString kind = p.suffix == " ms" ? "time" : p.suffix == ":1" ? "ratio" : p.min < 0 && p.max > 0 ? "gain"
							   : p.suffix.isEmpty() && p.max <= 1 ? "percent" : "level";
					QVariantList presets;
					if (kind == "ratio")
						for (double r : t->type == "expander" ? QList<double>{2, 3, 4, 6} : QList<double>{2, 4, 10, 20})
							if (r >= p.min && r <= p.max)
								presets << r;
					params << QVariantMap{{"key", p.key}, {"label", p.label}, {"suffix", p.suffix}, {"min", p.min}, {"max", p.max},
							      {"decimals", p.decimals}, {"def", p.def}, {"kind", kind}, {"presets", presets},
							      {"value", f.settings.value(p.key, p.def).toDouble()}};
				}
			const QString plugin = t ? QString() : pluginTypeLabel(f.type);
			const QString label = t ? t->label : !plugin.isEmpty() ? plugin : f.settings.value("_label", f.type.mid(4)).toString();
			fx << QVariantMap{{"type", f.type}, {"label", label}, {"enabled", f.enabled}, {"params", params}, {"sidechain", f.settings.value("sidechain_source").toString()}, {"known", t != nullptr || !plugin.isEmpty()},
					  {"obs", !plugin.isEmpty()}};
		}
		out << QVariantMap{{"id", id}, {"name", s.name}, {"icon", kindInfo(s.kind).icon}, {"color", s.color}, {"volume", s.audio.volume},
				   {"muted", s.audio.muted}, {"monitor", s.audio.monitor}, {"monitorMutes", s.audio.monitorMutes}, {"tracks", tracks}, {"inRecording", s.audio.inRecording}, {"mutedHere", doc().scenes.value(doc().mainScene(m_editScene)).mutedAudio.contains(id)}, {"ducksOthers", doc().duckUnder == id}, {"global", global}, {"kind", s.kind}, {"warnings", warn}, {"fx", fx},
				   {"mono", s.audio.mono}, {"balance", s.audio.balance}, {"syncMs", s.audio.syncMs}, {"pinned", doc().mixerPinned.contains(id)}};
	}
	return out;
}

QVariantList App::audioFxTypes() const
{
	QVariantList out;
	for (const FxType &t : lobs::audioFxTypes())
		if (obs_source_get_display_name(t.obsId.toUtf8().constData()))
			out << QVariantMap{{"type", t.type}, {"label", t.label}};
	for (const QString &id : lobs::plugins::filterTypes()) {
		const uint32_t flags = obs_get_source_output_flags(id.toUtf8().constData());
		if ((flags & OBS_SOURCE_AUDIO) && !(flags & (OBS_SOURCE_VIDEO | OBS_SOURCE_CAP_DISABLED | OBS_SOURCE_DEPRECATED)))
			out << QVariantMap{{"type", "obs:" + id}, {"label", pluginTypeLabel("obs:" + id) + " (plugin)"}};
	}
	return out;
}

void App::addAudioFilter(const QString &sourceId, const QString &type)
{
	const FxType *t = audioFx(type);
	const QString label = t ? t->label : pluginTypeLabel(type);
	if (label.isEmpty() || !doc().sources.contains(sourceId))
		return;
	m_store.commit("Add " + label, [&](Doc &d) { d.sources[sourceId].audio.filters << Filter{uid("afx_"), type, true, audioFxDefaults(type)}; });
	if (type.startsWith("obs:"))
		settleObsLists("audio", sourceId, int(doc().sources[sourceId].audio.filters.size()) - 1);
}

QJsonObject App::effectsJson(const QString &sourceId, const QString &name) const
{
	QJsonArray fx;
	for (const Filter &f : doc().sources.value(sourceId).audio.filters)
		fx << QJsonObject{{"type", f.type}, {"enabled", f.enabled}, {"settings", QJsonObject::fromVariantMap(f.settings)}};
	return {{"format", "lobsfx"}, {"version", 1}, {"name", name.isEmpty() ? doc().sources.value(sourceId).name : name}, {"effects", fx}};
}

static bool isEffectsJson(const QJsonObject &o) { return o.value("format").toString() == "lobsfx" && o.value("effects").isArray(); }

bool App::applyEffects(const QStringList &sourceIds, const QJsonObject &o, bool replace, const QString &what)
{
	if (!isEffectsJson(o))
		return false;
	QVector<Filter> chain;
	for (const QJsonValue v : o.value("effects").toArray()) {
		const QJsonObject e = v.toObject();
		const QString type = e.value("type").toString();
		if (type.isEmpty())
			continue;
		QVariantMap st = audioFxDefaults(type);
		const QVariantMap given = e.value("settings").toObject().toVariantMap();
		for (auto it = given.cbegin(); it != given.cend(); ++it)
			st[it.key()] = it.value();
		chain << Filter{QString(), type, e.value("enabled").toBool(true), st};
	}
	QStringList targets;
	for (const QString &id : sourceIds)
		if (doc().sources.contains(id) && doc().sources[id].hasAudio)
			targets << id;
	if (targets.isEmpty())
		return false;
	m_store.commit(what, [&](Doc &d) {
		for (const QString &id : targets) {
			QVector<Filter> &fl = d.sources[id].audio.filters;
			if (replace)
				fl.clear();
			for (Filter f : chain) {
				f.id = uid("afx_");
				fl << f;
			}
		}
	});
	return true;
}

void App::copyAudioFilters(const QString &sourceId)
{
	if (!doc().sources.contains(sourceId))
		return;
	QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(effectsJson(sourceId, {})).toJson(QJsonDocument::Compact)));
	showToast("Copied " + QString::number(doc().sources[sourceId].audio.filters.size()) + " effects from " + doc().sources[sourceId].name);
}

bool App::canPasteAudioFilters() const
{
	return isEffectsJson(QJsonDocument::fromJson(QGuiApplication::clipboard()->text().toUtf8()).object());
}

void App::pasteAudioFilters(const QStringList &sourceIds, bool replace)
{
	const QJsonObject o = QJsonDocument::fromJson(QGuiApplication::clipboard()->text().toUtf8()).object();
	if (!applyEffects(sourceIds, o, replace, replace ? "Paste effects" : "Add pasted effects"))
		showToast("Nothing to paste. Copy effects from a source first.");
}

bool App::saveAudioFilters(const QString &sourceId, const QString &path, const QString &name)
{
	QFile f(path);
	if (!doc().sources.contains(sourceId) || !f.open(QIODevice::WriteOnly)) {
		showToast("Couldn't save the effects there.");
		return false;
	}
	f.write(QJsonDocument(effectsJson(sourceId, name)).toJson());
	showToast("Saved " + QFileInfo(path).fileName());
	return true;
}

bool App::loadAudioFilters(const QStringList &sourceIds, const QString &path, bool replace)
{
	QFile f(path);
	const QJsonObject o = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
	if (!applyEffects(sourceIds, o, replace, "Load " + o.value("name").toString(QFileInfo(path).completeBaseName()))) {
		showToast("That file isn't a LOBS effects file.");
		return false;
	}
	return true;
}

static const QList<QPair<QStringList, QJsonObject>> &audioPresetTable()
{
	auto fx = [](const QString &type, const QJsonObject &settings) { return QJsonObject{{"type", type}, {"enabled", true}, {"settings", settings}}; };
	auto chain = [](const QString &name, const QJsonArray &effects) { return QJsonObject{{"format", "lobsfx"}, {"version", 1}, {"name", name}, {"effects", effects}}; };
	static const QList<QPair<QStringList, QJsonObject>> table = {
		{{"clean", "Clean voice", "Takes out steady background noise and evens out your level."},
		 chain("Clean voice", {fx("noise_suppress", {{"suppress_level", -30}}),
				       fx("compressor", {{"threshold", -20}, {"ratio", 3}, {"attack_time", 6}, {"release_time", 60}, {"output_gain", 2}})})},
		{{"broadcast", "Broadcast voice", "Quiet between words, a fuller tone, and no clipping."},
		 chain("Broadcast voice", {fx("noise_suppress", {{"suppress_level", -30}}),
					   fx("noise_gate", {{"close_threshold", -45}, {"open_threshold", -38}, {"attack_time", 10}, {"hold_time", 150}, {"release_time", 120}}),
					   fx("eq", {{"low", -2}, {"mid", 1}, {"high", 2}}),
					   fx("compressor", {{"threshold", -18}, {"ratio", 4}, {"attack_time", 5}, {"release_time", 80}, {"output_gain", 4}}),
					   fx("limiter", {{"threshold", -1}, {"release_time", 60}})})},
		{{"noisy", "Noisy room", "Stronger noise removal and a tighter gate for fans, keyboards and traffic."},
		 chain("Noisy room", {fx("noise_suppress", {{"suppress_level", -45}}),
				      fx("noise_gate", {{"close_threshold", -36}, {"open_threshold", -30}, {"attack_time", 10}, {"hold_time", 200}, {"release_time", 150}}),
				      fx("compressor", {{"threshold", -20}, {"ratio", 3}, {"attack_time", 6}, {"release_time", 60}, {"output_gain", 2}})})},
		{{"loud", "Loud moments", "Keeps yells and laughs from blasting your viewers."},
		 chain("Loud moments", {fx("compressor", {{"threshold", -24}, {"ratio", 6}, {"attack_time", 2}, {"release_time", 120}, {"output_gain", 3}}),
					fx("limiter", {{"threshold", -1}, {"release_time", 60}})})},
	};
	return table;
}

static QString userPresetDir()
{
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/effects";
}

QVariantList App::audioPresets() const
{
	QVariantList out;
	for (const auto &p : audioPresetTable())
		out << QVariantMap{{"id", p.first[0]}, {"name", p.first[1]}, {"desc", p.first[2]}};
	for (const QFileInfo &f : QDir(userPresetDir()).entryInfoList({"*.lobsfx"}, QDir::Files, QDir::Name))
		out << QVariantMap{{"id", "file:" + f.absoluteFilePath()}, {"name", f.completeBaseName()}, {"desc", QString()}, {"mine", true}};
	return out;
}

void App::applyAudioPreset(const QString &sourceId, const QString &presetId)
{
	if (presetId.startsWith("file:")) {
		loadAudioFilters({sourceId}, presetId.mid(5), true);
		return;
	}
	for (const auto &p : audioPresetTable())
		if (p.first[0] == presetId)
			applyEffects({sourceId}, p.second, true, "Preset " + p.first[1]);
}

bool App::saveAudioPreset(const QString &sourceId, const QString &name)
{
	QString n = name.trimmed();
	n.replace(QRegularExpression(R"([\\/:*?"<>|])"), "-");
	if (n.isEmpty() || !doc().sources.contains(sourceId))
		return false;
	QDir().mkpath(userPresetDir());
	QFile f(userPresetDir() + "/" + n + ".lobsfx");
	if (!f.open(QIODevice::WriteOnly)) {
		showToast("Couldn't save the preset.");
		return false;
	}
	f.write(QJsonDocument(effectsJson(sourceId, n)).toJson());
	showToast(QStringLiteral(u"Saved \u201c") + n + QStringLiteral(u"\u201d to + Effect \u2192 Start from a preset"));
	return true;
}

void App::applyMicTuning(const QString &micId, const QVariantMap &m)
{
	if (!doc().sources.contains(micId))
		return;
	const double vol = std::max(1e-4, doc().sources[micId].audio.volume);
	const double volDb = 20 * std::log10(vol);
	const double floor = m.value("floor", -60).toDouble() - volDb;
	const double speech = m.value("speech", -20).toDouble() - volDb;
	const double loud = std::max(speech, m.value("loud", speech).toDouble() - volDb);
	auto clamp = [](double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); };
	auto r1 = [](double v) { return std::round(v * 10) / 10; };

	const bool needGate = floor >= -65, needSuppress = floor >= -52;
	double gain = clamp(-12 - speech, -10, 24);
	if (!needSuppress && floor + gain > -50 && floor > -80)
		gain = std::max(std::min(gain, -50 - floor), std::min(0.0, gain));
	const double suppress = floor < -45 ? -30 : -40;
	const double open = clamp(floor + 10, -70, speech - 18);
	const double close = clamp(floor + 4, -75, open - 4);
	const double spread = loud - speech;
	const double ratio = spread > 12 ? 4 : 3;
	const double threshold = clamp(speech + gain - 5, -40, -6);
	const double reduction = (speech + gain - threshold) * (1 - 1 / ratio);
	const double makeup = clamp(std::round(reduction * 0.6), 0, 3);

	auto fx = [](const QString &type, const QVariantMap &settings) {
		QVariantMap st = audioFxDefaults(type);
		for (auto it = settings.cbegin(); it != settings.cend(); ++it)
			st[it.key()] = it.value();
		return Filter{uid("afx_"), type, true, st};
	};
	QVector<Filter> chain;
	if (needSuppress)
		chain << fx("noise_suppress", {{"suppress_level", suppress}});
	if (needGate)
		chain << fx("noise_gate", {{"close_threshold", r1(close)}, {"open_threshold", r1(open)}, {"attack_time", 5}, {"hold_time", 250}, {"release_time", 200}});
	if (std::abs(gain) >= 0.5)
		chain << fx("gain", {{"db", r1(gain)}});
	chain << fx("compressor", {{"threshold", r1(threshold)}, {"ratio", ratio}, {"attack_time", 10}, {"release_time", 100}, {"output_gain", makeup}})
	      << fx("limiter", {{"threshold", -1}, {"release_time", 60}});

	const QString deskId = m.value("deskId").toString();
	const bool desk = !deskId.isEmpty() && doc().sources.contains(deskId) && m.contains("desk");
	double deskVol = desk ? doc().sources[deskId].audio.volume : 1;
	if (desk) {
		const double deskRaw = m.value("desk").toDouble() - 20 * std::log10(std::max(1e-4, deskVol));
		const double want = (speech + gain) - 8;
		deskVol = std::pow(10.0, clamp(want - deskRaw, -40, 0) / 20);
	}
	m_store.commit("Tune " + doc().sources[micId].name, [&](Doc &d) {
		d.sources[micId].audio.filters = chain;
		if (desk)
			d.sources[deskId].audio.volume = deskVol;
	});
}

struct App::ClipRec {
	QMutex lock;
	std::vector<float> samples;
	bool on = false;
};

static void clipTap(void *param, const float *const *data, int channels, int frames, bool muted)
{
	auto *c = static_cast<App::ClipRec *>(param);
	QMutexLocker l(&c->lock);
	if (!c->on || !data || !data[0])
		return;
	for (int i = 0; i < frames; ++i) {
		float v = 0;
		for (int ch = 0; ch < channels && data[ch]; ++ch)
			v += data[ch][i];
		c->samples.push_back(muted ? 0.f : v / std::max(1, channels));
	}
}

void App::startClip(const QString &sourceId)
{
	stopClip();
	if (!m_clip)
		m_clip = new ClipRec;
	{
		QMutexLocker l(&m_clip->lock);
		m_clip->samples.clear();
		m_clip->on = true;
	}
	m_clipSource = sourceId;
	m_engine->addAudioTap(sourceId, clipTap, m_clip);
}

QString App::stopClip(const QString &name)
{
	if (!m_clip || m_clipSource.isEmpty())
		return {};
	m_engine->removeAudioTap(m_clipSource, clipTap, m_clip);
	m_clipSource.clear();
	std::vector<float> s;
	{
		QMutexLocker l(&m_clip->lock);
		m_clip->on = false;
		s.swap(m_clip->samples);
	}
	if (name.isEmpty() || s.empty())
		return {};
	const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("lobs-" + name + ".wav");
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly))
		return {};
	const quint32 rate = quint32(m_engine->audioSampleRate()), bytes = quint32(s.size() * 2);
	QDataStream out(&f);
	out.setByteOrder(QDataStream::LittleEndian);
	f.write("RIFF");
	out << quint32(36 + bytes);
	f.write("WAVEfmt ");
	out << quint32(16) << quint16(1) << quint16(1) << rate << quint32(rate * 2) << quint16(2) << quint16(16);
	f.write("data");
	out << bytes;
	for (float v : s)
		out << qint16(std::lround(std::max(-1.f, std::min(1.f, v)) * 32767));
	return path;
}

void App::playClip(const QString &path)
{
#ifdef Q_OS_WIN
	PlaySoundW(path.isEmpty() ? nullptr : reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(path).utf16()), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
#else
	Q_UNUSED(path);
#endif
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

void App::setAudioFilterText(const QString &sourceId, int index, const QString &key, const QString &value)
{
	if (!doc().sources.contains(sourceId) || index < 0 || index >= doc().sources[sourceId].audio.filters.size())
		return;
	m_store.commit("Edit audio effect", [&](Doc &d) { d.sources[sourceId].audio.filters[index].settings[key] = value; });
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
		else if (field == "monitorMutes") a.monitorMutes = value.toBool();
		else if (field == "mono") a.mono = value.toBool();
		else if (field == "inRecording") a.inRecording = value.toBool();
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

void App::setDuckUnder(const QString &micId, bool on)
{
	if (!doc().sources.contains(micId) || (doc().duckUnder == micId) == on)
		return;
	const QString old = doc().duckUnder;
	m_store.commit((on ? "Duck under " : "Stop ducking under ") + doc().sources[micId].name, [&](Doc &d) {
		d.duckUnder = on ? micId : QString();
		for (Source &s : d.sources) {
			if (!s.hasAudio || s.kind == "mic")
				continue;
			QVector<Filter> &df = s.audio.filters;
			df.erase(std::remove_if(df.begin(), df.end(), [&](const Filter &f) {
				const QString sc = f.settings.value("sidechain_source").toString();
				return f.type == "duck" && (sc == micId || (!old.isEmpty() && sc == old));
			}), df.end());
			if (on)
				df << duckFilter(micId);
		}
	});
}

void App::setMutedInScene(const QString &sourceId, bool muted)
{
	const QString sc = doc().mainScene(m_editScene);
	if (!doc().scenes.contains(sc) || doc().scenes[sc].mutedAudio.contains(sourceId) == muted)
		return;
	m_store.commit((muted ? "Mute " : "Unmute ") + doc().sources.value(sourceId).name + " in " + doc().scenes[sc].name, [&](Doc &d) {
		QStringList &m = d.scenes[sc].mutedAudio;
		muted ? (void)m.append(sourceId) : (void)m.removeAll(sourceId);
	});
}
