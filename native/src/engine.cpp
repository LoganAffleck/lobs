#include "engine.h"
#include "frontend.h"
#include "model/audio_fx.h"
#include "model/video_fx.h"
#include "permissions.h"
#include "plugins.h"
#include <QUuid>
#include "platform.h"
#include <QtGlobal>
#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#else
#include <dlfcn.h>
#endif
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QJsonArray>

#include <obs.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSize>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <cmath>

extern "C" void lobs_register_instance_source(void);
extern "C" void lobs_register_frame_source(void);
extern "C" void lobs_frame_set(obs_source_t *source, const uint8_t *bgra, uint32_t width, uint32_t height, uint32_t stride);
#include "shapes.h"

using namespace lobs;

Engine::Engine(QObject *parent) : QObject(parent)
{
	connect(&m_statsTimer, &QTimer::timeout, this, &Engine::pollStats);
	m_recordTimer.setSingleShot(true);
	connect(&m_recordTimer, &QTimer::timeout, this, &Engine::stopRecording);
	m_parkTimer.setSingleShot(true);
	connect(&m_parkTimer, &QTimer::timeout, this, &Engine::parkHiddenFilters);
}

Engine::~Engine()
{
	shutdown();
}

void Engine::setStatus(const QString &s)
{
	m_status = s;
	blog(LOG_INFO, "[lobs] %s", s.toUtf8().constData());
	emit statusChanged();
}

bool Engine::start(const QString &runtimeRoot)
{
	const QString root = QDir(runtimeRoot).absolutePath();
	m_root = root;
#ifdef Q_OS_WIN
	const QString cfg = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/plugin_config";
#else
	// ~/Library/Preferences is for settings files only; modules keep caches here (the browser's).
	const QString cfg = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugin_config";
#endif
	QDir().mkpath(cfg);
	if (!obs_startup("en-US", cfg.toUtf8().constData(), nullptr)) {
		setStatus("The OBS engine failed to start");
		return false;
	}
	obs_video_info ovi = {};
#ifdef Q_OS_WIN
	// With the cwd at <root>/bin/64bit, libobs's default search finds the bundled
	// <root>/obs-plugins/64bit and <root>/data, never an OBS install.
	obs_add_data_path((root + "/data/libobs/").toUtf8().constData());
	ovi.graphics_module = "libobs-d3d11.dll";
#else
	// libobs.framework carries its own data, and its default search finds the modules in
	// LOBS.app/Contents/PlugIns, never an OBS install.
	ovi.graphics_module = "libobs-opengl.dylib";
#endif
	ovi.fps_num = 60;
	ovi.fps_den = 1;
	ovi.base_width = ovi.output_width = uint32_t(m_canvasW);
	ovi.base_height = ovi.output_height = uint32_t(m_canvasH);
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.gpu_conversion = true;
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.scale_type = OBS_SCALE_BICUBIC;
	if (int vr = obs_reset_video(&ovi); vr != OBS_VIDEO_SUCCESS) {
		setStatus(QString("Couldn't start video (%1)").arg(vr));
		return false;
	}
	obs_audio_info ai = {48000, SPEAKERS_STEREO};
	if (!obs_reset_audio(&ai)) {
		setStatus("Couldn't start audio");
		return false;
	}
	// LOBS answers OBS's interface API before plugins load: they register docks and events there.
	lobs::frontend::install(this);
	// OBS 33 replaced module loading: obs_load_all_modules() became an empty stub, and its
	// obs_load_core_modules() insists on every module OBS ships (including OBS's UI-only ones,
	// which LOBS leaves out). So on 33+ LOBS loads exactly the modules it kept, from
	// core/<name>/, with obs_load_plugins(). Resolved at runtime: one LOBS build runs on both.
	struct RuntimeModulePath { const char *binary, *data; };                         // OBS 33 obs.h
	struct RuntimeModuleInfo { RuntimeModulePath path_info; int type; const char *name; };
	struct ModuleFailureInfo { char **failed_modules; size_t count; };
	using LoadPlugins = void (*)(RuntimeModuleInfo *, ModuleFailureInfo *);
	using FreeFailures = void (*)(ModuleFailureInfo *);
#ifdef Q_OS_WIN
	const HMODULE obsDll = GetModuleHandleW(L"obs.dll");
	auto loadPlugins = reinterpret_cast<LoadPlugins>(GetProcAddress(obsDll, "obs_load_plugins"));
	auto freeFailures = reinterpret_cast<FreeFailures>(GetProcAddress(obsDll, "obs_module_failure_info_free"));
#else
	auto loadPlugins = reinterpret_cast<LoadPlugins>(dlsym(RTLD_DEFAULT, "obs_load_plugins"));
	auto freeFailures = reinterpret_cast<FreeFailures>(dlsym(RTLD_DEFAULT, "obs_module_failure_info_free"));
#endif
	if (loadPlugins && QDir(root + "/core").exists()) {
		const QByteArray bin = (root + "/core/%module%").toUtf8(), data = (root + "/core/%module%/data").toUtf8();
		RuntimeModuleInfo info{{bin.constData(), data.constData()}, 1 /* OBS_MODULE_TYPE_CORE */, "lobs-core"};
		ModuleFailureInfo failures{};
		loadPlugins(&info, &failures);
		for (size_t i = 0; i < failures.count; ++i)
			blog(LOG_WARNING, "[lobs] module failed to load: %s", failures.failed_modules[i]);
		if (freeFailures)
			freeFailures(&failures);
	} else {
		obs_load_all_modules();
	}
	// Hotkeys work while another app (a game) has focus, like OBS.
	obs_hotkey_enable_background_press(true);
	lobs::plugins::loadAll(); // third-party plugins, one at a time (see plugins.h)
	obs_log_loaded_modules();
	obs_post_load_modules();
	lobs_register_instance_source();
	lobs_register_frame_source();
	// Laser pointer: a canvas-size frame on the last output channel, drawn over the program.
	m_laser = obs_source_create_private("lobs_frame", "lobs_laser", nullptr);
	obs_set_output_source(MAX_CHANNELS - 1, m_laser);
	m_ghost = obs_source_create_private("lobs_frame", "lobs_ghost", nullptr);
	m_laserTimer.setInterval(16);
	connect(&m_laserTimer, &QTimer::timeout, this, &Engine::tickLaser);
	m_laserClock.start();
	m_started = true;
	m_statsTimer.start(1000);
	setStatus("Engine running");
	return true;
}

// ---------- document mapping ----------

// Text layers: GDI+ on Windows; on macOS LOBS draws them itself (renderText, as shapes) into a
// lobs_frame, since OBS's FreeType text has fewer options and lays text out differently.
#ifdef Q_OS_WIN
static const char *kTextSource = "text_gdiplus_v3";
static constexpr bool kOwnText = false;
#else
static const char *kTextSource = "lobs_frame";
static constexpr bool kOwnText = true;
#endif

// renderText's settings for a text source, with this platform's default font filled in.
static QVariantMap ownTextSettings(const Source &s)
{
	QVariantMap st = s.settings;
	if (st.value("font").toString().isEmpty())
		st["font"] = defaultTextFont();
	return st;
}

// The OBS source type for a LOBS kind on this platform, or nullptr if it has none here.
static const char *obsKindOrNull(const QString &kind)
{
	static const QHash<QString, const char *> map = {
#ifdef Q_OS_WIN
		{"camera", "dshow_input"},          {"display", "monitor_capture"},     {"window", "window_capture"},
		{"game", "game_capture"},
		{"mic", "wasapi_input_capture"},    {"desktop_audio", "wasapi_output_capture"},
#else
		// ScreenCaptureKit covers displays and windows (its "type" setting picks which, see
		// inputSettings); macOS has no game capture.
		{"camera", "macos-avcapture"},      {"display", "screen_capture"},      {"window", "screen_capture"},
		{"mic", "coreaudio_input_capture"}, {"desktop_audio", "sck_audio_capture"},
#endif
		{"image", "image_source"},          {"media", "ffmpeg_source"},
		{"browser", "browser_source"},      {"color", "color_source_v3"},       {"text", kTextSource},
		{"shape", "lobs_frame"}, // drawn per layer (see apply); the input itself stays empty
	};
	if (kind.startsWith("obs:")) { // a plugin's source, by its OBS id
		static QHash<QString, QByteArray> ids;
		auto it = ids.find(kind);
		if (it == ids.end())
			it = ids.insert(kind, kind.mid(4).toUtf8());
		return obs_source_get_display_name(it->constData()) ? it->constData() : nullptr;
	}
	return map.value(kind, nullptr);
}

static const char *obsKind(const QString &kind)
{
	const char *id = obsKindOrNull(kind);
	return id ? id : "color_source_v3";
}

QString Engine::obsIdFor(const QString &kind)
{
	const char *id = obsKindOrNull(kind);
	return id ? QString(id) : kind;
}

bool Engine::kindAvailable(const QString &kind) const
{
	if (kind == "scene")
		return true; // a LOBS scene, not an OBS module
	const char *id = obsKindOrNull(kind);
	return id && obs_source_get_display_name(id); // registered by a module this engine loaded
}

static const char *deviceProp(const QString &kind)
{
#ifdef Q_OS_WIN
	if (kind == "camera") return "video_device_id";
	if (kind == "display") return "monitor_id";
	if (kind == "window" || kind == "game") return "window";
	if (kind == "mic" || kind == "desktop_audio") return "device_id";
#else
	if (kind == "camera") return "device";
	if (kind == "display") return "display_uuid";
	if (kind == "window") return "window";
	if (kind == "mic") return "device_id";
#endif
	return nullptr;
}

static long long hexToAbgr(const QString &hex)
{
	bool ok = false;
	const uint n = hex.mid(hex.startsWith('#') ? 1 : 0).toUInt(&ok, 16);
	if (!ok) return 0xFF000000LL;
	const uint r = (n >> 16) & 255, g = (n >> 8) & 255, b = n & 255;
	return 0xFF000000LL | (b << 16) | (g << 8) | r;
}

// Text is resolution-independent in LOBS: each layer rasterizes the text itself at the
// pixel height it occupies on the canvas, so resizing a text layer re-renders it crisply
// instead of scaling a fixed-size bitmap (OBS ties resolution to font size).
static QString textStyle(const QVariantMap &st)
{
	const bool b = st.value("bold", true).toBool(), i = st.value("italic").toBool();
	return b && i ? "Bold Italic" : b ? "Bold" : i ? "Italic" : "Regular";
}

static QJsonObject textSettings(const Source &s, int pixelHeight)
{
	const QVariantMap &st = s.settings;
	const long long color = hexToAbgr(st.value("textColor", "#ffffff").toString()) & 0xFFFFFF;
	QJsonObject o{{"text", st.value("text").toString()},
		      {"color", color},
		      {"font", QJsonObject{{"face", st.value("font", defaultTextFont()).toString()}, {"size", std::clamp(pixelHeight, 4, 2048)},
					   {"style", textStyle(st)}, {"flags", (st.value("bold", true).toBool() ? 1 : 0) | (st.value("italic").toBool() ? 2 : 0)}}}};
#ifndef Q_OS_WIN
	// FreeType2 colors text with a top-to-bottom gradient, in opaque ABGR.
	o["color1"] = color | 0xFF000000LL;
	o["color2"] = color | 0xFF000000LL;
#endif
	return o;
}

static QJsonObject inputSettings(const Doc &doc, const Source &s)
{
	const QVariantMap &st = s.settings;
	if (s.kind.startsWith("obs:"))
		return QJsonObject::fromVariantMap(st); // the plugin's own settings, as OBS keeps them
	if (s.kind == "color")
		return {{"color", hexToAbgr(st.value("color", "#000000").toString())}, {"width", doc.canvasW}, {"height", doc.canvasH}};
	if (s.kind == "text")
		return textSettings(s, 96); // nominal size; only used for Sources previews
	if (s.kind == "image")
		return {{"file", st.value("file").toString()}};
	if (s.kind == "browser") {
		// Page size defaults to the canvas, so full-screen overlays line up 1:1. Page audio
		// is routed through libobs so it shows up in the mixer like any other source.
		const int w = st.value("width").toInt(), h = st.value("height").toInt();
		QJsonObject o{{"url", st.value("url").toString()}, {"width", w > 0 ? w : doc.canvasW}, {"height", h > 0 ? h : doc.canvasH},
			      {"is_local_file", st.value("local").toBool()}, {"local_file", st.value("file").toString()},
			      {"reroute_audio", st.value("controlAudio", true).toBool()}, {"shutdown", st.value("shutdown").toBool()},
			      {"restart_when_active", st.value("restart").toBool()}, {"webpage_control_level", st.value("permissions", 1).toInt()},
			      {"fps_custom", st.value("fps").toInt() > 0}, {"fps", st.value("fps").toInt() > 0 ? st.value("fps").toInt() : 30}};
		if (!st.value("css").toString().isEmpty())
			o["css"] = st.value("css").toString(); // else the plugin's default: transparent page background
		return o;
	}
	if (s.kind == "media")
		return {{"local_file", st.value("file").toString()}, {"looping", st.value("loop").toBool()}};
	if (s.kind == "game")
		return {{"capture_mode", st.value("mode", "any_fullscreen").toString()}, {"window", st.value("device").toString()},
			{"allow_transparency", st.value("transparent").toBool()}}; // keeps the alpha of apps that draw it (VTuber avatars)
#ifndef Q_OS_WIN
	if (s.kind == "display" || s.kind == "window") { // one ScreenCaptureKit source: 0 display, 1 window
		QJsonObject o{{"type", s.kind == "display" ? 0 : 1}, {"show_cursor", true}};
		if (const QString d = st.value("device").toString(); !d.isEmpty()) {
			if (s.kind == "window")
				o["window"] = d.toLongLong(); // a CGWindowID; device ids are kept as text
			else
				o["display_uuid"] = d;
		}
		return o;
	}
#endif
#ifdef Q_OS_WIN
	// Windows Graphics Capture (method 2): OBS's Automatic picks BitBlt for most apps, which gets
	// a frozen frame or black from anything drawn on the GPU (OBS, LOBS, Unity apps like VNyan).
	if (s.kind == "window")
		return {{"method", st.value("method", 2).toInt()}, {"window", st.value("device").toString()}};
#endif
	if (const char *p = deviceProp(s.kind); p && !st.value("device").toString().isEmpty())
		return {{p, st.value("device").toString()}};
	return {};
}

static obs_data_t *toData(const QJsonObject &o)
{
	return obs_data_create_from_json(QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
}

static obs_data_t *filterData(const Doc &doc, const Filter &f, QString &kind)
{
	if (f.type.startsWith("obs:")) { // a plugin's filter: its settings as OBS keeps them
		kind = f.type.mid(4);
		return obs_source_get_display_name(kind.toUtf8().constData()) ? toData(QJsonObject::fromVariantMap(f.settings)) : nullptr;
	}
	const VfxType *t = videoFx(f.type);
	if (!t)
		return nullptr;
	kind = t->obsId;
	QJsonObject o;
	for (const VfxParam &p : t->params) {
		const QVariant v = videoFxValue(f.settings, p);
		const QString key = p.obsKey.isEmpty() ? p.key : p.obsKey;
		if (p.kind == "color")
			o[key] = double(hexToAbgr(v.toString()) & 0xFFFFFF);
		else if (p.kind == "toggle")
			o[key] = v.toBool();
		else if (p.kind == "slider")
			o[key] = p.decimals == 0 ? QJsonValue(qint64(std::llround(v.toDouble()))) : QJsonValue(v.toDouble());
		else
			o[key] = v.toString();
	}
	if (f.type == "scale" && o["resolution"].toString() == "canvas")
		o["resolution"] = QString("%1x%2").arg(doc.canvasW).arg(doc.canvasH);
	return toData(o);
}

static QString transitionSig(const Transition &t);

// A white rounded rectangle on transparent, for the mask filter that rounds a layer's corners.
// The mask is stretched to the layer, so only its shape matters: it's keyed by aspect (to 0.5%)
// and radius, not exact size. Moving or resizing a layer reuses it instead of making a new file
// every frame, and known masks are remembered without touching the disk.
static QString cornerMask(double w, double h, double radiusPct)
{
	static QHash<QString, QString> made;
	const double aspect = std::clamp(w / std::max(1.0, h), 0.02, 50.0);
	const int aspectKey = int(std::lround(std::log(aspect) * 200)); // ~0.5% steps
	const int radiusKey = int(std::lround(std::clamp(radiusPct, 0.0, 50.0) * 2));
	const QString key = QString("%1_%2").arg(aspectKey).arg(radiusKey);
	if (auto it = made.constFind(key); it != made.constEnd())
		return *it;
	const double a = std::exp(aspectKey / 200.0);
	const int mw = a >= 1 ? 1536 : std::max(8, int(std::lround(1536 * a))), mh = a >= 1 ? std::max(8, int(std::lround(1536 / a))) : 1536;
	const double r = radiusKey / 2.0 / 100.0 * std::min(mw, mh);
	const QString dir = QDir::tempPath() + "/LOBS-masks";
	const QString path = QString("%1/corners-%2.png").arg(dir, key);
	if (!QFileInfo::exists(path)) {
		QDir().mkpath(dir);
		QImage img(mw, mh, QImage::Format_ARGB32);
		img.fill(Qt::transparent);
		QPainter p(&img);
		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		p.setBrush(Qt::white);
		p.drawRoundedRect(QRectF(0, 0, mw, mh), r, r);
		p.end();
		img.save(path, "PNG", 100); // fastest encode
	}
	made.insert(key, path);
	return path;
}

void Engine::apply(const Doc &doc)
{
	// apply() can switch scenes (a deleted scene was on screen), and switching unparks layers
	// through apply(); never re-enter it while it walks the layers.
	if (m_applying)
		return;
	m_applying = true;
	struct Done { bool &f; ~Done() { f = false; } } done{m_applying};
	m_lastDoc = doc;
	updateEditCanvas();
	// When it's done (scenes made): the program's vertical layout on the vertical canvas.
	struct Then { Engine *e; ~Then() { e->syncVerticalProgram(); } } then{this};
	// The show's transition: rebuilt (without playing) when it changes.
	if (const Transition *t = doc.transition()) {
		const bool changed = transitionSig(*t) != transitionSig(m_trDef);
		m_trDef = *t;
		if (changed && m_started && m_transition)
			setProgramScene(m_programId);
	}
	if (!m_started)
		return;

	// Most changes touch one layer or source; everything unchanged is skipped below, so an edit
	// costs about the same in a 3-scene show and a 300-scene one (LOBS-024).
	const bool canvasSame = m_appliedCanvasW == doc.canvasW && m_appliedCanvasH == doc.canvasH;
	m_appliedCanvasW = doc.canvasW, m_appliedCanvasH = doc.canvasH;

	// 1. Inputs: one per pool source, named by its id. A browser source on no scene shuts its
	// page down (nothing shows it, and a page costs a browser process and its CPU).
	QSet<QString> placed;
	for (const Instance &i : doc.instances)
		placed.insert(i.sourceId);
	for (const QString &id : doc.sourceOrder) {
		Source s = doc.sources[id];
		if (s.kind == "browser" && !placed.contains(id))
			s.settings["shutdown"] = true;
		if (canvasSame && m_inputs.contains(id) && m_appliedInput.value(id) == s)
			continue;
		// Not created until macOS allows it (it would block, or fail); left unapplied, it's
		// created on the apply that follows the grant (see Permissions::granted).
		if (!m_inputs.contains(id) && (!Permissions::allows(s.kind) || (s.kind != "shape" && !obsKindOrNull(s.kind))))
			continue; // and kinds this platform has no capture for (Game Capture on macOS): nothing, not a stand-in
		m_appliedInput[id] = s;
		const QJsonObject settings = inputSettings(doc, s);
		const QString sig = QString(obsKind(s.kind)) + QJsonDocument(settings).toJson(QJsonDocument::Compact);
		obs_source_t *in = m_inputs.value(id);
		const bool fresh = !in, changed = fresh || m_inputSig.value(id) != sig;
		if (!in) {
			obs_data_t *d = toData(settings);
			in = obs_source_create(obsKind(s.kind), id.toUtf8().constData(), d, nullptr);
			obs_data_release(d);
			m_inputs[id] = in;
		} else if (changed) {
			obs_data_t *d = toData(settings);
			obs_source_update(in, d);
			obs_data_release(d);
		}
		m_inputSig[id] = sig;
		if (kOwnText && s.kind == "text" && changed && in) { // the Sources preview: one 96 px line
			const QImage img = renderText(ownTextSettings(s), 0, 0, 96);
			lobs_frame_set(in, img.constBits(), uint32_t(img.width()), uint32_t(img.height()), uint32_t(img.bytesPerLine()));
		}
		if (s.hasAudio)
			ensureMeter(id, in);
		if (s.hasAudio) {
			obs_source_set_volume(in, float(s.audio.volume));
			m_docMuted[id] = s.audio.muted;
			obs_source_set_muted(in, s.audio.muted || (m_pttBound.contains(id) && !m_pttHeld.contains(id)));
			obs_source_set_monitoring_type(in, s.audio.monitor == "monitor_only"          ? OBS_MONITORING_TYPE_MONITOR_ONLY
							   : s.audio.monitor == "monitor_and_output" ? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT
												     : OBS_MONITORING_TYPE_NONE);
			uint32_t mixers = 0;
			for (int t = 0; t < s.audio.tracks.size(); t++)
				if (s.audio.tracks[t])
					mixers |= 1u << t;
			obs_source_set_audio_mixers(in, mixers);
			applyAudioFilters(in, id, s.audio.filters);
			// Advanced audio: mono downmix, L/R balance, sync offset.
			const uint32_t flags = obs_source_get_flags(in);
			const uint32_t want = s.audio.mono ? (flags | OBS_SOURCE_FLAG_FORCE_MONO) : (flags & ~uint32_t(OBS_SOURCE_FLAG_FORCE_MONO));
			if (want != flags)
				obs_source_set_flags(in, want);
			if (std::abs(obs_source_get_balance_value(in) - float(s.audio.balance)) > 0.0001f)
				obs_source_set_balance_value(in, float(s.audio.balance));
			const int64_t sync = int64_t(s.audio.syncMs) * 1000000;
			if (obs_source_get_sync_offset(in) != sync)
				obs_source_set_sync_offset(in, sync);
		}
		// Audio-only sources are global: give each its own output channel. From 20 up, and never
		// one a plugin uses: plugins draw on the low ones (Downstream Keyer's default is 7).
		if (!kindInfo(s.kind).video && !m_audioChannel.contains(id)) {
			auto taken = [this](int ch) {
				if (std::find(m_audioChannel.begin(), m_audioChannel.end(), ch) != m_audioChannel.end())
					return true;
				obs_source_t *o = obs_get_output_source(uint32_t(ch));
				obs_source_release(o);
				return o != nullptr;
			};
			int ch = 20;
			while (ch < MAX_CHANNELS - 1 && taken(ch)) // the last one is the laser pointer
				ch++;
			if (ch < MAX_CHANNELS - 1) {
				m_audioChannel[id] = ch;
				obs_set_output_source(uint32_t(ch), in);
			}
		}
	}

	m_hasSceneSources = std::any_of(doc.sources.cbegin(), doc.sources.cend(), [](const Source &s) { return s.kind == "scene"; });
	// The vertical canvas (Twitch's vertical stream): made while it's on, its scenes are the scenes'
	// vertical layouts. It follows the program; audio is the main canvas's (not mixed twice).
	if (doc.verticalOn && !m_vcanvas) {
		obs_video_info ovi{};
		obs_get_video_info(&ovi);
		ovi.base_width = ovi.output_width = uint32_t(doc.verticalW);
		ovi.base_height = ovi.output_height = uint32_t(doc.verticalH);
		m_vcanvas = obs_canvas_create("LOBS Vertical", &ovi, ACTIVATE | SCENE_REF);
		blog(LOG_INFO, "[lobs] vertical canvas %dx%d: %s", doc.verticalW, doc.verticalH, m_vcanvas ? "made" : "failed");
	}
	const QStringList allScenes = scenesToMake(doc);
	// 2. Scenes, named as in LOBS (made unique), since plugins show OBS's names (Downstream Keyer's
	// list, for one). LOBS finds them by id (m_scenes) and layers find them by UUID.
	{
		QSet<QString> used;
		for (const QString &id : allScenes) {
			QString name = doc.scenes[id].verticalOf.isEmpty() ? doc.scenes[id].name.trimmed()
									 : doc.scenes.value(doc.scenes[id].verticalOf).name.trimmed() + " (Vertical)";
			if (name.isEmpty())
				name = "Scene";
			const QString base = name;
			for (int n = 2; used.contains(name.toLower()); n++)
				name = QString("%1 (%2)").arg(base).arg(n);
			used.insert(name.toLower());
			if (!m_scenes.contains(id))
				m_scenes[id] = doc.scenes[id].verticalOf.isEmpty() || !m_vcanvas ? obs_scene_create(name.toUtf8().constData())
													 : obs_canvas_scene_create(m_vcanvas, name.toUtf8().constData());
			else if (obs_source_t *s = obs_scene_get_source(m_scenes[id]); name != obs_source_get_name(s))
				obs_source_set_name(s, name.toUtf8().constData());
		}
	}

	// 3. Layers: one lobs_instance per visible-capable leaf, with its own filters.
	QSet<QString> wanted;
	for (const QString &sceneId : allScenes) {
		obs_scene_t *scene = m_scenes[sceneId];
		int order = 0;
		for (const Doc::Leaf &leaf : doc.flatten(sceneId)) {
			const Instance &inst = doc.instances[leaf.id];
			if (!doc.sources.contains(inst.sourceId) || !kindInfo(doc.sources[inst.sourceId].kind).video)
				continue;
			// A scene shown in itself (pasted in, say) would draw forever: it's left out.
			if (const Source &ss = doc.sources[inst.sourceId]; ss.kind == "scene" && doc.sceneReaches(ss.settings.value("scene").toString(), sceneId))
				continue;
			wanted.insert(inst.id);
			Item &it = m_items[inst.id];
			const Crop c = doc.crop(inst);
			const Source &src = doc.sources[inst.sourceId];
			const Item::Applied now{doc.transform(inst), c, Visibility{leaf.visible, leaf.opacity}, order, sceneId, src.kind,
						src.kind == "text" || src.kind == "shape" ? src.settings : QVariantMap(), doc.filters(inst)};
			// A parked layer stays parked until its scene shows, or while it's hidden and unchanged:
			// else every edit re-adds a hidden layer's filters and the park timer drops them again.
			if (it.hasApplied && (it.parked ? !sceneWanted(sceneId) || (!leaf.visible && canvasSame && it.applied == now)
							: it.item && canvasSame && it.applied == now)) {
				// Unchanged, or parked (applied in full when it shows). A parked layer has no item in the
				// scene, so it takes no place in the order: counting it put later layers too high.
				if (it.item)
					++order;
				continue;
			}
			it.applied = now;
			it.hasApplied = true;
			QString childName = inst.sourceId;
			if (src.kind == "scene") // a scene by its UUID (its name follows the scene's)
				if (obs_source_t *sc = sceneSource(src.settings.value("scene").toString()))
					childName = obs_source_get_uuid(sc);
			if (src.kind == "text" && kOwnText) {
				// Drawn at the layer's own pixel size, so it stays crisp (as shapes).
				const Transform tt = doc.transform(inst);
				const int tw = int(std::lround(std::max(1.0, tt.w))), th = int(std::lround(std::max(1.0, tt.h)));
				const QVariantMap st = ownTextSettings(src);
				const QString textSig = QJsonDocument(QJsonObject::fromVariantMap(st)).toJson(QJsonDocument::Compact) + QString("|%1x%2").arg(tw).arg(th);
				childName = inst.id + ":text" + (it.textGen ? QString("#%1").arg(it.textGen) : QString());
				if (!it.text)
					it.text = obs_source_create("lobs_frame", childName.toUtf8().constData(), nullptr, nullptr);
				if (it.textSig != textSig) {
					const QImage img = renderText(st, tw, th);
					lobs_frame_set(it.text, img.constBits(), uint32_t(img.width()), uint32_t(img.height()), uint32_t(img.bytesPerLine()));
					it.textSig = textSig;
				}
			} else if (src.kind == "text") {
				// Font pixel size from the layer's height (split across lines), so it renders 1:1.
				const Transform tt = doc.transform(inst);
				const int lines = int(src.settings.value("text").toString().count('\n')) + 1;
				const QJsonObject ts = textSettings(src, int(std::lround(tt.h / lines)));
				const QString textSig = QJsonDocument(ts).toJson(QJsonDocument::Compact);
				// A new name after parking, so the instance looks its renderer up again.
				childName = inst.id + ":text" + (it.textGen ? QString("#%1").arg(it.textGen) : QString());
				obs_data_t *td = toData(ts);
				if (!it.text)
					it.text = obs_source_create(kTextSource, childName.toUtf8().constData(), td, nullptr);
				else if (it.textSig != textSig)
					obs_source_update(it.text, td);
				obs_data_release(td);
				it.textSig = textSig;
			}
			const Transform ft = doc.transform(inst);
			if (src.kind == "shape") {
				// Vector graphics are drawn at the layer's own pixel size, so they stay crisp.
				const Transform st = doc.transform(inst);
				const int sw = int(std::lround(std::max(1.0, st.w))), sh = int(std::lround(std::max(1.0, st.h)));
				// Rounded corners on a rectangle are drawn by the shape itself, so the stroke follows
				// the curve (a mask would cut the stroke off at the corners).
				QVariantMap shapeSt = src.settings;
				if (shapeSt.value("shape").toString() == "rect" && c.radius > 0.01)
					shapeSt["radius"] = c.radius / 100.0 * std::min(sw, sh);
				const QString shapeSig = QJsonDocument(QJsonObject::fromVariantMap(shapeSt)).toJson(QJsonDocument::Compact) + QString("|%1x%2").arg(sw).arg(sh);
				childName = inst.id + ":shape";
				if (!it.shape)
					it.shape = obs_source_create("lobs_frame", childName.toUtf8().constData(), nullptr, nullptr);
				if (it.shapeSig != shapeSig) {
					const QImage img = renderShape(shapeSt, sw, sh);
					lobs_frame_set(it.shape, img.constBits(), uint32_t(img.width()), uint32_t(img.height()), uint32_t(img.bytesPerLine()));
					it.shapeSig = shapeSig;
				}
			}
			const QString cropSig = QString("%1|%2,%3,%4,%5|%6%7").arg(childName).arg(c.l).arg(c.t).arg(c.r).arg(c.b).arg(ft.flipH).arg(ft.flipV);
			obs_data_t *instSettings = toData({{"source", childName}, {"crop_left", c.l}, {"crop_top", c.t}, {"crop_right", c.r}, {"crop_bottom", c.b},
							   {"flip_h", ft.flipH}, {"flip_v", ft.flipV}});
			if (!it.inst) {
				// Create with the shared input already attached, so adding it to a scene that is
				// showing also activates the input (activation walks enum_active_sources).
				it.sceneId = sceneId;
				it.inst = obs_source_create_private("lobs_instance", inst.id.toUtf8().constData(), instSettings);
				it.item = obs_scene_add(scene, it.inst);
				obs_sceneitem_addref(it.item);
				it.cropSig = cropSig;
			} else if (it.cropSig != cropSig) {
				obs_source_update(it.inst, instSettings);
				it.cropSig = cropSig;
			}
			obs_data_release(instSettings);
			if (!it.item) { // parked: back into its scene (order is set below)
				it.item = obs_scene_add(scene, it.inst);
				obs_sceneitem_addref(it.item);
			}
			it.parked = false;

			// Filters: rounded-corner mask first, then the layer's filters, then opacity last.
			// The chain is rebuilt only when its shape changes (filters added, removed, reordered,
			// or the mask/opacity appearing); values update in place, so dragging a slider
			// (opacity, corner radius, a filter setting) never makes the layer blink.
			const QVector<Filter> fs = doc.filters(inst);
			QString maskPath;
			const bool roundsItself = src.kind == "shape" && src.settings.value("shape").toString() == "rect";
			if (c.radius > 0.01 && !roundsItself) {
				const Transform mt = doc.transform(inst);
				maskPath = cornerMask(mt.w, mt.h, c.radius);
			}
			const bool needOpacity = leaf.opacity < 0.999 || it.opacityFilter; // kept once made
			QString shapeSig = QString(maskPath.isEmpty() ? "-" : "M") + (needOpacity ? "O" : "-");
			QString valueSig = QString("%1x%2").arg(doc.canvasW).arg(doc.canvasH); // a Scaling filter can follow the canvas
			for (const Filter &f : fs) {
				shapeSig += "|" + f.id + f.type;
				valueSig += "|" + QString(f.enabled ? "1" : "0") + QJsonDocument(QJsonObject::fromVariantMap(f.settings)).toJson(QJsonDocument::Compact);
			}
			auto maskData = [&] { return toData({{"type", "mask_alpha_filter.effect"}, {"image_path", maskPath}, {"stretch", true}, {"opacity", 1.0}, {"color", double(0xFFFFFFFF)}}); };
			if (it.filterSig != shapeSig) {
				std::vector<obs_source_t *> old;
				obs_source_enum_filters(it.inst, [](obs_source_t *, obs_source_t *f, void *p) { static_cast<std::vector<obs_source_t *> *>(p)->push_back(f); }, &old);
				for (obs_source_t *f : old)
					obs_source_filter_remove(it.inst, f);
				if (maskPath.isEmpty() && it.maskFilter) {
					obs_source_release(it.maskFilter);
					it.maskFilter = nullptr;
				}
				if (!maskPath.isEmpty()) {
					if (!it.maskFilter) {
						obs_data_t *d = maskData();
						it.maskFilter = obs_source_create_private("mask_filter_v2", "lobs_corners", d);
						obs_data_release(d);
						it.maskPath = maskPath;
					}
					obs_source_filter_add(it.inst, it.maskFilter);
				}
				for (const Filter &f : fs) {
					QString kind;
					obs_data_t *d = filterData(doc, f, kind);
					if (!d)
						continue;
					obs_source_t *fsrc = obs_source_create_private(kind.toUtf8().constData(), f.id.toUtf8().constData(), d);
					obs_source_set_enabled(fsrc, f.enabled);
					obs_source_filter_add(it.inst, fsrc);
					obs_source_release(fsrc);
					obs_data_release(d);
				}
				if (needOpacity) {
					if (!it.opacityFilter) {
						obs_data_t *d = toData({{"opacity", leaf.opacity}});
						it.opacityFilter = obs_source_create_private("color_filter_v2", "lobs_opacity", d);
						obs_data_release(d);
						it.opacity = leaf.opacity;
					}
					obs_source_filter_add(it.inst, it.opacityFilter);
				}
				it.filterSig = shapeSig;
				it.filterValueSig = valueSig;
			} else if (it.filterValueSig != valueSig) {
				for (const Filter &f : fs) {
					obs_source_t *fsrc = obs_source_get_filter_by_name(it.inst, f.id.toUtf8().constData());
					if (!fsrc)
						continue;
					QString kind;
					if (obs_data_t *d = filterData(doc, f, kind)) {
						obs_source_update(fsrc, d);
						obs_data_release(d);
					}
					obs_source_set_enabled(fsrc, f.enabled);
					obs_source_release(fsrc);
				}
				it.filterValueSig = valueSig;
			}
			if (it.maskFilter && it.maskPath != maskPath) {
				obs_data_t *d = maskData();
				obs_source_update(it.maskFilter, d);
				obs_data_release(d);
				it.maskPath = maskPath;
			}
			if (it.opacityFilter && std::abs(it.opacity - leaf.opacity) > 1e-4) {
				obs_data_t *d = toData({{"opacity", leaf.opacity}});
				obs_source_update(it.opacityFilter, d);
				obs_data_release(d);
				it.opacity = leaf.opacity;
			}

			const Transform t = doc.transform(inst);
			// Placed by its center, so rotation turns the layer around its middle.
			vec2 pos = {float(t.x + t.w / 2), float(t.y + t.h / 2)}, bounds = {float(std::max(1.0, t.w)), float(std::max(1.0, t.h))};
			obs_sceneitem_defer_update_begin(it.item);
			obs_sceneitem_set_alignment(it.item, OBS_ALIGN_CENTER);
			// Text keeps its proportions inside the layer box (centered) instead of stretching,
			// and is downscaled with area filtering from its 2x render, so edges stay crisp.
			const bool isText = src.kind == "text";
			obs_sceneitem_set_bounds_type(it.item, isText ? OBS_BOUNDS_SCALE_INNER : OBS_BOUNDS_STRETCH);
			obs_sceneitem_set_bounds_alignment(it.item, isText ? OBS_ALIGN_CENTER : (OBS_ALIGN_LEFT | OBS_ALIGN_TOP));
			// A scale filter makes OBS draw the item through its own texture the size of the source,
			// kept for the item's life. A solid color or vector shape looks the same without one, and
			// a canvas-sized color layer would hold 8 MB for nothing (LOBS-034).
			const bool flat = src.kind == "color" || src.kind == "shape";
			obs_sceneitem_set_scale_filter(it.item, isText ? OBS_SCALE_AREA : flat ? OBS_SCALE_DISABLE : OBS_SCALE_BICUBIC);
			obs_sceneitem_set_bounds(it.item, &bounds);
			obs_sceneitem_set_pos(it.item, &pos);
			obs_sceneitem_set_rot(it.item, float(t.rot));
			obs_sceneitem_defer_update_end(it.item);
			obs_sceneitem_set_visible(it.item, leaf.visible);
			obs_sceneitem_set_order_position(it.item, order++);
		}
	}
	for (const QString &id : m_items.keys())
		if (!wanted.contains(id))
			removeItem(id);

	// 4. Deleted scenes and sources.
	for (const QString &id : m_scenes.keys()) {
		if (doc.scenes.contains(id))
			continue;
		if (m_editId == id)
			setEditScene({});
		if (m_programId == id)
			setProgramScene({});
		if (m_showingAll.remove(id))
			obs_source_dec_showing(obs_scene_get_source(m_scenes.value(id)));
		obs_scene_release(m_scenes.take(id));
	}
	for (const QString &id : m_inputs.keys()) {
		if (doc.sources.contains(id))
			continue;
		if (m_audioChannel.contains(id))
			obs_set_output_source(uint32_t(m_audioChannel.take(id)), nullptr);
		removeMeter(id);
		obs_source_t *in = m_inputs.take(id);
		obs_source_remove(in);
		obs_source_release(in);
		m_inputSig.remove(id);
		m_audioFxSig.remove(id);
	}
}

void Engine::removeItem(const QString &instId)
{
	Item it = m_items.take(instId);
	if (it.item) {
		obs_sceneitem_remove(it.item);
		obs_sceneitem_release(it.item);
	}
	obs_source_release(it.inst);
	if (it.maskFilter)
		obs_source_release(it.maskFilter);
	if (it.opacityFilter)
		obs_source_release(it.opacityFilter);
	if (it.text) {
		obs_source_remove(it.text);
		obs_source_release(it.text);
	}
	if (it.shape) {
		obs_source_remove(it.shape);
		obs_source_release(it.shape);
	}
}

void Engine::laserPoint(QPointF at, bool newStroke)
{
	m_laserPts.push_back({at, m_laserClock.elapsed(), newStroke});
	if (!m_laserTimer.isActive())
		m_laserTimer.start();
}

void Engine::tickLaser()
{
	const qint64 now = m_laserClock.elapsed();
	const int life = m_laserLife;
	while (!m_laserPts.isEmpty() && now - m_laserPts.first().t > life)
		m_laserPts.removeFirst();
	QVector<LaserPoint> pts;
	for (const LaserPt &p : m_laserPts)
		pts.push_back({p.at, p.t, p.newStroke});
	bool any = false;
	const QImage img = renderLaser(pts, now, life, m_canvasW, m_canvasH, &any, m_laserColor);
	if (!any) {
		lobs_frame_set(m_laser, nullptr, 0, 0, 0);
		m_laserTimer.stop();
		return;
	}
	lobs_frame_set(m_laser, img.constBits(), uint32_t(img.width()), uint32_t(img.height()), uint32_t(img.bytesPerLine()));
}

void Engine::setGhost(const QImage &img, QPointF canvasPos)
{
	if (!m_ghost)
		return;
	{
		QMutexLocker lock(&m_overlayLock);
		m_ghostPos = canvasPos;
	}
	if (img.isNull())
		return lobs_frame_set(m_ghost, nullptr, 0, 0, 0);
	const QImage s = img.convertToFormat(QImage::Format_ARGB32);
	lobs_frame_set(m_ghost, s.constBits(), uint32_t(s.width()), uint32_t(s.height()), uint32_t(s.bytesPerLine()));
}

void Engine::setInk(const QVector<QPointF> &pts, const QString &color, double width)
{
	const QColor c(color);
	QMutexLocker lock(&m_overlayLock);
	m_ink = pts;
	m_inkColor[0] = float(c.redF()); m_inkColor[1] = float(c.greenF()); m_inkColor[2] = float(c.blueF()); m_inkColor[3] = 1;
	m_inkWidth = width;
}

QVariantMap Engine::layerResources() const
{
	int parked = 0, withFilters = 0, filters = 0, unknownFilters = 0, texts = 0, shapes = 0, showing = 0;
	for (const Item &it : m_items) {
		parked += it.parked;
		const int n = it.inst ? int(obs_source_filter_count(it.inst)) : 0;
		withFilters += n > 0, filters += n;
		// libobs keeps a filter whose id no loaded module registers, but it draws nothing.
		if (n)
			obs_source_enum_filters(it.inst, [](obs_source_t *, obs_source_t *f, void *p) { *static_cast<int *>(p) += !obs_source_get_display_name(obs_source_get_id(f)); }, &unknownFilters);
		texts += !!it.text, shapes += !!it.shape;
		showing += it.inst && obs_source_showing(it.inst);
	}
	return {{"layers", int(m_items.size())}, {"parked", parked}, {"withFilters", withFilters}, {"filters", filters}, {"unknownFilters", unknownFilters},
		{"texts", texts}, {"shapes", shapes}, {"showing", showing}};
}

void Engine::parkHiddenFilters()
{
	bool waiting = false;
	for (Item &it : m_items) {
		if (it.parked || !it.inst)
			continue;
		if (obs_source_showing(it.inst)) {
			// Still showing although its scene isn't wanted: a transition or projector; look again.
			waiting = waiting || !sceneWanted(it.applied.sceneId);
			continue;
		}
		// Plugin filters stay: OBS never destroys a filter because its source is hidden, and some
		// keep working off-screen or hold GPU state (win-spout sends while hidden, and its teardown
		// raced the graphics thread when parking dropped and re-made it: LOBS-042).
		if (std::any_of(it.applied.filters.cbegin(), it.applied.filters.cend(), [](const Filter &f) { return f.type.startsWith("obs:"); }))
			continue;
		// Its scene item goes too: with a scale filter it holds a source-sized texture, and a
		// hidden scene never ticks, so changing the filter wouldn't free it. apply() re-adds it.
		if (it.item) {
			obs_sceneitem_remove(it.item);
			obs_sceneitem_release(it.item);
			it.item = nullptr;
		}
		std::vector<obs_source_t *> old;
		obs_source_enum_filters(it.inst, [](obs_source_t *, obs_source_t *f, void *p) { static_cast<std::vector<obs_source_t *> *>(p)->push_back(f); }, &old);
		for (obs_source_t *f : old)
			obs_source_filter_remove(it.inst, f);
		if (it.maskFilter)
			obs_source_release(it.maskFilter), it.maskFilter = nullptr;
		if (it.opacityFilter)
			obs_source_release(it.opacityFilter), it.opacityFilter = nullptr;
		it.filterSig.clear();
		// A text layer's renderer holds GDI objects (DC, font, bitmap): ~2.5 per layer, and Windows
		// allows 10,000 per process. The stress show's 818 text layers held 2,000 (LOBS-025).
		if (it.text) {
			obs_source_remove(it.text);
			obs_source_release(it.text);
			it.text = nullptr;
			it.textSig.clear();
			++it.textGen;
		}
		it.parked = true;
	}
	if (waiting)
		m_parkTimer.start(1000);
}

bool Engine::sceneWanted(const QString &sceneId) const
{
	if (sceneId == m_editId || sceneId == m_programId || sceneId == m_unparkTarget || m_showingAll.contains(sceneId) || m_heldScenes.contains(sceneId) ||
	    m_pluginScenes.contains(sceneId))
		return true;
	if (const QString of = m_lastDoc.scenes.value(sceneId).verticalOf; !of.isEmpty() && (of == m_programId || of == m_editId))
		return true; // the live scene's vertical layout
	if (!m_hasSceneSources) // the usual show: no scene inside another, nothing to walk (this runs per layer)
		return false;
	QStringList roots{m_editId, m_programId, m_unparkTarget};
	roots += QStringList(m_showingAll.begin(), m_showingAll.end());
	roots += m_heldScenes.keys();
	roots += QStringList(m_pluginScenes.begin(), m_pluginScenes.end());
	for (const QString &r : roots)
		if (!r.isEmpty() && (r == sceneId || m_lastDoc.sceneReaches(r, sceneId)))
			return true;
	return false;
}

void Engine::unparkScene(const QString &sceneId)
{
	if (m_applying || sceneId.isEmpty())
		return; // apply() is already running and brings shown scenes back itself
	for (const Item &it : m_items)
		if (it.parked && (it.applied.sceneId == sceneId || (m_hasSceneSources && m_lastDoc.sceneReaches(sceneId, it.applied.sceneId)))) { // and scenes shown inside it
			m_unparkTarget = sceneId;
			apply(m_lastDoc); // re-attaches this scene's parked filters
			m_unparkTarget.clear();
			return;
		}
}

void Engine::setEditScene(const QString &sceneId)
{
	if (sceneId == m_editId)
		return;
	unparkScene(sceneId);
	if (!m_parkTimer.isActive())
		m_parkTimer.start(500);
	if (m_editSource) {
		obs_source_dec_showing(m_editSource);
		m_editSource = nullptr;
	}
	m_editId = sceneId;
	updateEditCanvas();
	if (obs_scene_t *s = m_scenes.value(sceneId)) {
		m_editSource = obs_scene_get_source(s);
		obs_source_inc_showing(m_editSource); // activates its cameras while editing
	}
}

// The transition sits on output channel 0 and shows the program scene. It's recreated
// when the transition type changes in settings, keeping the current scene on air.
// OBS source id and settings for a LOBS transition.
static const char *transitionObsId(const QString &type)
{
	return type == "cut" ? "cut_transition" : type == "slide" ? "slide_transition" : type == "swipe" ? "swipe_transition"
	     : type == "fade_to_color" ? "fade_to_color_transition" : type == "luma_wipe" ? "wipe_transition"
	     : type == "stinger" ? "obs_stinger_transition" : "fade_transition";
}

obs_source_t *Engine::createTransition(const Transition &t, const char *name)
{
	QJsonObject js = QJsonObject::fromVariantMap(t.settings);
	if (js.contains("color")) // #rrggbb -> ABGR
		js["color"] = double(hexToAbgr(js["color"].toString("#000000")));
	obs_data_t *d = toData(js);
	obs_source_t *tr = obs_source_create_private(transitionObsId(t.type), name, d);
	obs_data_release(d);
	return tr;
}

static QString transitionSig(const Transition &t)
{
	return t.type + "|" + QJsonDocument(QJsonObject::fromVariantMap(t.settings)).toJson(QJsonDocument::Compact);
}

obs_source_t *Engine::ensureTransition()
{
	const QString sig = transitionSig(m_trDef);
	if (m_transition && m_transitionType == sig)
		return m_transition;
	obs_source_t *tr = createTransition(m_trDef, "lobs_transition");
	if (!tr)
		return m_transition;
	const QString type = sig;
	obs_scene_t *cur = m_scenes.value(m_programId);
	obs_transition_set(tr, cur ? obs_scene_get_source(cur) : nullptr);
	obs_set_output_source(0, tr);
	if (m_transition)
		obs_source_release(m_transition);
	m_transition = tr;
	m_transitionType = type;
	return tr;
}

void Engine::setProgramScene(const QString &sceneId, bool animate)
{
	const bool changed = sceneId != m_programId;
	unparkScene(sceneId);
	if (!m_parkTimer.isActive())
		m_parkTimer.start(500); // parks what stopped showing; waits out transitions itself
	m_programId = sceneId;
	syncVerticalProgram();
	obs_scene_t *s = m_scenes.value(sceneId);
	obs_source_t *scene = s ? obs_scene_get_source(s) : nullptr;
	obs_source_t *tr = m_started ? ensureTransition() : nullptr;
	if (!tr) {
		obs_set_output_source(0, scene);
		return;
	}
	if (animate && changed && scene && m_trDef.type != "cut")
		obs_transition_start(tr, OBS_TRANSITION_MODE_AUTO, uint32_t(std::max(50, m_trDef.durationMs)), scene);
	else
		obs_transition_set(tr, scene);
}

QImage Engine::renderThumbnail(const QString &sceneId, int width)
{
	obs_scene_t *scene = m_scenes.value(sceneId);
	if (!scene || width <= 0)
		return {};
	unparkScene(sceneId);
	if (!m_parkTimer.isActive())
		m_parkTimer.start(500);
	const QSize c = m_lastDoc.canvasOf(sceneId); // a vertical layout is tall
	const int CW = c.width() > 0 ? c.width() : m_canvasW, CH = c.height() > 0 ? c.height() : m_canvasH;
	const uint32_t w = uint32_t(width), h = uint32_t(width * CH / CW);
	QImage img;
	obs_enter_graphics();
	gs_texrender_t *tr = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	if (gs_texrender_begin(tr, w, h)) {
		vec4 clear;
		vec4_set(&clear, 0, 0, 0, 1);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, float(CW), 0.0f, float(CH), -100.0f, 100.0f);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		obs_source_video_render(obs_scene_get_source(scene));
		gs_blend_state_pop();
		gs_texrender_end(tr);
		gs_stagesurf_t *stage = gs_stagesurface_create(w, h, GS_RGBA);
		gs_stage_texture(stage, gs_texrender_get_texture(tr));
		uint8_t *data = nullptr;
		uint32_t linesize = 0;
		if (gs_stagesurface_map(stage, &data, &linesize)) {
			img = QImage(data, int(w), int(h), int(linesize), QImage::Format_RGBA8888).copy();
			gs_stagesurface_unmap(stage);
		}
		gs_stagesurface_destroy(stage);
	}
	gs_texrender_destroy(tr);
	obs_leave_graphics();
	return img;
}

// ---------- transition preview (the transitions editor) ----------

// A big letter on a color card, like OBS's preview: what "from" and "to" look like.
static QImage previewCard(const QString &letter, QColor bg, int w, int h)
{
	QImage img(w, h, QImage::Format_ARGB32);
	img.fill(bg);
	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing);
	p.setRenderHint(QPainter::TextAntialiasing);
	QFont f(defaultTextFont());
	f.setPixelSize(h * 2 / 3);
	f.setBold(true);
	p.setFont(f);
	p.setPen(Qt::white);
	p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, letter);
	return img;
}

void Engine::previewTransition(const Transition &t)
{
	if (!m_started)
		return;
	const int W = m_canvasW, H = m_canvasH;
	if (!m_pvA) {
		m_pvA = obs_source_create_private("lobs_frame", "lobs_preview_a", nullptr);
		m_pvB = obs_source_create_private("lobs_frame", "lobs_preview_b", nullptr);
		const QImage a = previewCard("A", QColor("#4f6fb4"), W, H), b = previewCard("B", QColor("#4caf6e"), W, H);
		lobs_frame_set(m_pvA, a.constBits(), uint32_t(W), uint32_t(H), uint32_t(a.bytesPerLine()));
		lobs_frame_set(m_pvB, b.constBits(), uint32_t(W), uint32_t(H), uint32_t(b.bytesPerLine()));
	}
	if (m_pvTr) {
		obs_source_dec_showing(m_pvTr);
		obs_source_release(m_pvTr);
	}
	m_pvTr = createTransition(t, "lobs_preview_transition");
	if (!m_pvTr)
		return;
	obs_transition_set_size(m_pvTr, uint32_t(W), uint32_t(H));
	obs_transition_set_alignment(m_pvTr, OBS_ALIGN_CENTER);
	obs_transition_set_scale_type(m_pvTr, OBS_TRANSITION_SCALE_ASPECT);
	obs_source_inc_showing(m_pvTr); // media (a stinger video) plays only while shown
	obs_transition_set(m_pvTr, m_pvAtB ? m_pvB : m_pvA);
	m_pvAtB = !m_pvAtB;
	if (t.type == "cut")
		obs_transition_set(m_pvTr, m_pvAtB ? m_pvB : m_pvA);
	else
		obs_transition_start(m_pvTr, OBS_TRANSITION_MODE_AUTO, uint32_t(std::max(50, t.durationMs)), m_pvAtB ? m_pvB : m_pvA);
}

void Engine::stopTransitionPreview()
{
	if (m_pvTr) {
		obs_source_dec_showing(m_pvTr);
		obs_source_release(m_pvTr);
		m_pvTr = nullptr;
	}
	for (obs_source_t **s : {&m_pvA, &m_pvB})
		if (*s) {
			obs_source_release(*s);
			*s = nullptr;
		}
	m_pvAtB = false;
}

QImage Engine::renderTransitionPreview(int width)
{
	if (!m_pvTr || width <= 0)
		return {};
	const uint32_t w = uint32_t(width), h = uint32_t(width * m_canvasH / m_canvasW);
	QImage img;
	obs_enter_graphics();
	gs_texrender_t *tr = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	if (gs_texrender_begin(tr, w, h)) {
		vec4 clear;
		vec4_set(&clear, 0, 0, 0, 1);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, float(m_canvasW), 0.0f, float(m_canvasH), -100.0f, 100.0f);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		obs_source_video_render(m_pvTr);
		gs_blend_state_pop();
		gs_texrender_end(tr);
		gs_stagesurf_t *stage = gs_stagesurface_create(w, h, GS_RGBA);
		gs_stage_texture(stage, gs_texrender_get_texture(tr));
		uint8_t *data = nullptr;
		uint32_t linesize = 0;
		if (gs_stagesurface_map(stage, &data, &linesize)) {
			img = QImage(data, int(w), int(h), int(linesize), QImage::Format_RGBA8888).copy();
			gs_stagesurface_unmap(stage);
		}
		gs_stagesurface_destroy(stage);
	}
	gs_texrender_destroy(tr);
	obs_leave_graphics();
	return img;
}

QImage Engine::renderSourceThumbnail(const QString &sourceId, int width)
{
	obs_source_t *in = m_inputs.value(sourceId);
	const uint32_t sw = in ? obs_source_get_width(in) : 0, sh = in ? obs_source_get_height(in) : 0;
	if (!sw || !sh || width <= 0)
		return {};
	const uint32_t w = uint32_t(width), h = std::max<uint32_t>(1, uint32_t(double(width) * sh / sw));
	QImage img;
	obs_enter_graphics();
	gs_texrender_t *tr = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	if (gs_texrender_begin(tr, w, h)) {
		vec4 clear;
		vec4_set(&clear, 0, 0, 0, 1);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, float(sw), 0.0f, float(sh), -100.0f, 100.0f);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		obs_source_video_render(in);
		gs_blend_state_pop();
		gs_texrender_end(tr);
		gs_stagesurf_t *stage = gs_stagesurface_create(w, h, GS_RGBA);
		gs_stage_texture(stage, gs_texrender_get_texture(tr));
		uint8_t *data = nullptr;
		uint32_t linesize = 0;
		if (gs_stagesurface_map(stage, &data, &linesize)) {
			img = QImage(data, int(w), int(h), int(linesize), QImage::Format_RGBA8888).copy();
			gs_stagesurface_unmap(stage);
		}
		gs_stagesurface_destroy(stage);
	}
	gs_texrender_destroy(tr);
	obs_leave_graphics();
	return img;
}

bool Engine::sourceActive(const QString &sourceId) const
{
	obs_source_t *in = m_inputs.value(sourceId);
	return in && obs_source_active(in);
}

void Engine::holdScene(const QString &sceneId, bool on)
{
	if (on) {
		++m_heldScenes[sceneId];
		unparkScene(sceneId);
	} else {
		if (--m_heldScenes[sceneId] <= 0)
			m_heldScenes.remove(sceneId);
		if (!m_parkTimer.isActive())
			m_parkTimer.start(500);
	}
}

void Engine::setAllScenesShowing(bool on)
{
	if (on)
		for (const QString &id : m_scenes.keys())
			unparkScene(id);
	else
		if (!m_parkTimer.isActive())
		m_parkTimer.start(500);
	for (auto it = m_scenes.begin(); it != m_scenes.end(); ++it) {
		const bool showing = m_showingAll.contains(it.key());
		if (on && !showing) {
			obs_source_inc_showing(obs_scene_get_source(*it));
			m_showingAll.insert(it.key());
		} else if (!on && showing) {
			obs_source_dec_showing(obs_scene_get_source(*it));
			m_showingAll.remove(it.key());
		}
	}
}

namespace {
struct TapBox { Engine::AudioTap tap; void *param; };
QHash<QPair<QString, void *>, TapBox *> g_taps;
void onSourceAudio(void *p, obs_source_t *, const audio_data *a, bool muted)
{
	auto *box = static_cast<TapBox *>(p);
	const audio_output_info *info = audio_output_get_info(obs_get_audio());
	const int channels = info ? int(get_audio_channels(info->speakers)) : 2;
	box->tap(box->param, reinterpret_cast<const float *const *>(a->data), channels, int(a->frames), muted);
}
}

bool Engine::addAudioTap(const QString &sourceId, AudioTap tap, void *param)
{
	obs_source_t *in = m_inputs.value(sourceId);
	if (!in || g_taps.contains({sourceId, param}))
		return false;
	auto *box = new TapBox{tap, param};
	g_taps.insert({sourceId, param}, box);
	obs_source_add_audio_capture_callback(in, onSourceAudio, box);
	return true;
}

void Engine::removeAudioTap(const QString &sourceId, AudioTap, void *param)
{
	TapBox *box = g_taps.take({sourceId, param});
	if (!box)
		return;
	if (obs_source_t *in = m_inputs.value(sourceId))
		obs_source_remove_audio_capture_callback(in, onSourceAudio, box);
	delete box; // the callback is removed under OBS's audio lock, so nothing is using it
}

int Engine::audioSampleRate() const
{
	const audio_output_info *info = obs_get_audio() ? audio_output_get_info(obs_get_audio()) : nullptr;
	return info ? int(info->samples_per_sec) : 48000;
}

void Engine::sendCaption(const QString &text, double seconds)
{
	if (m_streamOutput && obs_output_active(m_streamOutput) && !text.trimmed().isEmpty())
		obs_output_output_caption_text2(m_streamOutput, text.toUtf8().constData(), seconds);
}

QVariantMap Engine::mediaInfo(const QString &sourceId) const
{
	obs_source_t *in = m_inputs.value(sourceId);
	if (!in)
		return {{"state", "none"}};
	const char *names[] = {"none", "playing", "opening", "buffering", "paused", "stopped", "ended", "error"};
	const int st = int(obs_source_media_get_state(in));
	return {{"state", st >= 0 && st < 8 ? names[st] : "none"}, {"timeMs", qint64(obs_source_media_get_time(in))},
		{"durationMs", qint64(obs_source_media_get_duration(in))}};
}

void Engine::mediaCommand(const QString &sourceId, const QString &cmd, qint64 ms)
{
	obs_source_t *in = m_inputs.value(sourceId);
	if (!in)
		return;
	if (cmd == "playPause") {
		const obs_media_state st = obs_source_media_get_state(in);
		if (st == OBS_MEDIA_STATE_STOPPED || st == OBS_MEDIA_STATE_ENDED)
			obs_source_media_restart(in);
		else
			obs_source_media_play_pause(in, st == OBS_MEDIA_STATE_PLAYING);
	} else if (cmd == "restart") {
		obs_source_media_restart(in);
	} else if (cmd == "stop") {
		obs_source_media_stop(in);
	} else if (cmd == "seek") {
		obs_source_media_set_time(in, std::max<qint64>(0, ms));
	}
}

QSize Engine::sourceSize(const QString &sourceId) const
{
	obs_source_t *in = m_inputs.value(sourceId);
	return in ? QSize(int(obs_source_get_width(in)), int(obs_source_get_height(in))) : QSize();
}

QVariantList Engine::listDevices(const QString &sourceId, const QString &kind) const
{
	QVariantList out;
	const char *prop = deviceProp(kind);
	obs_source_t *in = m_inputs.value(sourceId);
	if (!prop || (!in && !Permissions::allows(kind)))
		return out;
#ifndef Q_OS_WIN
	// Screen and camera capture list their devices only through a running source (screen capture
	// crashes without one, camera capture lists nothing), so macOS is asked directly.
	if (kind == "display" || kind == "window" || kind == "camera")
		return platform::captureDevices(kind);
#endif
	obs_properties_t *props = in ? obs_source_properties(in) : obs_get_source_properties(obsKind(kind));
	if (obs_property_t *p = props ? obs_properties_get(props, prop) : nullptr) {
		for (size_t i = 0, n = obs_property_list_item_count(p); i < n; i++) {
			const bool isInt = obs_property_list_format(p) == OBS_COMBO_FORMAT_INT;
			const char *v = isInt ? nullptr : obs_property_list_item_string(p, i);
			if (!isInt && !v)
				continue; // a placeholder with no value (macOS's display list starts with one), not "0"
			const QString value = v ? QString::fromUtf8(v) : QString::number(obs_property_list_item_int(p, i));
			if (value.isEmpty() && QString::fromUtf8(obs_property_list_item_name(p, i)).trimmed().isEmpty())
				continue; // a blank placeholder (macOS's camera list starts with one)
			// Disabled: open in another source (Windows cameras work in one place at a time). Still
			// listed, so a camera in the show isn't "not found"; pickers say it's in use.
			QVariantMap item{{"name", QString::fromUtf8(obs_property_list_item_name(p, i))}, {"value", value}};
			if (obs_property_list_item_disabled(p, i))
				item["inUse"] = true;
			out << item;
		}
	}
	obs_properties_destroy(props);
	return out;
}

obs_source *Engine::createPreviewInput(const QString &kind, const QVariantMap &settings) const
{
	if (!obsKindOrNull(kind) || !Permissions::allows(kind))
		return nullptr;
	Doc doc;
	doc.canvasW = m_canvasW, doc.canvasH = m_canvasH;
	Source s;
	s.kind = kind, s.settings = settings;
	obs_data_t *d = toData(inputSettings(doc, s));
	const QByteArray name = ("lobs_addpreview_" + QUuid::createUuid().toString(QUuid::Id128).left(8)).toUtf8();
	obs_source_t *src = obs_source_create_private(obsKind(kind), name.constData(), d);
	obs_data_release(d);
	return src;
}

// ---------- preview ----------

static void drawCallback(void *data, uint32_t cx, uint32_t cy)
{
	static_cast<Engine *>(data)->drawPreview(cx, cy);
}

static void fillRect(float x, float y, float w, float h)
{
	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	gs_matrix_scale3f(w, h, 1.0f);
	gs_draw_sprite(nullptr, 0, 1, 1);
	gs_matrix_pop();
}

void Engine::drawPreview(uint32_t cx, uint32_t cy)
{
	const int CW = m_editCanvasW.load(), CH = m_editCanvasH.load(); // the edited scene's canvas
	const float W = float(CW), H = float(CH);
	const bool other = m_otherSource != nullptr;
	const PreviewLayout lay = previewLayoutFor(cx, cy, CW, CH, other ? m_otherW.load() : 0, other ? m_otherH.load() : 0, m_otherLeft.load(), 16 * m_previewDpr.load(), m_pixelPerfect);
	const float scale = float(lay.scale);
	// Rounded the same way as the other layout's box, so a layout is the same size in either spot.
	const int vx = int(std::lround(lay.ox)), vy = int(std::lround(lay.oy));
	const int vw = int(std::lround(lay.ox + W * scale)) - vx, vh = int(std::lround(lay.oy + H * scale)) - vy;

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, W, 0.0f, H, -100.0f, 100.0f);
	// Canvas bounds: a hairline frame just outside, then a black fill, so what's on
	// the canvas reads clearly against the panel (scenes don't clear their own background).
	{
		gs_set_viewport(vx - 1, vy - 1, vw + 2, vh + 2);
		gs_ortho(0.0f, float(vw + 2), 0.0f, float(vh + 2), -100.0f, 100.0f);
		gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
		while (gs_effect_loop(solid, "Solid")) {
			vec4 c;
			vec4_set(&c, 0.30f, 0.30f, 0.30f, 1.0f);
			gs_effect_set_vec4(color, &c);
			fillRect(0, 0, float(vw + 2), float(vh + 2));
			vec4_set(&c, 0.0f, 0.0f, 0.0f, 1.0f);
			gs_effect_set_vec4(color, &c);
			fillRect(1, 1, float(vw), float(vh));
		}
		gs_ortho(0.0f, W, 0.0f, H, -100.0f, 100.0f);
	}
	// Exact ratios (2x, 1x, 1/2, 1/3...) draw straight to the screen: canvas pixels land on screen
	// pixels. Any other size first renders the canvas at full resolution, then scales it down with
	// Lanczos (small reductions) or area averaging (big ones), the filters OBS uses for its own
	// output, instead of plain bilinear sampling, so text and edges stay sharp.
	const double inv = scale > 0 ? 1.0 / scale : 1;
	const bool exact = scale >= 1 ? std::abs(scale - std::round(scale)) < 1e-4
				      : std::abs(inv - std::round(inv)) < 1e-4 && CW % int(std::round(inv)) == 0 && CH % int(std::round(inv)) == 0;
	gs_texture_t *full = nullptr;
	if (!exact && m_editSource) {
		if (!m_previewTex)
			m_previewTex = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
		gs_texrender_reset(m_previewTex);
		if (gs_texrender_begin(m_previewTex, uint32_t(CW), uint32_t(CH))) {
			vec4 clear;
			vec4_set(&clear, 0, 0, 0, 1);
			gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
			gs_ortho(0.0f, W, 0.0f, H, -100.0f, 100.0f);
			obs_source_video_render(m_editSource);
			renderPluginChannels();
			gs_texrender_end(m_previewTex);
			full = gs_texrender_get_texture(m_previewTex);
		}
	}
	gs_set_viewport(vx, vy, vw, vh);
	if (full) {
		gs_ortho(0.0f, float(vw), 0.0f, float(vh), -100.0f, 100.0f);
		gs_effect_t *effect = obs_get_base_effect(scale < 0.5f ? OBS_EFFECT_AREA : OBS_EFFECT_LANCZOS);
		gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), full);
		vec2 dim, dimInv;
		vec2_set(&dim, W, H);
		vec2_set(&dimInv, 1.0f / W, 1.0f / H);
		if (gs_eparam_t *p = gs_effect_get_param_by_name(effect, "base_dimension"))
			gs_effect_set_vec2(p, &dim);
		if (gs_eparam_t *p = gs_effect_get_param_by_name(effect, "base_dimension_i"))
			gs_effect_set_vec2(p, &dimInv);
		if (gs_eparam_t *p = gs_effect_get_param_by_name(effect, "undistort_factor"))
			gs_effect_set_float(p, 1.0f);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO); // the canvas is opaque: copy it
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(full, 0, uint32_t(vw), uint32_t(vh));
		gs_blend_state_pop();
		gs_ortho(0.0f, W, 0.0f, H, -100.0f, 100.0f); // overlays below are in canvas units
	} else if (m_editSource) {
		obs_source_video_render(m_editSource);
		renderPluginChannels();
	} else {
		obs_render_main_texture();
	}

	// Layers waiting on a permission: dimmed, with what they need (preview only).
	{
		QVector<BlockedLayer> blocked;
		QHash<QString, QImage> images;
		{
			QMutexLocker lock(&m_overlayLock);
			blocked = m_blocked;
			for (const BlockedLayer &b : blocked)
				if (!m_labelTextures.contains(b.label))
					images.insert(b.label, m_labelImages.value(b.label));
		}
		for (auto it = images.cbegin(); it != images.cend(); ++it) {
			const uint8_t *bits = it->constBits();
			m_labelTextures.insert(it.key(), gs_texture_create(uint32_t(it->width()), uint32_t(it->height()), GS_RGBA, 1, &bits, 0));
		}
		const float px = m_previewDpr / std::max(scale, 0.01f); // one point on screen, in canvas units
		for (const BlockedLayer &b : blocked) {
			gs_matrix_push();
			gs_matrix_translate3f(float(b.rect.center().x()), float(b.rect.center().y()), 0.0f);
			gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, RAD(float(b.rot)));
			gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
			gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
			while (gs_effect_loop(solid, "Solid")) {
				vec4 c;
				vec4_set(&c, 0.08f, 0.08f, 0.08f, 0.85f);
				gs_effect_set_vec4(color, &c);
				fillRect(float(-b.rect.width() / 2), float(-b.rect.height() / 2), float(b.rect.width()), float(b.rect.height()));
			}
			if (gs_texture_t *tex = m_labelTextures.value(b.label)) {
				// 14 points of text, whatever the zoom (the image is drawn at 2x).
				const float w = float(gs_texture_get_width(tex)) / 2 * px, h = float(gs_texture_get_height(tex)) / 2 * px;
				gs_effect_t *def = obs_get_base_effect(OBS_EFFECT_DEFAULT);
				gs_effect_set_texture(gs_effect_get_param_by_name(def, "image"), tex);
				gs_blend_state_push();
				gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA); // premultiplied
				gs_matrix_translate3f(-w / 2, -h / 2, 0.0f);
				while (gs_effect_loop(def, "Draw"))
					gs_draw_sprite(tex, 0, uint32_t(std::lround(w)), uint32_t(std::lround(h)));
				gs_blend_state_pop();
			}
			gs_matrix_pop();
		}
	}

	if (m_laser)
		obs_source_video_render(m_laser); // same trail viewers see
	if (m_ghost && obs_source_get_width(m_ghost)) {
		QPointF gp;
		{
			QMutexLocker lock(&m_overlayLock);
			gp = m_ghostPos;
		}
		gs_matrix_push();
		gs_matrix_translate3f(float(gp.x()), float(gp.y()), 0.0f);
		obs_source_video_render(m_ghost);
		gs_matrix_pop();
	}
	// Live ink while drawing (pencil / arrow): segments as thin rotated quads.
	QVector<QPointF> ink;
	float inkColor[4];
	double inkWidth;
	{
		QMutexLocker lock(&m_overlayLock);
		ink = m_ink;
		std::copy(m_inkColor, m_inkColor + 4, inkColor);
		inkWidth = m_inkWidth;
	}
	if (ink.size() >= 2) {
		gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
		vec4 c;
		vec4_set(&c, inkColor[0], inkColor[1], inkColor[2], inkColor[3]);
		while (gs_effect_loop(solid, "Solid")) {
			gs_effect_set_vec4(color, &c);
			for (int i = 1; i < ink.size(); ++i) {
				const QPointF a = ink[i - 1], b = ink[i];
				const double len = std::hypot(b.x() - a.x(), b.y() - a.y());
				if (len < 0.01)
					continue;
				gs_matrix_push();
				gs_matrix_translate3f(float(a.x()), float(a.y()), 0.0f);
				gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, float(std::atan2(b.y() - a.y(), b.x() - a.x())));
				fillRect(0, float(-inkWidth / 2), float(len + inkWidth / 4), float(inkWidth));
				gs_matrix_pop();
			}
		}
	}

	// Selection boxes, handles and snap guides, in canvas units.
	QVector<OverlayBox> boxes;
	QVector<OverlayGuide> guides;
	{
		QMutexLocker lock(&m_overlayLock);
		boxes = m_boxes;
		guides = m_guides;
	}
	if (!boxes.isEmpty() || !guides.isEmpty()) {
		gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
		gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
		const float px = m_previewDpr / scale; // one point on screen, in canvas units (2 device pixels on Retina)
		while (gs_effect_loop(solid, "Solid")) {
			vec4 c;
			vec4_set(&c, 1.0f, 0.31f, 0.85f, 1.0f);
			gs_effect_set_vec4(color, &c);
			for (const OverlayGuide &g : guides)
				g.vertical ? fillRect(float(g.at) - px * 0.5f, 0, px, H) : fillRect(0, float(g.at) - px * 0.5f, W, px);
			for (const OverlayBox &b : boxes) {
				if (b.drop) vec4_set(&c, 0.42f, 0.59f, 1.0f, 1.0f);
				else if (b.linked) vec4_set(&c, 0.22f, 0.74f, 0.97f, 1.0f);
				else vec4_set(&c, 0.49f, 0.42f, 1.0f, 1.0f);
				gs_effect_set_vec4(color, &c);
				gs_matrix_push();
				// Around the box's center, as layers rotate.
				gs_matrix_translate3f(float(b.rect.center().x()), float(b.rect.center().y()), 0.0f);
				gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, RAD(float(b.rot)));
				gs_matrix_translate3f(float(-b.rect.width() / 2), float(-b.rect.height() / 2), 0.0f);
				const float w = float(b.rect.width()), h = float(b.rect.height()), t = 2 * px;
				const float dash = b.group ? 0.5f : 1.0f;
				if (b.marquee) { // a faint fill inside the selection rectangle's outline
					vec4 f;
					vec4_set(&f, 0.42f, 0.59f, 1.0f, 0.10f);
					gs_effect_set_vec4(color, &f);
					fillRect(0, 0, w, h);
					gs_effect_set_vec4(color, &c);
				}
				fillRect(0, 0, w, t * dash);
				fillRect(0, h - t, w, t);
				fillRect(0, 0, t, h);
				fillRect(w - t, 0, t, h);
				if (b.crop) {
					// Crop handles: thick L brackets at the corners and bars mid-edge.
					const float L = 18 * px, T = 4 * px, M = 26 * px;
					vec4_set(&c, 1, 1, 1, 1);
					gs_effect_set_vec4(color, &c);
					fillRect(-T / 2, -T / 2, L, T), fillRect(-T / 2, -T / 2, T, L);
					fillRect(w - L + T / 2, -T / 2, L, T), fillRect(w - T / 2, -T / 2, T, L);
					fillRect(-T / 2, h - T / 2, L, T), fillRect(-T / 2, h - L + T / 2, T, L);
					fillRect(w - L + T / 2, h - T / 2, L, T), fillRect(w - T / 2, h - L + T / 2, T, L);
					fillRect(w / 2 - M / 2, -T / 2, M, T), fillRect(w / 2 - M / 2, h - T / 2, M, T);
					fillRect(-T / 2, h / 2 - M / 2, T, M), fillRect(w - T / 2, h / 2 - M / 2, T, M);
				} else if (b.handles) {
					const float hs = 10 * px;
					vec4_set(&c, 1, 1, 1, 1);
					gs_effect_set_vec4(color, &c);
					for (auto [hx, hy] : {std::pair{0.0f, 0.0f}, {w, 0.0f}, {0.0f, h}, {w, h}})
						fillRect(hx - hs / 2, hy - hs / 2, hs, hs);
					// Mid-edge handles stretch one side; smaller, so corners read as the main ones.
					const float es = 8 * px;
					if (w > 6 * hs)
						fillRect(w / 2 - es / 2, -es / 2, es, es), fillRect(w / 2 - es / 2, h - es / 2, es, es);
					if (h > 6 * hs)
						fillRect(-es / 2, h / 2 - es / 2, es, es), fillRect(w - es / 2, h / 2 - es / 2, es, es);
				}
				gs_matrix_pop();
			}
		}
	}
	drawOtherLayout(lay.other); // drawn here: the preview is a native window, so nothing in QML can sit on it
	gs_projection_pop();
	gs_viewport_pop();
}

// Audio effects run on the input itself. The chain is rebuilt only when its shape changes;
// value edits update the existing filters in place, so dragging a slider doesn't glitch audio.
void Engine::applyAudioFilters(obs_source_t *in, const QString &sourceId, const QVector<Filter> &fs)
{
	QString shape;
	for (const Filter &f : fs)
		shape += f.id + ":" + f.type + "|";
	if (m_audioFxSig.value(sourceId) != shape || !m_audioFxSig.contains(sourceId)) {
		std::vector<obs_source_t *> old;
		obs_source_enum_filters(in, [](obs_source_t *, obs_source_t *f, void *p) { static_cast<std::vector<obs_source_t *> *>(p)->push_back(f); }, &old);
		for (obs_source_t *f : old)
			obs_source_filter_remove(in, f);
		for (const Filter &f : fs) {
			const QString obsId = audioFxObsId(f.type);
			if (obsId.isEmpty())
				continue;
			obs_data_t *d = toData(QJsonObject::fromVariantMap(f.settings));
			obs_source_t *fsrc = obs_source_create_private(obsId.toUtf8().constData(), f.id.toUtf8().constData(), d);
			obs_data_release(d);
			if (!fsrc)
				continue; // filter module not available on this PC (e.g. NVIDIA effects)
			obs_source_set_enabled(fsrc, f.enabled);
			obs_source_filter_add(in, fsrc);
			obs_source_release(fsrc);
		}
		m_audioFxSig[sourceId] = shape;
		return;
	}
	for (const Filter &f : fs) {
		obs_source_t *fsrc = obs_source_get_filter_by_name(in, f.id.toUtf8().constData());
		if (!fsrc)
			continue;
		obs_data_t *d = toData(QJsonObject::fromVariantMap(f.settings));
		obs_source_update(fsrc, d);
		obs_data_release(d);
		if (obs_source_enabled(fsrc) != f.enabled)
			obs_source_set_enabled(fsrc, f.enabled);
		obs_source_release(fsrc);
	}
}

obs_source *Engine::inputSource(const QString &sourceId) const
{
	return m_inputs.value(sourceId);
}

obs_source *Engine::layerFilter(const QString &instanceId, const QString &filterId) const
{
	const auto it = m_items.constFind(instanceId);
	if (it == m_items.constEnd() || !it->inst)
		return nullptr;
	obs_source_t *f = obs_source_get_filter_by_name(it->inst, filterId.toUtf8().constData());
	obs_source_release(f); // borrowed: the layer keeps it
	return f;
}

obs_source *Engine::audioFilter(const QString &sourceId, const QString &filterId) const
{
	obs_source_t *in = m_inputs.value(sourceId);
	obs_source_t *f = in ? obs_source_get_filter_by_name(in, filterId.toUtf8().constData()) : nullptr;
	obs_source_release(f);
	return f;
}

QJsonObject Engine::inputFilters() const
{
	QJsonObject out;
	for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it) {
		QJsonArray ids;
		obs_source_enum_filters(it.value(), [](obs_source_t *, obs_source_t *f, void *p) {
			*static_cast<QJsonArray *>(p) << QString(obs_source_get_id(f)) + (obs_source_enabled(f) ? "" : " (off)");
		}, &ids);
		if (!ids.isEmpty())
			out[it.key()] = ids;
	}
	return out;
}

void Engine::refreshBrowser(const QString &sourceId)
{
	obs_source_t *in = m_inputs.value(sourceId);
	if (!in)
		return;
	// obs-browser exposes reload as a properties button.
	obs_properties_t *props = obs_source_properties(in);
	if (obs_property_t *p = obs_properties_get(props, "refreshnocache"))
		obs_property_button_clicked(p, in);
	obs_properties_destroy(props);
}

// What plugins put on output channels (Downstream Keyer's overlays, for one) draws over the
// program, so the preview of the live scene draws it too. Channel 0 is the program itself, the
// last one LOBS's laser pointer (drawn separately), and LOBS's audio channels draw nothing.
void Engine::renderPluginChannels()
{
	// What's there is also watched: a LOBS scene a plugin shows (Downstream Keyer puts it straight
	// on a channel) counts as showing, so its layers aren't parked and it draws.
	static QString seen;
	QString now;
	QSet<QString> scenes;
	for (int ch = 1; ch < MAX_CHANNELS - 1; ch++) {
		obs_source_t *o = obs_get_output_source(uint32_t(ch));
		if (o) {
			now += QString("%1=%2 ").arg(ch).arg(obs_source_get_name(o));
			if (obs_scene_from_source(o))
				scenes.insert(sceneIdOf(o));
			else if (obs_source_get_type(o) == OBS_SOURCE_TYPE_TRANSITION)
				if (obs_source_t *a = obs_transition_get_active_source(o)) {
					if (obs_scene_from_source(a))
						scenes.insert(sceneIdOf(a)), now += QString("(%1) ").arg(obs_source_get_name(a));
					obs_source_release(a);
				}
		}
		if (o && m_editId == m_programId && (obs_source_get_output_flags(o) & OBS_SOURCE_VIDEO))
			obs_source_video_render(o);
		obs_source_release(o);
	}
	if (now != seen) {
		blog(LOG_INFO, "[lobs] output channels: %s", qPrintable(now));
		seen = now;
		QMetaObject::invokeMethod(this, [this, scenes] { setPluginScenes(scenes); }, Qt::QueuedConnection);
	}
}

void Engine::setPluginScenes(const QSet<QString> &scenes)
{
	if (scenes == m_pluginScenes)
		return;
	m_pluginScenes = scenes;
	for (const QString &id : scenes)
		unparkScene(id);
	if (!m_parkTimer.isActive())
		m_parkTimer.start(500); // parks what stopped showing
}

// The scenes the engine makes: the listed ones, and their vertical layouts while vertical is on.
QStringList Engine::scenesToMake(const Doc &doc) const
{
	QStringList out = doc.sceneOrder;
	if (doc.verticalOn && m_vcanvas)
		for (const QString &id : doc.sceneOrder)
			if (const QString v = doc.verticalLayout(id); !v.isEmpty())
				out << v;
	return out;
}

void Engine::updateEditCanvas()
{
	const QSize c = m_lastDoc.canvasOf(m_editId);
	m_editCanvasW = c.width() > 0 ? c.width() : m_canvasW;
	m_editCanvasH = c.height() > 0 ? c.height() : m_canvasH;
	// The other layout, for the corner of the preview.
	const Doc &d = m_lastDoc;
	const QString vOf = d.scenes.value(m_editId).verticalOf;
	const QString other = !d.verticalOn ? QString() : !vOf.isEmpty() ? vOf : d.verticalLayout(m_editId);
	obs_scene_t *os = m_scenes.value(other);
	obs_source_t *src = os ? obs_scene_get_source(os) : nullptr;
	if (src != m_otherSource) {
		if (src)
			obs_source_inc_showing(src);
		if (m_otherSource)
			obs_source_dec_showing(m_otherSource);
		m_otherSource = src;
	}
	const QSize oc = d.canvasOf(other);
	m_otherW = oc.width() > 0 ? oc.width() : m_canvasW;
	m_otherH = oc.height() > 0 ? oc.height() : m_canvasH;
	m_otherLeft = !vOf.isEmpty();
}

void Engine::drawOtherLayout(const QRectF &r)
{
	obs_source_t *src = m_otherSource;
	const int OW = m_otherW.load(), OH = m_otherH.load();
	if (!src || OW <= 0 || OH <= 0 || r.isEmpty())
		return;
	// A hairline frame just outside, as around the edited canvas.
	const int x = int(std::lround(r.x())) - 1, y = int(std::lround(r.y())) - 1;
	const int w = int(std::lround(r.right())) + 1 - x, h = int(std::lround(r.bottom())) + 1 - y;
	gs_set_viewport(x, y, w, h);
	gs_ortho(0.0f, float(w), 0.0f, float(h), -100.0f, 100.0f);
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	while (gs_effect_loop(solid, "Solid")) {
		vec4 c;
		vec4_set(&c, 0.30f, 0.30f, 0.30f, 1.0f);
		gs_effect_set_vec4(color, &c);
		fillRect(0, 0, float(w), float(h));
		vec4_set(&c, 0.0f, 0.0f, 0.0f, 1.0f);
		gs_effect_set_vec4(color, &c);
		fillRect(1, 1, float(w - 2), float(h - 2));
	}
	// The layout, fitted inside the frame.
	const float s = std::min(float(w - 2) / OW, float(h - 2) / OH);
	const int iw = int(OW * s), ih = int(OH * s);
	gs_set_viewport(x + (w - iw) / 2, y + (h - ih) / 2, iw, ih);
	gs_ortho(0.0f, float(OW), 0.0f, float(OH), -100.0f, 100.0f);
	obs_source_video_render(src);
}

void Engine::syncVerticalProgram()
{
	if (!m_vcanvas)
		return;
	const QString v = m_lastDoc.verticalOn ? m_lastDoc.verticalLayout(m_programId) : QString(); // off: nothing on it
	if (!v.isEmpty())
		unparkScene(v);
	obs_scene_t *s = m_scenes.value(v);
	obs_canvas_set_channel(m_vcanvas, 0, s ? obs_scene_get_source(s) : nullptr);
}

QString Engine::sceneIdOf(obs_source *scene) const
{
	for (auto it = m_scenes.cbegin(); it != m_scenes.cend(); ++it)
		if (obs_scene_get_source(it.value()) == scene)
			return it.key();
	return {};
}

obs_source_t *Engine::sceneSource(const QString &sceneId) const
{
	obs_scene_t *s = m_scenes.value(sceneId);
	return s ? obs_scene_get_source(s) : nullptr;
}

void Engine::attachPreview(void *hwnd, int width, int height)
{
	if (!m_started || m_display)
		return;
	gs_init_data init = {};
#ifdef Q_OS_WIN
	init.window.hwnd = hwnd;
#else
	init.window.view = (__bridge id)hwnd; // the NSView of the preview window
#endif
	init.cx = uint32_t(width);
	init.cy = uint32_t(height);
	init.format = GS_BGRA;
	init.zsformat = GS_ZS_NONE;
	m_display = obs_display_create(&init, 0xFF171717);
	obs_display_add_draw_callback(m_display, drawCallback, this);
}

// After the main mix is drawn, inside the graphics context (taking it from a tick callback would
// lock in the opposite order to libobs's own rendering, and can deadlock).
static void sharedPreviewRendered(void *data)
{
	static_cast<Engine *>(data)->renderSharedPreview();
}

void Engine::startSharedPreview(std::function<void()> onFrame)
{
	if (!m_started || m_spOn)
		return;
	m_spOnFrame = std::move(onFrame);
	graphicsDevice(); // here, on the UI thread, not later on the render thread
	m_spOn = true;
	obs_add_main_rendered_callback(sharedPreviewRendered, this);
}

void Engine::stopSharedPreview()
{
	if (!m_spOn)
		return;
	obs_remove_main_rendered_callback(sharedPreviewRendered, this);
	m_spOn = false;
	obs_enter_graphics();
	for (int i = 0; i < kSharedFrames; i++) {
		gs_texture_destroy(m_spTex[i]);
		m_spTex[i] = nullptr;
#ifdef Q_OS_WIN
		if (m_spDone[i])
			static_cast<ID3D11Query *>(m_spDone[i])->Release();
		m_spDone[i] = nullptr;
#endif
	}
	obs_leave_graphics();
	m_spW = m_spH = 0;
	m_spPublished = -1;
	m_spPending = 0;
}

Engine::SharedFrames Engine::sharedPreviewFrames() const
{
	QMutexLocker lock(&m_spLock);
	return m_spFrames;
}

void *Engine::graphicsDevice() const
{
	if (!m_spDevice && m_started) { // asked once; the device lives as long as the engine
		obs_enter_graphics();
		m_spDevice = gs_get_device_obj();
		obs_leave_graphics();
	}
	return m_spDevice;
}

// The UI reads these frames on its own GPU device, with nothing between the two devices to keep
// them in step. So a frame is published only once the GPU has finished drawing it (an event
// query, polled without waiting), and a frame is never drawn into a texture the UI may still be
// sampling: the last few it took (it keeps up to two frames in flight) or the newest.
void Engine::renderSharedPreview()
{
	const int w = m_spWantW, h = m_spWantH;
	if (w < 16 || h < 16 || w > 16384 || h > 16384)
		return;
	// Once per video frame, however many mixes are drawn in it.
	const uint64_t frame = obs_get_video_frame_time();
	if (frame == m_spLastFrame)
		return;
	m_spLastFrame = frame;
	obs_enter_graphics();
#ifdef Q_OS_WIN
	auto *dev = static_cast<ID3D11Device *>(gs_get_device_obj());
	ID3D11DeviceContext *ctx = nullptr;
	if (dev)
		dev->GetImmediateContext(&ctx);
#endif
	if (w != m_spW || h != m_spH || !m_spTex[0]) {
		// New size: fresh textures; the UI opens them again when the generation changes (and
		// keeps showing its last frame until one at the new size is ready).
		SharedFrames f;
		for (int i = 0; i < kSharedFrames; i++) {
			gs_texture_destroy(m_spTex[i]);
			m_spTex[i] = gs_texture_create(uint32_t(w), uint32_t(h), GS_RGBA, 1, nullptr, GS_RENDER_TARGET | GS_SHARED_TEX);
			f.handles[i] = m_spTex[i] ? gs_texture_get_shared_handle(m_spTex[i]) : 0;
#ifdef Q_OS_WIN
			if (!m_spDone[i] && dev) {
				D3D11_QUERY_DESC qd{D3D11_QUERY_EVENT, 0};
				ID3D11Query *q = nullptr;
				if (SUCCEEDED(dev->CreateQuery(&qd, &q)))
					m_spDone[i] = q;
			}
#endif
		}
		m_spW = w, m_spH = h;
		m_spPublished = -1;
		m_spPending = 0;
		f.width = w, f.height = h, f.generation = m_spGen + 1;
		{
			QMutexLocker lock(&m_spLock);
			m_spFrames = f;
		}
		m_spGen = f.generation;
	}
	// Frames the GPU has finished since last time: the newest of them is published.
	bool published = false;
	int newestDone = -1;
	uint64_t newestSeq = 0;
	for (int i = 0; i < kSharedFrames; i++) {
		if (!(m_spPending & (1u << i)))
			continue;
		bool done = true;
#ifdef Q_OS_WIN
		if (ctx && m_spDone[i])
			done = ctx->GetData(static_cast<ID3D11Query *>(m_spDone[i]), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
#endif
		if (!done)
			continue;
		m_spPending &= ~(1u << i);
		if (m_spSeq[i] > newestSeq)
			newestSeq = m_spSeq[i], newestDone = i;
	}
	if (newestDone >= 0 && (m_spPublished < 0 || newestSeq > m_spSeq[m_spPublished])) {
		m_spPublished = newestDone;
		published = true;
	}
	// Draw into a texture that's free: not held by the UI, not the newest, not still drawing.
	const uint32_t busy = m_spHeld | m_spPending | (m_spPublished >= 0 ? 1u << m_spPublished : 0u);
	int k = -1;
	for (int i = 0; i < kSharedFrames && k < 0; i++)
		if (!(busy & (1u << i)) && m_spTex[i])
			k = i;
	if (k >= 0) {
		gs_texture_t *prevRt = gs_get_render_target();
		gs_zstencil_t *prevZs = gs_get_zstencil_target();
		gs_viewport_push();
		gs_projection_push();
		gs_set_render_target(m_spTex[k], nullptr);
		gs_set_viewport(0, 0, w, h);
		vec4 clear;
		vec4_from_rgba(&clear, 0xFF171717);
		gs_clear(GS_CLEAR_COLOR, &clear, 1.0f, 0);
		drawPreview(uint32_t(w), uint32_t(h));
		gs_set_render_target(prevRt, prevZs);
		gs_projection_pop();
		gs_viewport_pop();
		m_spSeq[k] = ++m_spSeqNext;
#ifdef Q_OS_WIN
		if (ctx && m_spDone[k])
			ctx->End(static_cast<ID3D11Query *>(m_spDone[k]));
		m_spPending |= 1u << k;
#else
		m_spPublished = k, published = true;
#endif
		gs_flush(); // on its way to the GPU; published once the query says it's done
	}
#ifdef Q_OS_WIN
	if (ctx)
		ctx->Release();
#endif
	obs_leave_graphics();
	if (published && m_spOnFrame)
		m_spOnFrame();
}

void Engine::resizePreview(int width, int height)
{
	// A squeezed layout can report a zero or negative size; a negative one cast to uint32_t
	// asked the GPU for a 4-billion-pixel swap chain and took the app down.
	if (m_display && width > 0 && height > 0)
		obs_display_resize(m_display, uint32_t(width), uint32_t(height));
}

void Engine::setBlockedLayers(const QVector<BlockedLayer> &layers)
{
	QMutexLocker lock(&m_overlayLock);
	m_blocked = layers;
	for (const BlockedLayer &b : layers) {
		if (m_labelImages.contains(b.label))
			continue;
		// Drawn at 2x and shown at a fixed size on screen, so it stays sharp at any zoom.
		QFont f = QGuiApplication::font();
		f.setPixelSize(28);
		f.setWeight(QFont::DemiBold);
		const QFontMetrics fm(f);
		QImage img(fm.horizontalAdvance(b.label) + 40, fm.height() + 20, QImage::Format_RGBA8888_Premultiplied);
		img.fill(Qt::transparent);
		QPainter p(&img);
		p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0x25, 0x25, 0x25, 235));
		p.drawRoundedRect(QRectF(img.rect()), img.height() / 2.0, img.height() / 2.0);
		p.setPen(QColor("#fbbf24"));
		p.setFont(f);
		p.drawText(img.rect(), Qt::AlignCenter, b.label);
		p.end();
		m_labelImages.insert(b.label, img);
	}
}

void Engine::setOverlay(const QVector<OverlayBox> &boxes, const QVector<OverlayGuide> &guides)
{
	QMutexLocker lock(&m_overlayLock);
	m_boxes = boxes;
	m_guides = guides;
}

void Engine::pollStats()
{
	const double fps = obs_get_active_fps();
	const double ms = double(obs_get_average_frame_time_ns()) / 1e6;
	m_stats = QString("%1 fps · %2 ms").arg(fps, 0, 'f', 0).arg(ms, 0, 'f', 2);
	pollLiveStats();
	emit statsChanged();
	if (m_streaming)
		pollStreamStats();
}

void Engine::shutdown()
{
	if (!m_started)
		return;
	m_statsTimer.stop();
	releaseOutputs();
	for (auto *vm : m_volmeters)
		obs_volmeter_destroy(vm);
	m_volmeters.clear();
	if (m_display) {
		obs_display_destroy(m_display);
		m_display = nullptr;
	}
	stopSharedPreview();
	if (m_previewTex) {
		obs_enter_graphics();
		gs_texrender_destroy(m_previewTex);
		obs_leave_graphics();
		m_previewTex = nullptr;
	}
	if (!m_labelTextures.isEmpty()) {
		obs_enter_graphics();
		for (gs_texture_t *t : m_labelTextures)
			gs_texture_destroy(t);
		obs_leave_graphics();
		m_labelTextures.clear();
	}
	setEditScene({});
	setAllScenesShowing(false);
	obs_set_output_source(0, nullptr);
	obs_set_output_source(MAX_CHANNELS - 1, nullptr);
	m_laserTimer.stop();
	if (m_laser) {
		obs_source_release(m_laser);
		m_laser = nullptr;
	}
	stopTransitionPreview();
	if (m_ghost) {
		obs_source_release(m_ghost);
		m_ghost = nullptr;
	}
	if (m_transition) {
		obs_source_release(m_transition);
		m_transition = nullptr;
	}
	for (int ch = 0; ch < MAX_CHANNELS; ch++)
		obs_set_output_source(uint32_t(ch), nullptr);
	for (const QString &id : m_items.keys())
		removeItem(id);
	if (m_vcanvas)
		obs_canvas_set_channel(m_vcanvas, 0, nullptr);
	for (obs_scene_t *s : m_scenes)
		obs_scene_release(s);
	m_scenes.clear();
	if (m_vcanvas) { // after its scenes
		obs_canvas_remove(m_vcanvas);
		obs_canvas_release(m_vcanvas);
		m_vcanvas = nullptr;
	}
	bool browsers = false;
	for (obs_source_t *in : m_inputs) {
		browsers |= in && QByteArray(obs_source_get_id(in)) == "browser_source";
		obs_source_release(in);
	}
	m_inputs.clear();
#ifndef Q_OS_WIN
	// obs-browser runs CEF on Qt's event loop on macOS, and that loop has stopped by now.
	// Closing a page takes a few turns of it, and CEF won't shut down with one still open.
	if (browsers) {
		QElapsedTimer t;
		t.start();
		while (t.elapsed() < 750)
			QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
	}
#else
	Q_UNUSED(browsers);
#endif
	obs_shutdown();
	m_started = false;
}
