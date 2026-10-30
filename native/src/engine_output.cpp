// Engine outputs: encoders, streaming (rtmp-services or custom RTMP), recording, video
// resets and audio meters.
#include "engine.h"

#include <obs.h>

#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>

// OBS's last error for an output, as ": message" (or ""), in current Windows wording (LOBS-035).
QString obsOutputError(obs_output_t *out)
{
	const char *e = obs_output_get_last_error(out);
	if (!e || !*e)
		return {};
	QString s = QString::fromUtf8(e);
	static const QRegularExpression ransom(QStringLiteral("If Windows 10 Ransomware Protection is enabled it can also cause this error\\.[^\n]*"));
	s.replace(ransom, QString("Controlled folder access in Windows Security may also block it. Allow LOBS there or choose another folder."));
	return ": " + s.trimmed();
}

static obs_data_t *toData(const QJsonObject &o)
{
	return obs_data_create_from_json(QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
}

// ---------- encoders ----------

// Best first. Each is only offered if its module registered it (NVENC/AMF/QSV check the GPU at load).
static const char *kVideoEncoders[][2] = {
	{"obs_nvenc_h264_tex", "NVIDIA NVENC H.264"},
	{"h264_texture_amf", "AMD AMF H.264"},
	{"obs_qsv11_v2", "Intel Quick Sync H.264"},
	{"com.apple.videotoolbox.videoencoder.ave.avc", "Apple VideoToolbox H.264"}, // Apple silicon
	{"com.apple.videotoolbox.videoencoder.h264.gva", "Apple VideoToolbox H.264"}, // Intel Macs
	{"obs_x264", "x264 (software)"},
};

static bool encoderRegistered(const char *id)
{
	const char *t = nullptr;
	for (size_t i = 0; obs_enum_encoder_types(i, &t); i++)
		if (strcmp(t, id) == 0)
			return true;
	return false;
}

QVariantList Engine::encoders() const
{
	QVariantList out;
	for (auto &e : kVideoEncoders)
		if (encoderRegistered(e[0]))
			out << QVariantMap{{"value", e[0]}, {"name", e[1]}};
	return out;
}

QString Engine::resolveEncoder() const
{
	const QString want = m_settings.video.encoder;
	if (want != "auto" && encoderRegistered(want.toUtf8().constData()))
		return want;
	for (auto &e : kVideoEncoders)
		if (encoderRegistered(e[0]))
			return e[0];
	return "obs_x264";
}

QString Engine::resolveRecordEncoder() const
{
	// About half the file size of H.264 at the same quality, and MP4, MKV and MOV all hold it.
	// Recordings only: streams stay H.264 (what services take), and so do replays, which get shared.
	static const char *hevc = "com.apple.videotoolbox.videoencoder.ave.hevc";
	const QString enc = resolveEncoder();
	return enc.startsWith("com.apple.videotoolbox") && encoderRegistered(hevc) ? QString(hevc) : enc;
}

obs_encoder *Engine::makeVideoEncoder(const QString &id, bool forRecording, const char *name)
{
	const QString preset = m_settings.video.preset;
	const QString q = m_settings.record.quality;
	QJsonObject s{{"keyint_sec", 2}};
	const bool cbr = !forRecording;
	if (id == "obs_x264") {
		s["preset"] = preset == "speed" ? "ultrafast" : preset == "balanced" ? "veryfast" : "faster";
		if (cbr) s["rate_control"] = "CBR", s["bitrate"] = m_settings.stream.bitrate;
		else s["rate_control"] = "CRF", s["crf"] = q == "indistinguishable" ? 16 : 20;
	} else if (id == "obs_nvenc_h264_tex") {
		// OBS 32's NVENC reads "preset" (the old "preset2" is ignored). The rest is what Twitch's
		// Enhanced Broadcasting asks OBS for on its top quality: two B-frames, lookahead, adaptive
		// quantization, profile high.
		s["preset"] = preset == "speed" ? "p2" : preset == "balanced" ? "p4" : "p6";
		s["tune"] = "hq";
		s["multipass"] = "qres";
		s["profile"] = "high";
		s["bf"] = 2;
		s["lookahead"] = true;
		s["adaptive_quantization"] = true;
		if (cbr) s["rate_control"] = "CBR", s["bitrate"] = m_settings.stream.bitrate;
		else s["rate_control"] = "CQP", s["cqp"] = q == "indistinguishable" ? 16 : 20;
	} else if (id == "h264_texture_amf") {
		s["preset"] = preset == "speed" ? "speed" : preset == "balanced" ? "balanced" : "quality";
		if (cbr) s["rate_control"] = "CBR", s["bitrate"] = m_settings.stream.bitrate;
		else s["rate_control"] = "CQP", s["cqp"] = q == "indistinguishable" ? 16 : 20;
	} else if (id.startsWith("com.apple.videotoolbox")) {
		// No speed presets: the hardware sets the pace. Quality 0-100, OBS's default is 60.
		s["profile"] = id.endsWith(".hevc") ? "main" : "high";
		if (cbr) s["rate_control"] = "CBR", s["bitrate"] = m_settings.stream.bitrate;
		else s["rate_control"] = "CRF", s["quality"] = q == "indistinguishable" ? 80 : 65;
	} else { // Quick Sync
		s["target_usage"] = preset == "speed" ? "TU7" : preset == "balanced" ? "TU4" : "TU2";
		s["rate_control"] = "CBR";
		s["bitrate"] = cbr ? m_settings.stream.bitrate : (q == "indistinguishable" ? 80000 : 40000);
	}
	obs_data_t *d = toData(s);
	obs_encoder_t *enc = obs_video_encoder_create(id.toUtf8().constData(), name, d, nullptr);
	obs_data_release(d);
	if (enc)
		obs_encoder_set_video(enc, obs_get_video());
	return enc;
}

// ---------- services ----------

QStringList Engine::services() const
{
	QStringList out;
	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	if (obs_property_t *p = props ? obs_properties_get(props, "service") : nullptr)
		for (size_t i = 0, n = obs_property_list_item_count(p); i < n; i++)
			out << QString::fromUtf8(obs_property_list_item_string(p, i));
	obs_properties_destroy(props);
	return out;
}

QVariantList Engine::servers(const QString &service) const
{
	QVariantList out;
	obs_data_t *d = toData({{"service", service}});
	obs_service_t *svc = obs_service_create_private("rtmp_common", "probe", d);
	obs_properties_t *props = obs_service_properties(svc);
	if (obs_property_t *p = props ? obs_properties_get(props, "service") : nullptr)
		obs_property_modified(p, d); // fills the server list for this service
	if (obs_property_t *p = props ? obs_properties_get(props, "server") : nullptr)
		for (size_t i = 0, n = obs_property_list_item_count(p); i < n; i++)
			out << QVariantMap{{"name", QString::fromUtf8(obs_property_list_item_name(p, i))}, {"value", QString::fromUtf8(obs_property_list_item_string(p, i))}};
	obs_properties_destroy(props);
	obs_service_release(svc);
	obs_data_release(d);
	return out;
}

// ---------- video ----------

bool Engine::resetVideo()
{
	if (live())
		return false;
	obs_video_info ovi = {};
	ovi.graphics_module = "libobs-d3d11.dll";
	ovi.fps_num = uint32_t(m_settings.video.fps);
	ovi.fps_den = 1;
	ovi.base_width = uint32_t(m_canvasW);
	ovi.base_height = uint32_t(m_canvasH);
	ovi.output_width = uint32_t(m_settings.video.outWidth);
	ovi.output_height = uint32_t(m_settings.video.outHeight);
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.gpu_conversion = true;
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.scale_type = OBS_SCALE_LANCZOS;
	// Encoders are bound to the old video output; drop them so they're rebuilt.
	releaseOutputs();
	const int r = obs_reset_video(&ovi);
	if (r != OBS_VIDEO_SUCCESS) {
		m_lastError = QString("Video reset failed (%1)").arg(r);
		return false;
	}
	return true;
}

// ---------- streaming ----------

static void onStreamStop(void *data, calldata_t *cd)
{
	auto *self = static_cast<Engine *>(data);
	const int code = int(calldata_int(cd, "code"));
	const char *err = calldata_string(cd, "last_error");
	const QString msg = err && *err ? QString::fromUtf8(err) : QString();
	QMetaObject::invokeMethod(self, [self, code, msg]() {
		// It was reconnecting and ran out of tries: say so, rather than just "disconnected".
		const int tries = self->reconnectAttempt();
		self->stopStreaming();
		if (tries > 0 && code != OBS_OUTPUT_SUCCESS) {
			emit self->streamError("Couldn't reconnect. The stream ended.");
			return;
		}
		const QString why = code == OBS_OUTPUT_BAD_PATH ? "Bad server address"
				    : code == OBS_OUTPUT_CONNECT_FAILED ? "Couldn't connect to the server"
				    : code == OBS_OUTPUT_INVALID_STREAM ? "Server rejected the stream key"
				    : code == OBS_OUTPUT_DISCONNECTED ? "Disconnected from the server"
				    : code == OBS_OUTPUT_ENCODE_ERROR ? "Encoder error"
				    : code == OBS_OUTPUT_SUCCESS ? QString() : QString("Stream stopped (%1)").arg(code);
		if (!why.isEmpty())
			emit self->streamError(why + (msg.isEmpty() ? QString() : ": " + msg));
	}, Qt::QueuedConnection);
}

// The connection dropped: OBS's engine waits timeout_sec, then tries again (the wait grows).
static void onStreamReconnect(void *data, calldata_t *cd)
{
	auto *self = static_cast<Engine *>(data);
	const int wait = int(calldata_int(cd, "timeout_sec"));
	QMetaObject::invokeMethod(self, [self, wait]() { self->onReconnecting(wait); }, Qt::QueuedConnection);
}

static void onStreamReconnected(void *data, calldata_t *)
{
	auto *self = static_cast<Engine *>(data);
	QMetaObject::invokeMethod(self, [self]() { self->onReconnected(); }, Qt::QueuedConnection);
}

void Engine::onReconnecting(int waitSec)
{
	if (m_reconnectAttempt++ == 0)
		blog(LOG_WARNING, "[lobs] stream connection dropped; reconnecting");
	m_reconnectAt = m_streamClock.elapsed() + qint64(waitSec) * 1000;
	pollStreamStats();
}

void Engine::onReconnected()
{
	const int tries = m_reconnectAttempt;
	m_reconnectAttempt = 0;
	blog(LOG_INFO, "[lobs] stream reconnected after %d tries", tries);
	emit streamReconnected(tries);
	pollStreamStats();
}

// Twitch's names for the GPU scaling OBS applies per quality.
static obs_scale_type scaleType(const QString &name)
{
	return name == "OBS_SCALE_BICUBIC" ? OBS_SCALE_BICUBIC : name == "OBS_SCALE_LANCZOS" ? OBS_SCALE_LANCZOS
	     : name == "OBS_SCALE_AREA" ? OBS_SCALE_AREA : name == "OBS_SCALE_POINT" ? OBS_SCALE_POINT : OBS_SCALE_BILINEAR;
}

void Engine::releaseStreamEncoders()
{
	obs_encoder_release(m_streamVenc), m_streamVenc = nullptr;
	obs_encoder_release(m_streamAenc), m_streamAenc = nullptr;
	for (obs_encoder_t *e : m_extraVencs)
		obs_encoder_release(e);
	m_extraVencs.clear();
	if (m_vencGroup) // after its encoders are gone
		obs_encoder_group_destroy(m_vencGroup), m_vencGroup = nullptr;
}

bool Engine::startStreaming(bool test, const QJsonObject &plan)
{
	if (!m_started || m_streaming)
		return false;
	m_lastError.clear();
	const StreamSettings &st = m_settings.stream;
	if (st.key.isEmpty() && st.service != "Custom") {
		m_lastError = "Add your stream key in Settings first";
		return false;
	}
	if (test && !st.service.startsWith("Twitch")) {
		m_lastError = "Test streams work with Twitch only";
		return false;
	}
	const bool multi = !plan.isEmpty();

	// The old output goes first: it still points at the old encoders (and service), and releasing
	// those under it freed memory the output then touched (heap corruption on the next start).
	if (m_streamOutput) {
		obs_output_release(m_streamOutput);
		m_streamOutput = nullptr;
	}
	if (m_service)
		obs_service_release(m_service);
	releaseStreamEncoders();
	m_qualities.clear();

	if (multi) {
		// Enhanced Broadcasting: Twitch's address and key. OBS streams over the RTMP entry.
		QJsonObject ep;
		for (const QJsonValue &v : plan.value("ingest_endpoints").toArray()) {
			const QJsonObject e = v.toObject();
			if (e.value("protocol").toString() == "RTMP" || (ep.isEmpty() && e.value("protocol").toString() == "RTMPS"))
				ep = e;
		}
		QString key = ep.value("authentication").toString();
		if (key.isEmpty())
			key = st.key;
		if (test)
			key += "?bandwidthtest=true";
		const QString server = ep.value("url_template").toString().replace("/{stream_key}", "").replace("{stream_key}", "");
		obs_data_t *d = toData({{"server", server}, {"key", key}});
		m_service = obs_service_create("rtmp_custom", "LOBS service", d, nullptr);
		obs_data_release(d);

		// One encoder per quality, grouped so they start together (keyframes line up).
		m_vencGroup = obs_encoder_group_create();
		const QJsonArray cfgs = plan.value("encoder_configurations").toArray();
		for (int i = 0; i < cfgs.size(); i++) {
			const QJsonObject c = cfgs[i].toObject();
			const QString type = c.value("type").toString();
			obs_data_t *es = toData(c.value("settings").toObject());
			obs_encoder_t *enc = obs_video_encoder_create(type.toUtf8().constData(), QString("LOBS stream video %1").arg(i).toUtf8().constData(), es, nullptr);
			obs_data_release(es);
			if (!enc) {
				m_lastError = "Twitch asked for an encoder this PC doesn't have (" + type + ")";
				releaseStreamEncoders();
				return false;
			}
			obs_encoder_set_video(enc, obs_get_video());
			const int w = c.value("width").toInt(), h = c.value("height").toInt();
			if (w && h && (w != m_settings.video.outWidth || h != m_settings.video.outHeight)) {
				obs_encoder_set_scaled_size(enc, uint32_t(w), uint32_t(h));
				obs_encoder_set_gpu_scale_type(enc, scaleType(c.value("gpu_scale_type").toString()));
			}
			const QJsonObject fr = c.value("framerate").toObject();
			const double fps = fr.value("numerator").toDouble() / std::max(1.0, fr.value("denominator").toDouble(1));
			if (fps > 0 && m_settings.video.fps / fps >= 1.5)
				obs_encoder_set_frame_rate_divisor(enc, uint32_t(std::lround(m_settings.video.fps / fps)));
			obs_encoder_set_group(enc, m_vencGroup);
			obs_data_t *got = obs_encoder_get_settings(enc);
			blog(LOG_INFO, "[lobs] stream video %d: %s %dx%d@%g %s", i, qPrintable(type), w, h, fps, obs_data_get_json(got));
			obs_data_release(got);
			m_qualities << QString("%1p%2").arg(h).arg(std::lround(fps));
			if (i == 0)
				m_streamVenc = enc;
			else
				m_extraVencs.push_back(enc);
		}
		const QJsonArray audio = plan.value("audio_configurations").toObject().value("live").toArray();
		const int abr = audio.isEmpty() ? st.audioBitrate : audio[0].toObject().value("settings").toObject().value("bitrate").toInt(st.audioBitrate);
		obs_data_t *as = toData({{"bitrate", abr}});
		m_streamAenc = obs_audio_encoder_create("ffmpeg_aac", "LOBS stream audio", as, 0, nullptr);
		obs_data_release(as);
		obs_encoder_set_audio(m_streamAenc, obs_get_audio());
		blog(LOG_INFO, "[lobs] Enhanced Broadcasting: %d qualities, config %s, to %s", int(cfgs.size()),
		     qPrintable(plan.value("meta").toObject().value("config_id").toString()), qPrintable(server));
	} else {
		if (st.service == "Custom") {
			obs_data_t *d = toData({{"server", st.customUrl}, {"key", st.key}});
			m_service = obs_service_create("rtmp_custom", "LOBS service", d, nullptr);
			obs_data_release(d);
		} else {
			const QString key = test ? st.key + "?bandwidthtest=true" : st.key;
			obs_data_t *d = toData({{"service", st.service}, {"server", st.server}, {"key", key}});
			m_service = obs_service_create("rtmp_common", "LOBS service", d, nullptr);
			obs_data_release(d);
		}
		m_streamVenc = makeVideoEncoder(resolveEncoder(), false, "LOBS stream video");
		if (!m_streamVenc) {
			m_lastError = "Couldn't create the video encoder";
			return false;
		}
		obs_data_t *as = toData({{"bitrate", st.audioBitrate}});
		m_streamAenc = obs_audio_encoder_create("ffmpeg_aac", "LOBS stream audio", as, 0, nullptr);
		obs_data_release(as);
		obs_encoder_set_audio(m_streamAenc, obs_get_audio());

		// Let the service clamp bitrates / keyframes to what the platform allows.
		obs_data_t *vset = obs_encoder_get_settings(m_streamVenc), *aset = obs_encoder_get_settings(m_streamAenc);
		obs_service_apply_encoder_settings(m_service, vset, aset);
		obs_data_set_int(vset, "bitrate", streamBitrate(int(obs_data_get_int(vset, "bitrate"))));
		obs_encoder_update(m_streamVenc, vset);
		obs_encoder_update(m_streamAenc, aset);
		// What the stream really uses, after the service's limits, to compare against OBS's log.
		blog(LOG_INFO, "[lobs] stream video: %s %s", obs_encoder_get_id(m_streamVenc), obs_data_get_json(vset));
		blog(LOG_INFO, "[lobs] stream audio: %s %s", obs_encoder_get_id(m_streamAenc), obs_data_get_json(aset));
		obs_data_release(vset);
		obs_data_release(aset);
	}

	const char *outType = multi ? "rtmp_output" : obs_service_get_preferred_output_type(m_service);
	if (!outType)
		outType = "rtmp_output";
	obs_data_t *od = toData({});
	m_streamOutput = obs_output_create(outType, "LOBS stream", od, nullptr);
	obs_data_release(od);
	obs_output_set_reconnect_settings(m_streamOutput, 25, 2); // OBS's defaults: 25 tries, from 2 s apart
	obs_output_set_service(m_streamOutput, m_service);
	obs_output_set_video_encoder2(m_streamOutput, m_streamVenc, 0);
	for (size_t i = 0; i < m_extraVencs.size(); i++)
		obs_output_set_video_encoder2(m_streamOutput, m_extraVencs[i], i + 1);
	obs_output_set_audio_encoder(m_streamOutput, m_streamAenc, 0);
	signal_handler_t *sh = obs_output_get_signal_handler(m_streamOutput);
	signal_handler_connect(sh, "stop", onStreamStop, this);
	signal_handler_connect(sh, "reconnect", onStreamReconnect, this);
	signal_handler_connect(sh, "reconnect_success", onStreamReconnected, this);

	if (!obs_output_start(m_streamOutput)) {
		m_lastError = "Couldn't start the stream" + obsOutputError(m_streamOutput);
		return false;
	}
	m_streaming = true;
	m_streamClock.start();
	m_lastBytes = 0;
	m_lastBytesAt = 0;
	m_byteHistory.clear();
	m_stallBytes = 0, m_stallSince = 0;
	m_testStream = test;
	m_multitrack = multi;
	m_reconnectAttempt = 0;
	pollStreamStats();
	emit streamingChanged();
	setStatus((test ? "Test stream to " : "Live on ") + (st.service == "Custom" ? st.customUrl : st.service) + " · " +
		  (multi ? "Enhanced Broadcasting, " + m_qualities.join(" · ") : QString(obs_encoder_get_name(m_streamVenc))));
	return true;
}

void Engine::stopStreaming()
{
	if (!m_streaming)
		return;
	m_streaming = false;
	if (m_streamOutput && obs_output_active(m_streamOutput))
		obs_output_stop(m_streamOutput);
	emit streamingChanged();
	setStatus("Stream ended after " + m_streamStats.value("time").toString());
}

// Twitch allows 6,000 kbps of video, and Inspector flags "bitrate too high" when the session's
// average goes over. Constant bitrate still averages ~1.5% over its target (keyframes, spikes): a
// 6,000 target averaged 6,092, while 5,800 measured 5,803. So the setting goes up to 6,000 and
// the encoder aims at most 5,900, which averages just inside it.
int Engine::streamBitrate(int kbps) const
{
	return m_settings.stream.service.startsWith("Twitch") ? std::min(kbps, 5900) : kbps;
}

// A new bitrate while live: the encoders (NVENC, x264, AMF, Quick Sync) take it without restarting.
void Engine::applyStreamBitrate()
{
	if (!m_streamVenc || m_multitrack) // Twitch's plan sets each quality's bitrate
		return;
	obs_data_t *v = obs_encoder_get_settings(m_streamVenc);
	obs_data_set_int(v, "bitrate", streamBitrate(m_settings.stream.bitrate));
	obs_service_apply_encoder_settings(m_service, v, nullptr); // the service's own cap still applies
	obs_data_set_int(v, "bitrate", streamBitrate(int(obs_data_get_int(v, "bitrate"))));
	obs_encoder_update(m_streamVenc, v);
	blog(LOG_INFO, "[lobs] stream bitrate now %lld kbps", obs_data_get_int(v, "bitrate"));
	obs_data_release(v);
}

void Engine::pollStreamStats()
{
	if (!m_streamOutput)
		return;
	const quint64 bytes = obs_output_get_total_bytes(m_streamOutput);
	const qint64 now = m_streamClock.elapsed();
	// Averaged over the last ~4 s: with a keyframe every 2 s, a 1 s window alternates between
	// holding one and not, and the reading seesawed (3,000 / 6,000 around a steady 4,500).
	double kbps = m_streamStats.value("kbps").toDouble();
	if (now - m_lastBytesAt >= 900) {
		m_byteHistory.append({now, bytes});
		while (m_byteHistory.size() > 1 && now - m_byteHistory.first().first > 4100)
			m_byteHistory.removeFirst();
		const auto &[t0, b0] = m_byteHistory.first();
		kbps = now > t0 ? double(bytes - b0) * 8.0 / double(now - t0) : 0;
		m_lastBytes = bytes;
		m_lastBytesAt = now;
	}
	// Nothing sent: the network is gone before OBS's engine notices (it only does once its send
	// buffer fills, several seconds after a cable comes out), so say so meanwhile.
	if (bytes != m_stallBytes)
		m_stallBytes = bytes, m_stallSince = now;
	const int stalledSec = now - m_stallSince >= 3000 ? int((now - m_stallSince) / 1000) : 0;
	const int dropped = obs_output_get_frames_dropped(m_streamOutput);
	const int total = obs_output_get_total_frames(m_streamOutput);
	const double dropPct = total > 0 ? 100.0 * dropped / total : 0;
	const float congestion = obs_output_get_congestion(m_streamOutput);
	const bool reconnecting = obs_output_reconnecting(m_streamOutput);
	const qint64 secs = now / 1000;
	const QString time = QString("%1:%2:%3").arg(secs / 3600, 2, 10, QChar('0')).arg((secs / 60) % 60, 2, 10, QChar('0')).arg(secs % 60, 2, 10, QChar('0'));
	const QString health = reconnecting ? "reconnecting" : (dropPct > 5 || congestion > 0.5f) ? "poor" : (dropPct > 1 || congestion > 0.2f) ? "fair" : "good";
	m_streamStats = {{"time", time}, {"kbps", kbps}, {"dropped", dropped}, {"total", total}, {"dropPct", dropPct},
			 {"congestion", congestion}, {"health", health}, {"reconnecting", reconnecting}, {"bytes", double(bytes)},
			 {"qualities", m_multitrack ? m_qualities.join(" · ") : QString()},
			 // While reconnecting: which try is next, and in how many seconds.
			 {"reconnectAttempt", m_reconnectAttempt},
			 {"stalledSec", reconnecting ? 0 : stalledSec},
			 {"reconnectIn", reconnecting ? int(std::max<qint64>(0, (m_reconnectAt - now + 999) / 1000)) : 0}};
	emit streamStatsChanged();
}

// ---------- recording ----------

static void onRecordStop(void *data, calldata_t *cd)
{
	auto *self = static_cast<Engine *>(data);
	const int code = int(calldata_int(cd, "code"));
	QMetaObject::invokeMethod(self, [self, code]() {
		const QFileInfo fi(self->lastRecording());
		emit self->recordingFinished(code == OBS_OUTPUT_SUCCESS && fi.exists(), fi.absoluteFilePath(), fi.exists() ? fi.size() : 0);
	}, Qt::QueuedConnection);
}

void Engine::recordClip(int seconds)
{
	startRecording();
	if (m_recording)
		m_recordTimer.start(seconds * 1000);
}

void Engine::startRecording()
{
	if (!m_started || m_recording)
		return;
	m_lastError.clear();
	const RecordSettings &rs = m_settings.record;
	const QString dir = m_settings.recordFolder();
	QDir().mkpath(dir);
	m_lastRecording = dir + "/LOBS " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss") + "." + rs.format;

	// "Same as stream" shares the stream encoders while live; otherwise a quality-based encoder.
	// The old output goes first: it still points at the old encoders (and service), and releasing
	// those under it freed memory the output then touched (heap corruption on the next start).
	if (m_recOutput) {
		obs_output_release(m_recOutput);
		m_recOutput = nullptr;
	}
	obs_encoder_t *venc = nullptr, *aenc = nullptr;
	if (rs.quality == "stream" && m_streaming) {
		venc = m_streamVenc;
		aenc = m_streamAenc;
	} else {
		obs_encoder_release(m_recVenc);
		obs_encoder_release(m_recAenc);
		m_recVenc = rs.quality != "stream" ? makeVideoEncoder(resolveRecordEncoder(), true, "LOBS record video")
						   : makeVideoEncoder(resolveEncoder(), false, "LOBS record video");
		obs_data_t *as = toData({{"bitrate", 192}});
		m_recAenc = obs_audio_encoder_create("ffmpeg_aac", "LOBS record audio", as, 0, nullptr);
		obs_data_release(as);
		obs_encoder_set_audio(m_recAenc, obs_get_audio());
		venc = m_recVenc;
		aenc = m_recAenc;
	}
	obs_data_t *os = toData({{"path", QDir::toNativeSeparators(m_lastRecording)}});
	m_recOutput = obs_output_create("ffmpeg_muxer", "LOBS recording", os, nullptr);
	obs_data_release(os);
	obs_output_set_video_encoder(m_recOutput, venc);
	obs_output_set_audio_encoder(m_recOutput, aenc, 0);
	signal_handler_connect(obs_output_get_signal_handler(m_recOutput), "stop", onRecordStop, this);
	if (!obs_output_start(m_recOutput)) {
		m_lastError = "Recording failed to start" + obsOutputError(m_recOutput);
		setStatus(m_lastError);
		return;
	}
	m_recording = true;
	m_recClock.start();
	emit recordingChanged();
	setStatus("Recording → " + QDir::toNativeSeparators(m_lastRecording));
}

void Engine::stopRecording()
{
	if (!m_recording)
		return;
	m_recordTimer.stop();
	obs_output_stop(m_recOutput);
	m_recording = false;
	emit recordingChanged();
	setStatus("Recording saved: " + QDir::toNativeSeparators(m_lastRecording));
}

void Engine::releaseOutputs()
{
	stopBandwidthTest();
	for (obs_output_t *o : {m_streamOutput, m_recOutput, m_replayOutput, m_vcamOutput}) {
		if (!o)
			continue;
		if (obs_output_active(o))
			obs_output_force_stop(o);
		obs_output_release(o);
	}
	m_streamOutput = m_recOutput = m_replayOutput = m_vcamOutput = nullptr;
	// The stream's, Enhanced Broadcasting's extra qualities and their group included: left to
	// obs_shutdown, the grouped encoders hung LOBS on exit (it kept running after its window closed).
	releaseStreamEncoders();
	for (obs_encoder_t *e : {m_recVenc, m_recAenc, m_replayVenc, m_replayAenc})
		obs_encoder_release(e);
	m_recVenc = m_recAenc = m_replayVenc = m_replayAenc = nullptr;
	obs_service_release(m_service);
	m_service = nullptr;
	m_streaming = m_recording = false;
}

// ---------- audio meters ----------

static void onVolmeter(void *data, const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS],
		       const float inputPeak[MAX_AUDIO_CHANNELS])
{
	Q_UNUSED(magnitude);
	Q_UNUSED(inputPeak);
	auto *m = static_cast<std::atomic<float> *>(data); // lr[2]
	auto clean = [](float v) { return std::isfinite(v) ? std::max(-60.0f, v) : -60.0f; };
	m[0].store(clean(peak[0]));
	m[1].store(clean(peak[1]));
}

void Engine::ensureMeter(const QString &sourceId, obs_source *in)
{
	if (m_volmeters.contains(sourceId) || !in)
		return;
	auto *meter = new Meter;
	obs_volmeter_t *vm = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(vm, in);
	obs_volmeter_add_callback(vm, onVolmeter, meter->lr);
	m_volmeters[sourceId] = vm;
	m_meters[sourceId] = meter;
}

void Engine::removeMeter(const QString &sourceId)
{
	if (obs_volmeter_t *vm = m_volmeters.take(sourceId)) {
		obs_volmeter_remove_callback(vm, onVolmeter, m_meters[sourceId]->lr);
		obs_volmeter_destroy(vm);
	}
	delete m_meters.take(sourceId);
}

QVariantMap Engine::audioLevels() const
{
	QVariantMap out;
	for (auto it = m_meters.begin(); it != m_meters.end(); ++it)
		out[it.key()] = QVariantList{double((*it)->lr[0].load()), double((*it)->lr[1].load())};
	return out;
}

// ---------- setup: bandwidth test ----------
// Streams the canvas to a server for a few seconds and measures what actually gets through.
// On Twitch, "?bandwidthtest" on the key makes the ingest accept and drop the stream, so
// nothing goes live. The encoder is pushed above the target so the result shows headroom.

bool Engine::startBandwidthTest(const QString &server, const QString &key, int kbps, int seconds)
{
	if (m_bwOutput || m_streaming)
		return false;
	obs_data_t *d = toData({{"server", server}, {"key", key}});
	m_bwService = obs_service_create_private("rtmp_custom", "LOBS bandwidth test", d);
	obs_data_release(d);
	const int saved = m_settings.stream.bitrate;
	m_settings.stream.bitrate = kbps;
	m_bwVenc = makeVideoEncoder(resolveEncoder(), false, "LOBS bandwidth test video");
	m_settings.stream.bitrate = saved;
	obs_data_t *as = toData({{"bitrate", 160}});
	m_bwAenc = obs_audio_encoder_create("ffmpeg_aac", "LOBS bandwidth test audio", as, 0, nullptr);
	obs_data_release(as);
	if (!m_bwService || !m_bwVenc || !m_bwAenc) {
		stopBandwidthTest();
		emit bandwidthTestDone({{"ok", false}, {"error", "Couldn't set up the test encoder"}});
		return false;
	}
	obs_encoder_set_audio(m_bwAenc, obs_get_audio());
	obs_data_t *od = toData({});
	m_bwOutput = obs_output_create("rtmp_output", "LOBS bandwidth test", od, nullptr);
	obs_data_release(od);
	obs_output_set_service(m_bwOutput, m_bwService);
	obs_output_set_video_encoder(m_bwOutput, m_bwVenc);
	obs_output_set_audio_encoder(m_bwOutput, m_bwAenc, 0);
	if (!obs_output_start(m_bwOutput)) {
		const char *e = obs_output_get_last_error(m_bwOutput);
		const QString err = e ? QString::fromUtf8(e) : QString("Couldn't connect to the server");
		stopBandwidthTest();
		emit bandwidthTestDone({{"ok", false}, {"error", err}});
		return false;
	}
	m_bwTarget = kbps;
	m_bwSeconds = seconds;
	m_bwClock.start();
	m_bwStartBytes = -1;
	m_bwSamples.clear();
	if (!m_bwTimer) {
		m_bwTimer = new QTimer(this);
		m_bwTimer->setInterval(500);
		connect(m_bwTimer, &QTimer::timeout, this, &Engine::pollBandwidthTest);
	}
	m_bwTimer->start();
	return true;
}

void Engine::pollBandwidthTest()
{
	if (!m_bwOutput)
		return;
	const qint64 ms = m_bwClock.elapsed();
	const qint64 bytes = qint64(obs_output_get_total_bytes(m_bwOutput));
	const bool connected = obs_output_active(m_bwOutput) && bytes > 0;
	// The first 2 s are the connection and encoder warming up.
	if (connected && ms >= 2000 && m_bwStartBytes < 0) {
		m_bwStartBytes = bytes;
		m_bwStartMs = ms;
	}
	double kbps = 0;
	if (m_bwStartBytes >= 0 && ms > m_bwStartMs)
		kbps = (bytes - m_bwStartBytes) * 8.0 / (ms - m_bwStartMs);
	const int total = obs_output_get_total_frames(m_bwOutput), dropped = obs_output_get_frames_dropped(m_bwOutput);
	const double droppedPct = total > 0 ? 100.0 * dropped / total : 0;
	emit bandwidthTestProgress({{"kbps", int(kbps)}, {"elapsed", ms / 1000.0}, {"seconds", m_bwSeconds}, {"dropped", droppedPct}, {"connected", connected}});
	if (ms >= m_bwSeconds * 1000 || (ms > 8000 && !connected)) {
		const QVariantMap result{{"ok", connected && kbps > 0}, {"kbps", int(kbps)}, {"target", m_bwTarget}, {"dropped", droppedPct},
					 {"error", connected ? QString() : QString("The server didn't accept the test stream. Check the stream key.")}};
		stopBandwidthTest();
		emit bandwidthTestDone(result);
	}
}

void Engine::stopBandwidthTest()
{
	if (m_bwTimer)
		m_bwTimer->stop();
	if (m_bwOutput) {
		if (obs_output_active(m_bwOutput))
			obs_output_force_stop(m_bwOutput);
		obs_output_release(m_bwOutput);
		m_bwOutput = nullptr;
	}
	obs_encoder_release(m_bwVenc), m_bwVenc = nullptr;
	obs_encoder_release(m_bwAenc), m_bwAenc = nullptr;
	if (m_bwService)
		obs_service_release(m_bwService), m_bwService = nullptr;
}
