#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "platform.h"

static QString settingsPath()
{
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/settings.json";
}

AppSettings AppSettings::load()
{
	AppSettings s;
	QFile f(settingsPath());
	if (!f.open(QIODevice::ReadOnly))
		return s;
	const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
	s.applyProfile(o);
	s.profile = o["profile"].toString(s.profile);
	const QJsonObject tr = o["transition"].toObject();
	s.transition.type = tr["type"].toString(s.transition.type);
	s.transition.durationMs = tr["durationMs"].toInt(s.transition.durationMs);
	s.hotkeys = o["hotkeys"].toObject().toVariantMap();
	s.liveLock = o["liveLock"].toBool(true);
	return s;
}

// A profile: where and how you stream and record (as OBS's profiles). Hotkeys, transitions and
// live lock are about how you work, and stay the same across profiles.
void AppSettings::applyProfile(const QJsonObject &o)
{
	AppSettings &s = *this;
	const AppSettings d; // defaults, for what a profile leaves out
	s.stream = d.stream, s.video = d.video, s.record = d.record;
	const QJsonObject st = o["stream"].toObject(), v = o["video"].toObject(), r = o["record"].toObject();
	s.stream.service = st["service"].toString(s.stream.service);
	s.stream.server = st["server"].toString(s.stream.server);
	s.stream.customUrl = st["customUrl"].toString();
	s.stream.key = platform::unprotectSecret(st["keyProtected"].toString());
	s.stream.bitrate = st["bitrate"].toInt(s.stream.bitrate);
	s.stream.audioBitrate = st["audioBitrate"].toInt(s.stream.audioBitrate);
	s.stream.enhanced = st["enhanced"].toBool(s.stream.enhanced);
	s.video.outWidth = v["outWidth"].toInt(s.video.outWidth);
	s.video.outHeight = v["outHeight"].toInt(s.video.outHeight);
	s.video.fps = v["fps"].toInt(s.video.fps);
	s.video.encoder = v["encoder"].toString(s.video.encoder);
	s.video.preset = v["preset"].toString(s.video.preset);
	s.record.folder = r["folder"].toString();
	s.record.format = r["format"].toString(s.record.format);
	s.record.quality = r["quality"].toString(s.record.quality);
	s.record.replaySeconds = r["replaySeconds"].toInt(s.record.replaySeconds);
}

QJsonObject AppSettings::profileJson() const
{
	return {
		{"stream", QJsonObject{{"service", stream.service}, {"server", stream.server}, {"customUrl", stream.customUrl},
				       {"keyProtected", platform::protectSecret(stream.key)}, {"bitrate", stream.bitrate}, {"audioBitrate", stream.audioBitrate}, {"enhanced", stream.enhanced}}},
		{"video", QJsonObject{{"outWidth", video.outWidth}, {"outHeight", video.outHeight}, {"fps", video.fps}, {"encoder", video.encoder}, {"preset", video.preset}}},
		{"record", QJsonObject{{"folder", record.folder}, {"format", record.format}, {"quality", record.quality}, {"replaySeconds", record.replaySeconds}}},
	};
}

QString AppSettings::profilesFolder() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/profiles"; }

void AppSettings::save() const
{
	QDir().mkpath(QFileInfo(settingsPath()).absolutePath());
	QJsonObject o = profileJson();
	o["profile"] = profile;
	o["hotkeys"] = QJsonObject::fromVariantMap(hotkeys);
	o["liveLock"] = liveLock;
	o["transition"] = QJsonObject{{"type", transition.type}, {"durationMs", transition.durationMs}};
	QFile f(settingsPath());
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		f.write(QJsonDocument(o).toJson());
}

QVariantMap AppSettings::toMap() const
{
	return {{"profile", profile}, {"service", stream.service}, {"server", stream.server}, {"customUrl", stream.customUrl}, {"hasKey", !stream.key.isEmpty()},
		{"bitrate", stream.bitrate}, {"audioBitrate", stream.audioBitrate}, {"enhanced", stream.enhanced}, {"outWidth", video.outWidth}, {"outHeight", video.outHeight},
		{"fps", video.fps}, {"encoder", video.encoder}, {"preset", video.preset}, {"recordFolder", recordFolder()},
		{"recordFormat", record.format}, {"recordQuality", record.quality},
		{"transition", transition.type}, {"transitionMs", transition.durationMs}, {"replaySeconds", record.replaySeconds}, {"hotkeys", hotkeys}, {"liveLock", liveLock}};
}

void AppSettings::applyMap(const QVariantMap &m)
{
	auto str = [&](const char *k, QString &dst) { if (m.contains(k)) dst = m[k].toString(); };
	auto num = [&](const char *k, int &dst) { if (m.contains(k)) dst = m[k].toInt(); };
	str("service", stream.service);
	str("server", stream.server);
	str("customUrl", stream.customUrl);
	if (m.contains("key") && !m["key"].toString().isEmpty())
		stream.key = m["key"].toString().trimmed();
	if (m.value("clearKey").toBool())
		stream.key.clear();
	num("bitrate", stream.bitrate);
	num("audioBitrate", stream.audioBitrate);
	if (m.contains("enhanced"))
		stream.enhanced = m["enhanced"].toBool();
	num("outWidth", video.outWidth);
	num("outHeight", video.outHeight);
	num("fps", video.fps);
	str("encoder", video.encoder);
	str("preset", video.preset);
	if (m.contains("recordFolder")) {
		const QString f = m["recordFolder"].toString();
		record.folder = (f == defaultRecordFolder()) ? QString() : f;
	}
	num("replaySeconds", record.replaySeconds);
	record.replaySeconds = std::clamp(record.replaySeconds, 5, 600);
	if (m.contains("hotkeys"))
		hotkeys = m["hotkeys"].toMap();
	if (m.contains("liveLock"))
		liveLock = m["liveLock"].toBool();
	str("transition", transition.type);
	num("transitionMs", transition.durationMs);
	transition.durationMs = std::clamp(transition.durationMs, 50, 5000);
	str("recordFormat", record.format);
	str("recordQuality", record.quality);
}

QString &AppSettings::defaultRecordFolderOverride()
{
	static QString folder;
	return folder;
}

QString AppSettings::defaultRecordFolder()
{
	const QString &o = defaultRecordFolderOverride();
	return o.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + "/LOBS" : o;
}

QString AppSettings::recordFolder() const
{
	return record.folder.isEmpty() ? defaultRecordFolder() : record.folder;
}
