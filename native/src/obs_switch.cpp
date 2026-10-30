#include "obs_switch.h"
#include "model/obs_import.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <cmath>

namespace obs_switch {

namespace {

// OBS's own plugins, which LOBS either ships itself or leaves out on purpose (OBS's interface
// tools, VST, websocket, VLC, capture cards): never imported as someone's third-party plugin.
const QSet<QString> kObsOwn = {
	"aja", "aja-output-ui", "coreaudio-encoder", "decklink", "decklink-captions", "decklink-output-ui", "frontend-tools",
	"image-source", "nv-filters", "obs-browser", "obs-ffmpeg", "obs-filters", "obs-nvenc", "obs-outputs", "obs-qsv11",
	"obs-text", "obs-transitions", "obs-vst", "obs-webrtc", "obs-websocket", "obs-x264", "rtmp-services", "text-freetype2",
	"vlc-video", "win-capture", "win-dshow", "win-wasapi", "obs-libfdk", "chrome_elf", "libcef", "libEGL", "libGLESv2",
	"lobs", // LOBS's own OBS plugin
};

QJsonObject readJson(const QString &path)
{
	QFile f(path);
	return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
}

// OBS's encoder names (Simple mode's short ones, or Advanced mode's ids) as LOBS's.
QString encoder(const QString &obs)
{
	const QString e = obs.toLower();
	if (e.contains("nvenc") && !e.contains("hevc") && !e.contains("av1")) return "obs_nvenc_h264_tex";
	if (e == "x264" || e == "obs_x264") return "obs_x264";
	if (e == "qsv" || e.startsWith("obs_qsv11") && !e.contains("hevc") && !e.contains("av1")) return "obs_qsv11_v2";
	if (e == "amd" || e.contains("amf") && !e.contains("hevc") && !e.contains("av1")) return "h264_texture_amf";
	return "auto";
}

} // namespace

QString obsFolder()
{
#ifdef Q_OS_WIN
	const QString d = qEnvironmentVariable("APPDATA") + "/obs-studio";
#else
	const QString d = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/obs-studio";
#endif
	return QDir(d + "/basic").exists() ? d : QString();
}

QVariantMap scan(const QString &folder)
{
	if (folder.isEmpty() || !QDir(folder + "/basic").exists())
		return {{"found", false}};
	QVariantList profiles, collections, plugins;
	for (const QFileInfo &fi : QDir(folder + "/basic/profiles").entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time)) {
		const QSettings ini(fi.filePath() + "/basic.ini", QSettings::IniFormat);
		profiles << QVariantMap{{"name", ini.value("General/Name", fi.fileName()).toString()}, {"dir", fi.filePath()}};
	}
	for (const QFileInfo &fi : QDir(folder + "/basic/scenes").entryInfoList({"*.json"}, QDir::Files, QDir::Time)) {
		const QJsonObject o = readJson(fi.filePath());
		if (lobs::isObsCollection(o))
			collections << QVariantMap{{"name", o.value("name").toString(fi.completeBaseName())}, {"path", fi.filePath()}};
	}
	// The ones OBS used last (user.ini since OBS 31, global.ini before).
	QSettings user(QFile::exists(folder + "/user.ini") ? folder + "/user.ini" : folder + "/global.ini", QSettings::IniFormat);
	const QString lastProfile = folder + "/basic/profiles/" + user.value("Basic/ProfileDir").toString();
	const QString lastCollection = folder + "/basic/scenes/" + user.value("Basic/SceneCollectionFile").toString();

	// Third-party plugins: OBS's per-machine plugin folder (current plugins install there), and
	// OBS's own install folder (older plugins copied their files in next to OBS's).
#ifdef Q_OS_WIN
	for (const QFileInfo &fi : QDir("C:/ProgramData/obs-studio/plugins").entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
		if (!kObsOwn.contains(fi.fileName()))
			plugins << QVariantMap{{"name", fi.fileName()}, {"path", fi.filePath()}};
	const QString install = qEnvironmentVariable("ProgramFiles", "C:/Program Files") + "/obs-studio";
	for (const QFileInfo &fi : QDir(install + "/obs-plugins/64bit").entryInfoList({"*.dll"}, QDir::Files)) {
		const QString name = fi.completeBaseName();
		if (!kObsOwn.contains(name) && QDir(install + "/data/obs-plugins/" + name).exists()) // no data folder: a library, not a plugin
			plugins << QVariantMap{{"name", name}, {"path", fi.filePath()}, {"installed", install}};
	}
#endif
	return {{"found", true}, {"folder", folder}, {"profiles", profiles}, {"collections", collections}, {"plugins", plugins},
		{"lastProfile", QDir(lastProfile).exists() ? QDir::cleanPath(lastProfile) : QString()},
		{"lastCollection", QFile::exists(lastCollection) ? QDir::cleanPath(lastCollection) : QString()}};
}

QVariantMap readProfile(const QString &dir)
{
	QVariantMap m;
	QStringList notes;
	const QSettings ini(dir + "/basic.ini", QSettings::IniFormat);

	// Canvas: the output size and frame rate (OBS's common, whole-number or fraction kinds).
	m["outWidth"] = ini.value("Video/OutputCX", 1920).toInt();
	m["outHeight"] = ini.value("Video/OutputCY", 1080).toInt();
	const int fpsType = ini.value("Video/FPSType", 0).toInt();
	double fps = fpsType == 1 ? ini.value("Video/FPSInt", 30).toDouble()
		   : fpsType == 2 ? ini.value("Video/FPSNum", 30).toDouble() / std::max(1.0, ini.value("Video/FPSDen", 1).toDouble())
				  : ini.value("Video/FPSCommon", "30").toString().toDouble();
	if (std::abs(fps - std::round(fps)) > 0.01)
		notes << QString("Frame rate %1 is %2 in LOBS (whole numbers only).").arg(fps).arg(std::lround(fps));
	m["fps"] = int(std::lround(fps));
	if (ini.value("Video/BaseCX").toInt() && (ini.value("Video/BaseCX").toInt() != m["outWidth"].toInt() || ini.value("Video/BaseCY").toInt() != m["outHeight"].toInt()))
		notes << QString("OBS drew at %1×%2 and scaled to %3×%4; LOBS uses %3×%4.").arg(ini.value("Video/BaseCX").toInt()).arg(ini.value("Video/BaseCY").toInt()).arg(m["outWidth"].toInt()).arg(m["outHeight"].toInt());

	// Stream: where, the key, and Enhanced Broadcasting.
	const QJsonObject svc = readJson(dir + "/service.json");
	const QJsonObject ss = svc.value("settings").toObject();
	if (svc.value("type").toString() == "rtmp_custom") {
		m["service"] = "Custom";
		m["customUrl"] = ss.value("server").toString();
	} else if (!ss.value("service").toString().isEmpty()) {
		m["service"] = ss.value("service").toString();
		m["server"] = ss.value("server").toString("auto");
	}
	if (!ss.value("key").toString().isEmpty())
		m["key"] = ss.value("key").toString();
	m["enhanced"] = ini.value("Stream1/EnableMultitrackVideo", true).toBool();

	// Encoder and bitrate: Simple mode's, or Advanced mode's (its encoder settings file).
	const bool advanced = ini.value("Output/Mode").toString() == "Advanced";
	if (advanced) {
		m["encoder"] = encoder(ini.value("AdvOut/Encoder").toString());
		const QJsonObject enc = readJson(dir + "/streamEncoder.json");
		if (enc.contains("bitrate"))
			m["bitrate"] = enc.value("bitrate").toInt();
		m["audioBitrate"] = ini.value("AdvOut/Track1Bitrate", 160).toInt();
	} else {
		m["encoder"] = encoder(ini.value("SimpleOutput/StreamEncoder").toString());
		m["bitrate"] = ini.value("SimpleOutput/VBitrate", 6000).toInt();
		m["audioBitrate"] = ini.value("SimpleOutput/ABitrate", 160).toInt();
	}
	if (m["encoder"] == "auto" && !ini.value(advanced ? "AdvOut/Encoder" : "SimpleOutput/StreamEncoder").toString().isEmpty())
		notes << "OBS's encoder (" + ini.value(advanced ? "AdvOut/Encoder" : "SimpleOutput/StreamEncoder").toString() + ") isn't one LOBS offers; LOBS picks the best one here.";

	// Recording: the folder and format.
	const QString recDir = ini.value(advanced ? "AdvOut/RecFilePath" : "SimpleOutput/FilePath").toString();
	if (!recDir.isEmpty())
		m["recordFolder"] = recDir;
	const QString fmt = ini.value(advanced ? "AdvOut/RecFormat2" : "SimpleOutput/RecFormat2").toString();
	m["recordFormat"] = fmt.contains("mkv") ? "mkv" : fmt.contains("mov") ? "mov" : "mp4"; // hybrid_mp4 too

	m["_notes"] = notes;
	return m;
}

} // namespace obs_switch
