// Engine: owns libobs. Applies the LOBS document to libobs (incrementally), renders the
// edited scene + selection overlay into the preview window, and records.
#pragma once
#include <QJsonObject>
#include <algorithm>
#include <cmath>

#include "model/doc.h"
#include "settings.h"

#include <QElapsedTimer>
#include <atomic>

#include <QHash>
#include <QColor>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QSet>

struct obs_source;
struct obs_scene;
struct obs_scene_item;
struct obs_display;
struct obs_output;
struct obs_encoder;
struct obs_service;
struct obs_volmeter;
struct os_cpu_usage_info;

// Preview scale, in physical pixels. Pixel-perfect: snap to an exact ratio (2x, 1x, 1/2, 1/3,
// 1/4...) that divides the canvas evenly, so canvas pixels land on screen pixels and text stays
// crisp, but only when that keeps at least 80% of the full-fit size. Otherwise, plain fit.
inline double previewScaleFor(double availW, double availH, int canvasW, int canvasH, bool pixelPerfect)
{
	if (availW <= 0 || availH <= 0 || canvasW <= 0 || canvasH <= 0)
		return 1;
	const double fit = std::min(availW / canvasW, availH / canvasH);
	if (!pixelPerfect)
		return fit;
	if (fit >= 1) {
		const double up = std::floor(fit);
		return up >= fit * 0.8 ? up : fit;
	}
	for (int n = 1; n <= 8; ++n) {
		if (canvasW % n || canvasH % n)
			continue; // not an even split: some canvas pixels would straddle screen pixels
		const double c = 1.0 / n;
		if (c <= fit + 1e-9)
			return c >= fit * 0.8 ? c : fit;
	}
	return fit;
}

struct OverlayBox {
	QRectF rect; // canvas px
	double rot = 0;
	bool handles = false;
	bool drop = false; // drop-target preview while dragging from Sources
	bool group = false;
	bool linked = false;
	bool crop = false; // crop mode: edge and corner crop handles instead of resize handles
	bool marquee = false; // the selection rectangle being dragged out
};
// A layer whose source waits on a macOS permission: the preview (never the output) dims it and
// says what's needed.
struct BlockedLayer {
	QRectF rect; // canvas px
	double rot = 0;
	QString label; // "Needs Screen Recording"
};
struct OverlayGuide {
	bool vertical;
	double at;
};

QString obsOutputError(obs_output *out); // ": message" from OBS for a failed output, or ""

class Engine : public QObject {
	Q_OBJECT
public:
	explicit Engine(QObject *parent = nullptr);
	~Engine() override;

	bool start(const QString &runtimeRoot);
	void shutdown();
	bool started() const { return m_started; }
	QString status() const { return m_status; }
	QString stats() const { return m_stats; }
	QVariantMap layerResources() const; // for QA: how many layers hold filters, text and shape renderers
	bool recording() const { return m_recording; }
	bool streaming() const { return m_streaming; }
	bool live() const { return m_streaming || m_recording; }
	QString lastRecording() const { return m_lastRecording; }
	QVariantMap streamStats() const { return m_streamStats; }

	// Document → libobs
	void apply(const lobs::Doc &doc);
	void setEditScene(const QString &sceneId);
	// Program output goes through a transition; `animate` plays it (Show), otherwise it cuts.
	void setProgramScene(const QString &sceneId, bool animate = false);
	QString programScene() const { return m_programId; }
	QImage renderThumbnail(const QString &sceneId, int width); // reads back a small frame of a scene
	QImage renderSourceThumbnail(const QString &sourceId, int width); // one input, at its own aspect
	bool sourceActive(const QString &sourceId) const; // running because a shown scene uses it
	void setAllScenesShowing(bool on);
	// A scene projector keeps its scene showing (and its layers' filters attached) while open.
	void holdScene(const QString &sceneId, bool on); // keep every scene's sources active (for live thumbnails)
	QSize sourceSize(const QString &sourceId) const; // natural size, or 0x0 if unknown
	// Media sources (video and audio files): {state: playing|paused|stopped|ended|none, timeMs, durationMs}.
	QVariantMap mediaInfo(const QString &sourceId) const;
	// cmd: playPause, restart, stop, seek (to ms).
	void mediaCommand(const QString &sourceId, const QString &cmd, qint64 ms = 0);
	// Live captions: a copy of a source's audio (float planar at sampleRate(), on OBS's audio
	// thread), and caption text sent inside the stream (CEA-608) while streaming.
	using AudioTap = void (*)(void *param, const float *const *planes, int channels, int frames, bool muted);
	bool addAudioTap(const QString &sourceId, AudioTap tap, void *param);
	void removeAudioTap(const QString &sourceId, AudioTap tap, void *param);
	int audioSampleRate() const;
	void sendCaption(const QString &text, double seconds);
	QVariantList listDevices(const QString &sourceId, const QString &kind) const;

	// Preview
	void attachPreview(void *hwnd, int width, int height);
	void resizePreview(int width, int height);
	void setPixelPerfect(bool on) { m_pixelPerfect = on; }
	// Laser pointer: a fading trail over the program output (and the preview). Not a layer.
	void laserPoint(QPointF canvasPos, bool newStroke);
	void setLaserColor(const QColor &c) { m_laserColor = c; }
	void setLaserLife(int ms) { m_laserLife = ms; }
	// A preview-only image at a canvas position (the text tool's "A" and caret). Null clears it.
	void setGhost(const QImage &img, QPointF canvasPos);
	// Live ink while drawing a pencil stroke or arrow (preview only).
	void setInk(const QVector<QPointF> &canvasPts, const QString &color, double width);
	void setOverlay(const QVector<OverlayBox> &boxes, const QVector<OverlayGuide> &guides);
	void setBlockedLayers(const QVector<BlockedLayer> &layers);
	int canvasW() const { return m_canvasW; }
	obs_source *sceneSource(const QString &sceneId) const; // borrowed; null if unknown
	int canvasH() const { return m_canvasH; }

	// Outputs (engine_output.cpp)
	void setSettings(const AppSettings &s) { m_settings = s; }
	const AppSettings &settings() const { return m_settings; }
	QVariantList encoders() const;               // [{value, name}] usable on this PC, best first
	bool kindAvailable(const QString &kind) const; // this platform and engine can capture it (no game capture on macOS)
	QString resolveEncoder() const;               // settings.encoder, or the best available for "auto"
	QString resolveRecordEncoder() const;         // HEVC for quality recordings where VideoToolbox has it, else resolveEncoder()
	QStringList services() const;                 // rtmp-services names
	QVariantList servers(const QString &service) const; // [{name, value}]
	bool resetVideo();                            // apply output size/FPS (only while nothing is live)
	bool startStreaming();
	void stopStreaming();
	Q_INVOKABLE void recordClip(int seconds);
	void refreshBrowser(const QString &sourceId);
	QJsonObject inputFilters() const; // source id -> [libobs filter ids], for self-tests

	// Status bar / stats (engine_extras.cpp)
	QVariantMap liveStats() const { return m_liveStats; }
	// Replay buffer: keeps the last N seconds; saveReplay() writes them to a file.
	bool replayActive() const;
	bool startReplay();
	void stopReplay();
	void saveReplay();
	void replaySavedNow();
	QString lastReplay() const { return m_lastReplay; }
	// Virtual camera: the program output as a webcam ("OBS Virtual Camera").
	bool vcamAvailable() const;
	bool vcamActive() const;
	bool installVirtualCam();
	bool startVirtualCam();
	void stopVirtualCam();
	// Push-to-talk: these sources stay muted unless their key is held.
	void setPushToTalk(const QSet<QString> &sourceIds);
	void pushToTalk(const QString &sourceId, bool held);
	void startRecording();
	void stopRecording();
	QString lastError() const { return m_lastError; }

	// Audio meters: peak dBFS per source id, [left, right]
	QVariantMap audioLevels() const;

	// Used by the draw callback (graphics thread).
	void drawPreview(uint32_t cx, uint32_t cy);

signals:
	void statusChanged();
	void statsChanged();
	void recordingChanged();
	void recordingFinished(bool ok, const QString &path, qint64 bytes);
	void streamingChanged();
	void streamStatsChanged();
	void streamError(const QString &message);
	void replayChanged();
	void replaySaved(const QString &path);
	void vcamChanged();
	void bandwidthTestProgress(const QVariantMap &p);
	void bandwidthTestDone(const QVariantMap &result);

private:
	struct Item {
		obs_source *inst = nullptr;
		obs_source *text = nullptr; // per-layer text renderer (text sources only)
		QString textSig;
		int textGen = 0; // bumped when parking frees the text renderer; the next one gets a new name
		obs_source *shape = nullptr; // per-layer vector graphic (shape sources only)
		QString shapeSig;
		obs_scene_item *item = nullptr;
		QString sceneId, cropSig, filterSig;
		QString filterValueSig, maskPath;
		// What this layer was last applied with: when nothing here changed, apply() skips it.
		struct Applied {
			lobs::Transform t; lobs::Crop c; lobs::Visibility v; int order = -1;
			QString sceneId, sourceKind; QVariantMap sourceSettings; QVector<lobs::Filter> filters;
			bool operator==(const Applied &) const = default;
		};
		Applied applied;
		bool hasApplied = false;
		obs_source *maskFilter = nullptr, *opacityFilter = nullptr; // kept, updated in place
		double opacity = 1;
		bool parked = false; // filters detached while its scene isn't showing (LOBS-034)
	};
	// Each OBS filter, and a scene item with a scale filter, keeps a render target the size of the
	// layer's source until it's destroyed. So a layer whose scene stopped showing gives its filters
	// and scale filter back, and gets them again just before the scene shows (LOBS-034).
	void parkHiddenFilters();
	void unparkScene(const QString &sceneId);
	QTimer m_parkTimer;
	QHash<QString, int> m_heldScenes; // scene -> number of projectors holding it
	QString m_unparkTarget; // the scene unparkScene() is preparing
	bool m_applying = false; // apply() is running (it must not re-enter)
	bool sceneWanted(const QString &sceneId) const { return sceneId == m_editId || sceneId == m_programId || sceneId == m_unparkTarget || m_showingAll.contains(sceneId) || m_heldScenes.contains(sceneId); }
	lobs::Doc m_lastDoc;
	void setStatus(const QString &s);
	void pollStats();
	void removeItem(const QString &instId);

	QString m_status = "Starting engine…";
	QString m_stats;
	QString m_lastRecording;
	bool m_started = false;
	bool m_recording = false;
	int m_canvasW = 1920, m_canvasH = 1080;

	QHash<QString, obs_source *> m_inputs; // source id -> input (named by id)
	QHash<QString, QString> m_inputSig;
	QHash<QString, lobs::Source> m_appliedInput; // input as last applied (apply() skips unchanged ones)
	int m_appliedCanvasW = 0, m_appliedCanvasH = 0;
	QHash<QString, QString> m_audioFxSig; // source id -> filter chain shape (ids + types)
	void applyAudioFilters(obs_source *in, const QString &sourceId, const QVector<lobs::Filter> &fs);
	QHash<QString, int> m_audioChannel;
	QHash<QString, obs_scene *> m_scenes; // scene id -> private scene
	QHash<QString, Item> m_items;          // instance id -> lobs_instance + scene item
	QString m_editId, m_programId;
	obs_source *m_transition = nullptr;
	QString m_transitionType;
	obs_source *ensureTransition();
	obs_source *createTransition(const lobs::Transition &t, const char *name);
	lobs::Transition m_trDef{"tr_fade", "Fade", "fade", 300, {}}; // the show's transition in use
	// Transition preview (the editor): A and B cards swapped by a private copy of a transition.
	obs_source *m_pvA = nullptr, *m_pvB = nullptr, *m_pvTr = nullptr;
	bool m_pvAtB = false;

public:
	void previewTransition(const lobs::Transition &t); // plays it once, A to B (or back)
	void stopTransitionPreview();
	QImage renderTransitionPreview(int width);

private:
	obs_source *m_editSource = nullptr; // showing-ref held while edited
	QSet<QString> m_showingAll;         // scenes held showing for thumbnails

	obs_display *m_display = nullptr;
	bool m_pixelPerfect = true;
	obs_source *m_laser = nullptr;
	struct LaserPt { QPointF at; qint64 t; bool newStroke; };
	QVector<LaserPt> m_laserPts;
	QTimer m_laserTimer;
	QElapsedTimer m_laserClock;
	void tickLaser();
	QVector<QPointF> m_ink;
	float m_inkColor[4] = {1, 1, 1, 1};
	double m_inkWidth = 4;
	struct gs_texture_render *m_previewTex = nullptr; // full-size canvas for high-quality preview scaling
	QMutex m_overlayLock;
	QVector<OverlayBox> m_boxes;
	QVector<BlockedLayer> m_blocked;
	QHash<QString, QImage> m_labelImages;          // label -> image, made on the UI thread
	QHash<QString, struct gs_texture *> m_labelTextures; // label -> texture, graphics thread only
	QVector<OverlayGuide> m_guides;

	AppSettings m_settings;
	QString m_lastError;
	obs_output *m_recOutput = nullptr;
	obs_output *m_streamOutput = nullptr;
	obs_service *m_service = nullptr;
	obs_encoder *m_streamVenc = nullptr, *m_streamAenc = nullptr;
	obs_encoder *m_recVenc = nullptr, *m_recAenc = nullptr;
	bool m_streaming = false;
	QVariantMap m_streamStats;
	QElapsedTimer m_streamClock;
	quint64 m_lastBytes = 0;
	qint64 m_lastBytesAt = 0;
	void pollStreamStats();
	void releaseOutputs();
	obs_encoder *makeVideoEncoder(const QString &id, bool forRecording, const char *name);
	// Setup's bandwidth test (a separate output; never goes live on Twitch).
	obs_output *m_bwOutput = nullptr;
	obs_service *m_bwService = nullptr;
	obs_encoder *m_bwVenc = nullptr, *m_bwAenc = nullptr;
	QTimer *m_bwTimer = nullptr;
	QElapsedTimer m_bwClock;
	qint64 m_bwStartBytes = -1, m_bwStartMs = 0;
	int m_bwTarget = 0, m_bwSeconds = 10;
	QVector<int> m_bwSamples;
	void pollBandwidthTest();

public:
	bool startBandwidthTest(const QString &server, const QString &key, int kbps, int seconds);
	void stopBandwidthTest();
	bool bandwidthTestRunning() const { return m_bwOutput; }

private:

	struct Meter {
		std::atomic<float> lr[2] = {-60.0f, -60.0f};
	};
	QHash<QString, obs_volmeter *> m_volmeters;
	QHash<QString, Meter *> m_meters;
	void ensureMeter(const QString &sourceId, obs_source *in);
	void removeMeter(const QString &sourceId);

	QTimer m_statsTimer, m_recordTimer;

	QString m_root;
	QVariantMap m_liveStats;
	os_cpu_usage_info *m_cpu = nullptr;
	// Frame counters at each stats poll over the last ~10 s, for the status bar's warning (LOBS-005).
	struct FrameSample { qint64 ms; uint32_t lagged, rendered, skipped, encoded; };
	QVector<FrameSample> m_frameSamples;
	QElapsedTimer m_frameClock;
	QElapsedTimer m_recClock;
	void pollLiveStats();
	obs_output *m_replayOutput = nullptr;
	obs_encoder *m_replayVenc = nullptr, *m_replayAenc = nullptr;
	QString m_lastReplay;
	obs_output *m_vcamOutput = nullptr;
	QSet<QString> m_pttBound, m_pttHeld;
	QHash<QString, bool> m_docMuted; // the show's mute state per audio source
	void applyMutes();
	QColor m_laserColor = QColor(255, 60, 60);
	int m_laserLife = 500;
	obs_source *m_ghost = nullptr;
	QPointF m_ghostPos;
};
