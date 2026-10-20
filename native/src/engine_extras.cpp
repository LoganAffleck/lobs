// Engine extras: live stats (status bar), replay buffer, virtual camera and push-to-talk.
#include "engine.h"

#include <obs.h>
#include <util/platform.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QStandardPaths>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

static obs_data_t *toData(const QJsonObject &o)
{
	return obs_data_create_from_json(QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
}

static QString hms(qint64 secs)
{
	return QString("%1:%2:%3").arg(secs / 3600, 2, 10, QChar('0')).arg((secs / 60) % 60, 2, 10, QChar('0')).arg(secs % 60, 2, 10, QChar('0'));
}

// ---------- live stats ----------

void Engine::pollLiveStats()
{
	if (!m_cpu)
		m_cpu = os_cpu_usage_info_start();
	video_t *video = obs_get_video();
	const uint32_t lagged = obs_get_lagged_frames(), rendered = obs_get_total_frames();
	const uint32_t skipped = video ? video_output_get_skipped_frames(video) : 0, encoded = video ? video_output_get_total_frames(video) : 0;
	QVariantMap s{
		{"fps", obs_get_active_fps()},
		{"targetFps", m_settings.video.fps},
		{"renderMs", double(obs_get_average_frame_time_ns()) / 1e6},
		{"lagged", lagged},
		{"rendered", rendered},
		{"laggedPct", rendered ? 100.0 * lagged / rendered : 0.0},
		{"skipped", skipped},
		{"encoded", encoded},
		{"skippedPct", encoded ? 100.0 * skipped / encoded : 0.0},
		{"cpu", os_cpu_usage_info_query(m_cpu)},
		{"memoryMb", double(os_get_proc_resident_size()) / (1024.0 * 1024.0)},
	};
	// The warning uses the last 10 s, so a startup hiccup doesn't read as an ongoing problem;
	// the totals above stay for the stats popup.
	if (!m_frameClock.isValid())
		m_frameClock.start();
	const qint64 now = m_frameClock.elapsed();
	m_frameSamples.push_back({now, lagged, rendered, skipped, encoded});
	while (m_frameSamples.size() > 2 && now - m_frameSamples[1].ms >= 10000)
		m_frameSamples.removeFirst();
	const FrameSample &old = m_frameSamples.first();
	const auto pct = [](uint32_t part, uint32_t all) { return all ? 100.0 * part / all : 0.0; };
	s["recentLaggedPct"] = pct(lagged - old.lagged, rendered - old.rendered);
	s["recentSkippedPct"] = encoded >= old.encoded ? pct(skipped - old.skipped, encoded - old.encoded) : 0.0;
	const QString recDir = m_settings.recordFolder();
	QDir().mkpath(recDir);
	s["diskFreeGb"] = double(os_get_free_disk_space(recDir.toUtf8().constData())) / (1024.0 * 1024.0 * 1024.0);
	if (m_recording && m_recOutput) {
		s["recTime"] = hms(m_recClock.elapsed() / 1000);
		s["recMb"] = double(obs_output_get_total_bytes(m_recOutput)) / (1024.0 * 1024.0);
		s["recDropped"] = obs_output_get_frames_dropped(m_recOutput);
	}
	s["replay"] = replayActive();
	s["vcam"] = vcamActive();
	m_liveStats = s;
}

// ---------- replay buffer ----------

static void onReplaySaved(void *data, calldata_t *)
{
	auto *self = static_cast<Engine *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->replaySavedNow(); }, Qt::QueuedConnection);
}

static void onReplayStop(void *data, calldata_t *)
{
	auto *self = static_cast<Engine *>(data);
	QMetaObject::invokeMethod(self, [self]() { emit self->replayChanged(); }, Qt::QueuedConnection);
}

bool Engine::replayActive() const { return m_replayOutput && obs_output_active(m_replayOutput); }

bool Engine::startReplay()
{
	if (!m_started || replayActive())
		return false;
	m_lastError.clear();
	const QString dir = m_settings.recordFolder();
	QDir().mkpath(dir);
	// Its own encoders (recording quality), so it runs whether or not you stream or record.
	obs_encoder_release(m_replayVenc);
	obs_encoder_release(m_replayAenc);
	m_replayVenc = makeVideoEncoder(resolveEncoder(), true, "LOBS replay video");
	obs_data_t *as = toData({{"bitrate", 192}});
	m_replayAenc = obs_audio_encoder_create("ffmpeg_aac", "LOBS replay audio", as, 0, nullptr);
	obs_data_release(as);
	obs_encoder_set_audio(m_replayAenc, obs_get_audio());
	if (!m_replayVenc || !m_replayAenc) {
		m_lastError = "Couldn't create encoders for the replay buffer";
		return false;
	}
	if (m_replayOutput)
		obs_output_release(m_replayOutput);
	obs_data_t *os = toData({{"directory", QDir::toNativeSeparators(dir)}, {"format", "Replay %CCYY-%MM-%DD %hh-%mm-%ss"},
				 {"extension", m_settings.record.format}, {"allow_spaces", true},
				 {"max_time_sec", m_settings.record.replaySeconds}, {"max_size_mb", 2048}});
	m_replayOutput = obs_output_create("replay_buffer", "LOBS replay buffer", os, nullptr);
	obs_data_release(os);
	if (!m_replayOutput) {
		m_lastError = "The replay buffer isn't available in this engine";
		return false;
	}
	obs_output_set_video_encoder(m_replayOutput, m_replayVenc);
	obs_output_set_audio_encoder(m_replayOutput, m_replayAenc, 0);
	signal_handler_t *sh = obs_output_get_signal_handler(m_replayOutput);
	signal_handler_connect(sh, "saved", onReplaySaved, this);
	signal_handler_connect(sh, "stop", onReplayStop, this);
	if (!obs_output_start(m_replayOutput)) {
		const char *e = obs_output_get_last_error(m_replayOutput);
		m_lastError = QString("Replay buffer failed to start%1").arg(e ? QString(": ") + e : QString());
		return false;
	}
	emit replayChanged();
	setStatus(QString("Replay buffer on · keeps the last %1 s").arg(m_settings.record.replaySeconds));
	return true;
}

void Engine::stopReplay()
{
	if (!replayActive())
		return;
	obs_output_stop(m_replayOutput);
	emit replayChanged();
	setStatus("Replay buffer off");
}

void Engine::saveReplay()
{
	if (!replayActive())
		return;
	proc_handler_t *ph = obs_output_get_proc_handler(m_replayOutput);
	calldata_t cd = {};
	proc_handler_call(ph, "save", &cd);
	calldata_free(&cd);
}

void Engine::replaySavedNow()
{
	QString path;
	if (m_replayOutput) {
		calldata_t cd = {};
		proc_handler_call(obs_output_get_proc_handler(m_replayOutput), "get_last_replay", &cd);
		path = QString::fromUtf8(calldata_string(&cd, "path"));
		calldata_free(&cd);
	}
	m_lastReplay = path;
	setStatus("Replay saved: " + QDir::toNativeSeparators(path));
	emit replaySaved(path);
}

// ---------- virtual camera ----------
// The camera itself is OBS's DirectShow driver (CLSID below), registered system-wide. If OBS
// already registered it, LOBS uses that; otherwise LOBS registers its own copy (one-time,
// needs admin) from a stable folder, so engine updates don't move it.

static const wchar_t *kVcamClsid = L"CLSID\\{A3FCE0F5-3493-419F-958A-ABA1250EC20B}\\InprocServer32";

static QString registeredVcamPath()
{
	wchar_t buf[MAX_PATH] = {};
	DWORD len = sizeof(buf);
	if (RegGetValueW(HKEY_CLASSES_ROOT, kVcamClsid, nullptr, RRF_RT_REG_SZ, nullptr, buf, &len) != ERROR_SUCCESS)
		return {};
	return QString::fromWCharArray(buf);
}

bool Engine::vcamAvailable() const
{
	const QString p = registeredVcamPath();
	return !p.isEmpty() && QFile::exists(p);
}

bool Engine::vcamActive() const { return m_vcamOutput && obs_output_active(m_vcamOutput); }

bool Engine::installVirtualCam()
{
	if (vcamAvailable())
		return true;
	// Copy the driver out of the engine folder (it changes with engine updates).
	QString srcDir;
	for (const QString &d : {m_root + "/data/obs-plugins/win-dshow", m_root + "/core/win-dshow/data"})
		if (QFile::exists(d + "/obs-virtualcam-module64.dll"))
			srcDir = d;
	if (srcDir.isEmpty()) {
		m_lastError = "This engine doesn't include the virtual camera driver";
		return false;
	}
	const QString dest = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/virtualcam";
	QDir().mkpath(dest);
	for (const char *f : {"obs-virtualcam-module64.dll", "obs-virtualcam-module32.dll"}) {
		QFile::remove(dest + "/" + f);
		QFile::copy(srcDir + "/" + f, dest + "/" + f);
	}
	// regsvr32 for both, elevated (Windows asks for permission once).
	const QString args = QString("/c regsvr32 /s \"%1\" && regsvr32 /s \"%2\"")
				     .arg(QDir::toNativeSeparators(dest + "/obs-virtualcam-module64.dll"), QDir::toNativeSeparators(dest + "/obs-virtualcam-module32.dll"));
	const std::wstring wargs = args.toStdWString();
	SHELLEXECUTEINFOW sei = {sizeof(sei)};
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = L"runas";
	sei.lpFile = L"cmd.exe";
	sei.lpParameters = wargs.c_str();
	sei.nShow = SW_HIDE;
	if (!ShellExecuteExW(&sei)) {
		m_lastError = "Virtual camera setup was cancelled";
		return false;
	}
	WaitForSingleObject(sei.hProcess, 30000);
	CloseHandle(sei.hProcess);
	if (!vcamAvailable()) {
		m_lastError = "Windows didn't register the virtual camera";
		return false;
	}
	return true;
}

bool Engine::startVirtualCam()
{
	if (!m_started || vcamActive())
		return false;
	m_lastError.clear();
	if (!vcamAvailable() && !installVirtualCam())
		return false;
	if (!m_vcamOutput)
		m_vcamOutput = obs_output_create("virtualcam_output", "LOBS virtual camera", nullptr, nullptr);
	if (!m_vcamOutput) {
		m_lastError = "The virtual camera isn't available in this engine";
		return false;
	}
	if (!obs_output_start(m_vcamOutput)) {
		const char *e = obs_output_get_last_error(m_vcamOutput);
		m_lastError = QString("Virtual camera failed to start%1").arg(e ? QString(": ") + e : QString());
		return false;
	}
	emit vcamChanged();
	setStatus("Virtual camera on");
	return true;
}

void Engine::stopVirtualCam()
{
	if (!vcamActive())
		return;
	obs_output_stop(m_vcamOutput);
	emit vcamChanged();
	setStatus("Virtual camera off");
}

// ---------- push-to-talk ----------

void Engine::setPushToTalk(const QSet<QString> &sourceIds)
{
	m_pttBound = sourceIds;
	for (auto it = m_pttHeld.begin(); it != m_pttHeld.end();)
		it = m_pttBound.contains(*it) ? std::next(it) : m_pttHeld.erase(it);
	applyMutes();
}

void Engine::pushToTalk(const QString &sourceId, bool held)
{
	if (!m_pttBound.contains(sourceId))
		return;
	held ? (void)m_pttHeld.insert(sourceId) : (void)m_pttHeld.remove(sourceId);
	applyMutes();
}

void Engine::applyMutes()
{
	// A push-to-talk source is muted unless its key is held (and the show doesn't mute it).
	for (auto it = m_docMuted.begin(); it != m_docMuted.end(); ++it)
		if (obs_source_t *in = m_inputs.value(it.key()))
			obs_source_set_muted(in, it.value() || (m_pttBound.contains(it.key()) && !m_pttHeld.contains(it.key())));
}
