// App settings (not part of a show): streaming, encoder, video and recording.
// Saved to %APPDATA%/LOBS/LOBS/settings.json (~/Library/Application Support/LOBS/LOBS on macOS).
// The stream key is encrypted with Windows DPAPI (CryptProtectData), so only this Windows user
// on this PC can read it back; on macOS it lives in the login Keychain instead.
#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

struct StreamSettings {
	QString service = "Twitch"; // a name from rtmp-services, or "Custom"
	QString server = "auto";
	QString customUrl;
	QString key;
	int bitrate = 6000; // kbps
	int audioBitrate = 160;
	// Twitch's Enhanced Broadcasting: Twitch picks several qualities and the GPU encodes them all.
	bool enhanced = true;
};

struct VideoSettings {
	int outWidth = 1920, outHeight = 1080;
	int fps = 60;
	QString encoder = "auto"; // auto | obs_nvenc_h264_tex | obs_qsv11_v2 | h264_texture_amf | com.apple.videotoolbox.… | obs_x264
	QString preset = "quality"; // speed | balanced | quality (mapped per encoder)
};

struct RecordSettings {
	QString folder; // empty = Videos/LOBS
	QString format = "mp4"; // mp4 | mkv | mov
	QString quality = "high"; // stream (reuse stream encoder) | high | indistinguishable
	int replaySeconds = 30;   // replay buffer length
	bool remux = true;        // MKV: also make an MP4 copy when it stops (OBS's automatic remux)
};

struct TransitionSettings {
	QString type = "fade"; // cut | fade | slide | swipe | fade_to_black
	int durationMs = 300;
};

struct AppSettings {
	TransitionSettings transition;
	QVariantMap hotkeys; // action -> key combo ("Ctrl+Shift+F1"); see hotkeys.h
	StreamSettings stream;
	VideoSettings video;
	RecordSettings record;
	bool liveLock = true; // while streaming or recording, layers on air need Alt or Unlock to change
	QString profile = "Default"; // the profile the stream, video and record settings belong to

	static AppSettings load();
	void save() const;
	// The profile part (stream, video, record), as profiles/<name>.json holds it.
	QJsonObject profileJson() const;
	void applyProfile(const QJsonObject &o);
	static QString profilesFolder();
	QVariantMap toMap() const; // for QML (stream key never included, only whether one is set)
	void applyMap(const QVariantMap &m);
	QString recordFolder() const;
	// Where recordings go when no folder is set. Self-tests point it at the temp folder (LOBS-003).
	static QString defaultRecordFolder();
	static QString &defaultRecordFolderOverride();
};
