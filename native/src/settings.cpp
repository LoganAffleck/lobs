#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <windows.h>
#include <dpapi.h>

static QString settingsPath()
{
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/settings.json";
}

static QString protect(const QString &plain)
{
	if (plain.isEmpty())
		return {};
	const QByteArray in = plain.toUtf8();
	DATA_BLOB src{DWORD(in.size()), reinterpret_cast<BYTE *>(const_cast<char *>(in.data()))}, out{};
	if (!CryptProtectData(&src, L"LOBS stream key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
		return {};
	const QByteArray enc(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
	LocalFree(out.pbData);
	return QString::fromLatin1(enc.toBase64());
}

static QString unprotect(const QString &b64)
{
	if (b64.isEmpty())
		return {};
	const QByteArray in = QByteArray::fromBase64(b64.toLatin1());
	DATA_BLOB src{DWORD(in.size()), reinterpret_cast<BYTE *>(const_cast<char *>(in.data()))}, out{};
	if (!CryptUnprotectData(&src, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
		return {};
	const QString plain = QString::fromUtf8(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
	SecureZeroMemory(out.pbData, out.cbData);
	LocalFree(out.pbData);
	return plain;
}

AppSettings AppSettings::load()
{
	AppSettings s;
	QFile f(settingsPath());
	if (!f.open(QIODevice::ReadOnly))
		return s;
	const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
	const QJsonObject st = o["stream"].toObject(), v = o["video"].toObject(), r = o["record"].toObject();
	s.stream.service = st["service"].toString(s.stream.service);
	s.stream.server = st["server"].toString(s.stream.server);
	s.stream.customUrl = st["customUrl"].toString();
	s.stream.key = unprotect(st["keyProtected"].toString());
	s.stream.bitrate = st["bitrate"].toInt(s.stream.bitrate);
	s.stream.audioBitrate = st["audioBitrate"].toInt(s.stream.audioBitrate);
	s.video.outWidth = v["outWidth"].toInt(s.video.outWidth);
	s.video.outHeight = v["outHeight"].toInt(s.video.outHeight);
	s.video.fps = v["fps"].toInt(s.video.fps);
	s.video.encoder = v["encoder"].toString(s.video.encoder);
	s.video.preset = v["preset"].toString(s.video.preset);
	const QJsonObject tr = o["transition"].toObject();
	s.transition.type = tr["type"].toString(s.transition.type);
	s.transition.durationMs = tr["durationMs"].toInt(s.transition.durationMs);
	s.record.folder = r["folder"].toString();
	s.record.format = r["format"].toString(s.record.format);
	s.record.quality = r["quality"].toString(s.record.quality);
	s.record.replaySeconds = r["replaySeconds"].toInt(s.record.replaySeconds);
	s.hotkeys = o["hotkeys"].toObject().toVariantMap();
	return s;
}

void AppSettings::save() const
{
	QDir().mkpath(QFileInfo(settingsPath()).absolutePath());
	const QJsonObject o{
		{"stream", QJsonObject{{"service", stream.service}, {"server", stream.server}, {"customUrl", stream.customUrl},
				       {"keyProtected", protect(stream.key)}, {"bitrate", stream.bitrate}, {"audioBitrate", stream.audioBitrate}}},
		{"video", QJsonObject{{"outWidth", video.outWidth}, {"outHeight", video.outHeight}, {"fps", video.fps}, {"encoder", video.encoder}, {"preset", video.preset}}},
		{"record", QJsonObject{{"folder", record.folder}, {"format", record.format}, {"quality", record.quality}, {"replaySeconds", record.replaySeconds}}},
		{"hotkeys", QJsonObject::fromVariantMap(hotkeys)},
		{"transition", QJsonObject{{"type", transition.type}, {"durationMs", transition.durationMs}}},
	};
	QFile f(settingsPath());
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		f.write(QJsonDocument(o).toJson());
}

QVariantMap AppSettings::toMap() const
{
	return {{"service", stream.service}, {"server", stream.server}, {"customUrl", stream.customUrl}, {"hasKey", !stream.key.isEmpty()},
		{"bitrate", stream.bitrate}, {"audioBitrate", stream.audioBitrate}, {"outWidth", video.outWidth}, {"outHeight", video.outHeight},
		{"fps", video.fps}, {"encoder", video.encoder}, {"preset", video.preset}, {"recordFolder", recordFolder()},
		{"recordFormat", record.format}, {"recordQuality", record.quality},
		{"transition", transition.type}, {"transitionMs", transition.durationMs}, {"replaySeconds", record.replaySeconds}, {"hotkeys", hotkeys}};
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
