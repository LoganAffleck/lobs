// App controller: the bridge between QML, the document store and the engine.
// QML reads plain lists/maps from here and calls invokables; every edit goes through Store.
#pragma once

#include "engine.h"
#include "eyedropper.h"
#include "hotkeys.h"
#include "model/obs_import.h"
#include "model/store.h"

#include <QKeyEvent>
#include <QObject>
#include <QPointer>
#include <QWindow>
#include <QPointF>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QElapsedTimer>
#include <QTimer>
#include <QVariant>

class App : public QObject {
	Q_OBJECT
	Q_PROPERTY(QVariantList scenes READ scenesCached NOTIFY scenesChanged)
	Q_PROPERTY(QVariantList pool READ poolCached NOTIFY poolChanged)
	Q_PROPERTY(QVariantList layers READ layersCached NOTIFY layersChanged)
	Q_PROPERTY(QVariantMap inspector READ inspectorCached NOTIFY inspectorChanged)
	// The selection's x, y, w, h and rot alone: follows a canvas drag every frame (LOBS-025).
	Q_PROPERTY(QVariantMap inspectorTransform READ inspectorTransform NOTIFY inspectorTransformChanged)
	Q_PROPERTY(QVariantList history READ historyCached NOTIFY historyChanged)
	Q_PROPERTY(int historyIndex READ historyIndex NOTIFY docChanged)
	Q_PROPERTY(QString editSceneId READ editSceneId NOTIFY docChanged)
	Q_PROPERTY(QString editSceneName READ editSceneName NOTIFY docChanged)
	// Twitch's vertical stream: each scene's vertical layout, edited with a Horizontal/Vertical switch.
	Q_PROPERTY(bool verticalOn READ verticalOn NOTIFY docChanged)
	Q_PROPERTY(bool editingVertical READ editingVertical NOTIFY docChanged)
	Q_PROPERTY(bool editingProgram READ editingProgram NOTIFY docChanged) // the scene on air (either layout)
	Q_PROPERTY(QString programSceneId READ programSceneId NOTIFY docChanged)
	Q_PROPERTY(bool canUndo READ canUndo NOTIFY docChanged)
	Q_PROPERTY(bool canRedo READ canRedo NOTIFY docChanged)
	Q_PROPERTY(QString undoLabel READ undoLabel NOTIFY docChanged)
	Q_PROPERTY(QString stats READ stats NOTIFY engineChanged)
	Q_PROPERTY(QString status READ status NOTIFY engineChanged)
	Q_PROPERTY(bool recording READ recording NOTIFY engineChanged)
	Q_PROPERTY(bool recordingPaused READ recordingPaused NOTIFY engineChanged)
	Q_PROPERTY(bool canPauseRecording READ canPauseRecording NOTIFY engineChanged)
	Q_PROPERTY(QString toast READ toast NOTIFY toastChanged)
	Q_PROPERTY(QStringList kinds READ kinds CONSTANT)
	Q_PROPERTY(int thumbTick READ thumbTick NOTIFY thumbTickChanged)
	Q_PROPERTY(bool streaming READ streaming NOTIFY engineChanged)
	Q_PROPERTY(bool testStreaming READ testStreaming NOTIFY engineChanged)
	Q_PROPERTY(bool preparingStream READ preparingStream NOTIFY engineChanged)
	Q_PROPERTY(bool live READ live NOTIFY engineChanged)
	// An untitled show with changes since New/Sample/Import: Open and Import ask to save it first.
	Q_PROPERTY(bool unsavedShow READ unsavedShow NOTIFY docChanged)
	Q_PROPERTY(bool liveLocked READ liveLocked NOTIFY engineChanged)
	Q_PROPERTY(QVariantMap streamStats READ streamStats NOTIFY streamStatsChanged)
	Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
	Q_PROPERTY(QVariantList encoders READ encoders CONSTANT)
	Q_PROPERTY(QStringList services READ services CONSTANT)
	Q_PROPERTY(QVariantList mixer READ mixerCached NOTIFY mixerChanged)
	Q_PROPERTY(QVariantMap levels READ levels NOTIFY levelsChanged)

public:
	App(Engine *engine, const QString &docPath, QObject *parent = nullptr);

	QVariantList scenes() const;
	QVariantList pool() const;
	QVariantList layers() const;
	QVariantMap inspector() const;
	QVariantList history() const;
	int historyIndex() const { return m_store.historyIndex(); }
	QString editSceneId() const { return m_editScene; }
	QString editSceneName() const;
	bool verticalOn() const { return doc().verticalOn; }
	bool editingProgram() const { return doc().mainScene(m_editScene) == programSceneId(); }
	bool editingVertical() const { return !doc().scenes.value(m_editScene).verticalOf.isEmpty(); }
	Q_INVOKABLE void setVerticalOn(bool on);
	Q_INVOKABLE void setEditingVertical(bool vertical);
	QSize editCanvas() const { return doc().canvasOf(m_editScene); }
	QString programSceneId() const { return m_engine->programScene(); }
	bool canUndo() const { return m_store.canUndo(); }
	bool canRedo() const { return m_store.canRedo(); }
	QString undoLabel() const { return m_store.undoLabel(); }
	QString stats() const { return m_engine->stats(); }
	QString status() const { return m_engine->status(); }
	bool recording() const { return m_engine->recording(); }
	bool recordingPaused() const { return m_engine->recordingPaused(); }
	bool canPauseRecording() const { return m_engine->canPauseRecording(); }
	QString toast() const { return m_toast; }
	QStringList kinds() const { return lobs::allKinds(); }
	const lobs::Doc &doc() const { return m_store.doc(); }

	// scenes
	Q_INVOKABLE void editScene(const QString &id);
	Q_INVOKABLE void take();
	Q_INVOKABLE void addScene();
	Q_INVOKABLE void renameScene(const QString &id, const QString &name);
	Q_INVOKABLE void deleteScene(const QString &id);
	Q_INVOKABLE void duplicateScene(const QString &id, bool linked);
	// pool
	// more: the Add dialog's other choices (a browser page's options; mono for audio).
	Q_INVOKABLE void addSource(const QString &kind, const QString &name, const QString &extra, bool place, const QVariantMap &more = {});
	Q_INVOKABLE void placeSource(const QString &id);
	Q_INVOKABLE void renameSource(const QString &id, const QString &name);
	// Audio Mixer arrangement (its own order, saved in the show; not live-locked).
	Q_INVOKABLE void moveMixer(const QString &id, int toIndex);
	Q_INVOKABLE void setMixerPinned(const QString &id, bool pinned);
	// Media playback (a live control: allowed on air, like show/hide). See Engine::mediaInfo.
	Q_INVOKABLE QVariantMap mediaInfo(const QString &sourceId) const { return m_engine->mediaInfo(sourceId); }
	Q_INVOKABLE void mediaCommand(const QString &sourceId, const QString &cmd, double ms = 0) { m_engine->mediaCommand(sourceId, cmd, qint64(ms)); }
	Q_INVOKABLE void deleteSource(const QString &id);
	Q_INVOKABLE void selectSource(const QString &id);
	// Devices or windows a source can use. Asking Windows takes 100-500 ms, so the list is kept
	// for a few seconds; fresh = true (opening the menu) always asks again.
	Q_INVOKABLE QVariantList devices(const QString &sourceId, bool fresh = false);
	// The source already capturing this device, if any: a camera opens in one place at a time, so
	// the Add dialog shows that source's feed instead of opening the camera a second time.
	Q_INVOKABLE QString sourceUsingDevice(const QString &kind, const QString &device) const;
	Q_INVOKABLE QVariantList devicesForKind(const QString &kind); // before the source exists
	Q_INVOKABLE void setSourceSetting(const QString &id, const QString &key, const QVariant &value);
	// Audio effects on a pool source (mixer).
	Q_INVOKABLE QVariantList audioFxTypes() const;
	Q_INVOKABLE bool kindAvailable(const QString &kind) const { return m_engine->kindAvailable(kind); }
	// Sources still without a device get this machine's first one, e.g. a display added before
	// macOS allowed Screen Recording (nothing to list then). Not an undo step.
	void fillMissingDevices();
	Q_INVOKABLE void addAudioFilter(const QString &sourceId, const QString &type);
	Q_INVOKABLE void removeAudioFilter(const QString &sourceId, int index);
	Q_INVOKABLE void moveAudioFilter(const QString &sourceId, int index, int delta);
	Q_INVOKABLE void setAudioFilterEnabled(const QString &sourceId, int index, bool on);
	Q_INVOKABLE void setAudioFilterValue(const QString &sourceId, int index, const QString &key, double value, bool final);
	Q_INVOKABLE void refreshBrowser(const QString &id) { m_engine->refreshBrowser(id); }
	// layers
	Q_INVOKABLE void selectLayer(const QString &id, bool additive);
	Q_INVOKABLE void selectLayerRange(const QString &id); // Shift-click in Layers
	Q_INVOKABLE void clearSelection(); // nothing selected: layers and Sources rows
	Q_INVOKABLE void toggleVisible(const QString &id);
	Q_INVOKABLE void toggleLock(const QString &id);
	Q_INVOKABLE void moveLayer(const QString &id, int delta);
	// Moves the selected layers up (-) or down (+) in their stack; a big delta = to the front/back.
	Q_INVOKABLE void moveSelected(int delta);
	// Drag and drop in Layers: mode "above"/"below" target, "into" a group (on top), or "group"
	// (make a new group of the two).
	Q_INVOKABLE void dropLayer(const QString &id, const QString &target, const QString &mode);
	Q_INVOKABLE void renameLayer(const QString &id, const QString &name);
	// Shows a scene inside the one being edited (a "scene" source, one per scene; never a scene
	// inside itself, directly or through others).
	Q_INVOKABLE void addSceneSource(const QString &sceneId);
	// Groups in the Layers panel open and close; remembered per group (not part of the show).
	Q_INVOKABLE void setGroupCollapsed(const QString &id, bool collapsed);
	Q_INVOKABLE void deleteSelected();
	Q_INVOKABLE void groupSelected();
	Q_INVOKABLE void ungroupSelected();
	Q_INVOKABLE void duplicateSelected();
	// Clipboard for layers (Ctrl+C / Ctrl+X / Ctrl+V; Ctrl+Shift+V pastes linked copies).
	Q_INVOKABLE void copyLayers();
	Q_INVOKABLE void cutLayers();
	Q_INVOKABLE void pasteLayers(bool linked = false);
	Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
	bool canPaste() const { return !m_clipIds.isEmpty(); }
	Q_INVOKABLE void copySelectedTo(const QString &sceneId, bool linked);
	// inspector
	Q_INVOKABLE void setTransform(const QString &field, double value);
	Q_INVOKABLE void setCrop(const QString &field, int value);
	// Scrubbing: previews while dragging, one undo step on release. keepAspect scales the other
	// side of W/H along with the one changed.
	Q_INVOKABLE void scrubTransform(const QString &field, double value, bool final, bool keepAspect = false);
	Q_INVOKABLE void scrubCrop(const QString &field, int value, bool final);
	// Crop to an aspect ratio (w/h; 0 = no crop), centered on the source.
	Q_INVOKABLE void cropToAspect(double ratio);
	// Rounded corners, % of the shorter side (50 = circle on a square).
	Q_INVOKABLE void setCornerRadius(double pct, bool final);
	// Crop mode: crop handles on the selected layer's edges.
	Q_PROPERTY(bool cropMode READ cropMode WRITE setCropMode NOTIFY docChanged)
	bool cropMode() const { return m_cropMode; }
	void setCropMode(bool on);
	// Esc in crop mode: puts the crop back as it was when crop mode started, and leaves it.
	Q_INVOKABLE void cancelCrop();
	// The aspect ratio crop handles keep for the selected layer (w/h on the canvas; 0 = free).
	// Picking one crops to it; Shift while dragging keeps whatever ratio the crop has.
	Q_PROPERTY(double cropAspect READ cropAspect NOTIFY docChanged)
	double cropAspect() const;
	Q_INVOKABLE void setCropAspect(double ratio);
	Q_INVOKABLE void setSize(const QString &field, double value, bool keepAspect); // w or h, optionally locked
	Q_INVOKABLE void setOpacity(double value, bool final);
	Q_INVOKABLE void setVisibleFlag(bool visible);
	Q_INVOKABLE void fit(const QString &mode);
	Q_INVOKABLE QVariantList filterTypes() const; // [{name, value}] for the Add filter menu
	// Third-party OBS plugins (Settings > Plugins; see plugins.h).
	Q_INVOKABLE QVariantList plugins() const;
	Q_INVOKABLE bool pluginsNeedRestart() const;
	Q_INVOKABLE QString pluginsFolder() const;
	Q_INVOKABLE void openPluginsFolder() const;
	Q_INVOKABLE QString addPlugin(const QString &pathOrUrl); // a folder or .zip; returns an error or empty
	Q_INVOKABLE QString removePlugin(const QString &name);
	Q_INVOKABLE void setPluginEnabled(const QString &name, bool on);
	Q_INVOKABLE QVariantList pluginSourceTypes() const; // [{kind, label, icon}] for Add Source
	// A plugin source's or filter's own settings, from obs_properties. scope: "source" (id = source),
	// "filter" (id = layer, index = filter), "audio" (id = source, index = audio filter).
	Q_INVOKABLE QVariantList obsProperties(const QString &scope, const QString &id, int index) const;
	Q_INVOKABLE void setObsProperty(const QString &scope, const QString &id, int index, const QString &key, const QVariant &value, bool final);
	Q_INVOKABLE void clickObsProperty(const QString &scope, const QString &id, int index, const QString &key);
	Q_INVOKABLE void addFilter(const QString &type);
	Q_INVOKABLE void removeFilter(int index);
	Q_INVOKABLE void toggleFilter(int index);
	Q_INVOKABLE void setFilterValue(int index, const QString &key, const QVariant &value, bool final);
	Q_INVOKABLE void unlinkGroup(const QString &group);
	Q_INVOKABLE void linkGroup(const QString &group, const QString &targetId, bool keepMine);
	Q_INVOKABLE void resetGroup(const QString &group);
	// history & files
	Q_INVOKABLE void undo();
	Q_INVOKABLE void redo();
	Q_INVOKABLE void jumpHistory(int index);
	Q_INVOKABLE void newDocument();
	// ---- Setup assistant ----
	// Measures the time to connect to each of a service's servers (fastest first): serverPings(list).
	Q_INVOKABLE void pingServers(const QString &service);
	// Streams a short test (Twitch: never goes live) with the saved key: bandwidthProgress / bandwidthDone.
	Q_INVOKABLE bool startBandwidthTest(const QString &server, int kbps);
	Q_INVOKABLE void stopBandwidthTest();
	// Settings that fit a measured upload (kbps) on a service: bitrate, resolution, fps, encoder.
	Q_INVOKABLE QVariantMap recommendSettings(int measuredKbps, const QString &service) const;
	Q_INVOKABLE QString streamKeyUrl(const QString &service) const; // where to copy your key
	// Projects: a folder holding <name>.lobs.json and its assets folder.
	Q_PROPERTY(QString projectFile READ projectFile NOTIFY projectChanged)
	QString projectFile() const { return m_projectFile; }
	Q_INVOKABLE bool saveProject(); // writes the current project file (false if it has none)
	Q_INVOKABLE QString defaultProjectsFolder() const;
	// Checks a new project's name and folder: "" if fine, else why not.
	Q_INVOKABLE QString checkNewProject(const QString &name, const QString &folder) const;
	Q_INVOKABLE bool createProject(const QString &name, const QString &folder);
	Q_INVOKABLE void loadSample();
	Q_INVOKABLE bool saveAs(const QUrl &url);
	// Shows opened or saved lately, newest first, those still on disk (File > Open Recent).
	Q_PROPERTY(QStringList recentShows READ recentShows NOTIFY recentShowsChanged)
	QStringList recentShows() const;
	Q_INVOKABLE void clearRecentShows();
	Q_INVOKABLE bool open(const QUrl &url);
	// OBS scene collections found in %APPDATA%/obs-studio/basic/scenes: [{name, path, modified}].
	Q_INVOKABLE QVariantList obsCollections() const;
	Q_INVOKABLE bool importObs(const QString &path);
	// Switching from OBS in one step (setup's "Coming from OBS?"): what OBS has here, then the
	// profile, scene collection and plugins at once. Returns {done: [...], notes: [...]}.
	Q_INVOKABLE QVariantMap obsSetup(const QString &folder = {}) const; // folder: QA points it at one
	Q_INVOKABLE QVariantMap importFromObs(const QString &profileDir, const QString &collectionPath, const QVariantList &plugins);
	Q_INVOKABLE void toggleRecording();
	Q_INVOKABLE void togglePauseRecording() { m_engine->setRecordingPaused(!m_engine->recordingPaused()); emit engineChanged(); }
	Q_INVOKABLE void nudge(int dx, int dy);
	// drag a source from Sources over the preview (coords relative to the preview item)
	Q_INVOKABLE void poolDragMove(const QString &sourceId, double x, double y);
	Q_INVOKABLE void poolDrop(const QString &sourceId, double x, double y);
	Q_INVOKABLE void poolDragCancel();
	Q_INVOKABLE void setGridVisible(bool on);
	// The native preview sits above QML, so it steps aside while a dialog is open.
	Q_INVOKABLE void setPreviewVisible(bool on) { m_dialogOpen = !on; updatePreviewHidden(); }
	// Menus call this on open/close: the preview steps aside while any popup could overlap it.
	// Popups that overlap the canvas call these; the canvas shows a frozen frame meanwhile.
	Q_INVOKABLE void popupOpened() { ++m_openPopups; updatePreviewHidden(); }
	Q_INVOKABLE void popupClosed() { m_openPopups = std::max(0, m_openPopups - 1); updatePreviewHidden(); }
	// Keyed version for things that can hide without a close signal (the shared tooltip).
	Q_INVOKABLE void setCovering(const QString &key, bool on) { on ? (void)m_coverers.insert(key) : (void)m_coverers.remove(key); updatePreviewHidden(); }
	// Safety net: nothing is actually open, so any leftover "covering" state is stale.
	Q_PROPERTY(int visibleMenus READ visibleMenus NOTIFY visibleMenusChanged)
	int visibleMenus() const { return m_visibleMenus; }
	Q_INVOKABLE void menuVisible(bool on) { m_visibleMenus = std::max(0, m_visibleMenus + (on ? 1 : -1)); emit visibleMenusChanged(); }
	// Dialogs ("dialog:" keys) keep theirs: they set and clear it with their own visibility.
	Q_INVOKABLE void resetCovering()
	{
		bool stale = m_openPopups > 0;
		for (auto it = m_coverers.begin(); it != m_coverers.end();)
			if (!it->startsWith("dialog:"))
				it = m_coverers.erase(it), stale = true;
			else
				++it;
		m_openPopups = 0;
		if (stale)
			updatePreviewHidden();
	}
	Q_PROPERTY(bool previewHidden READ previewHidden NOTIFY previewHiddenChanged)
	Q_PROPERTY(int freezeTick READ freezeTick NOTIFY previewHiddenChanged)
	bool previewHidden() const { return m_previewHidden; }
	int freezeTick() const { return m_freezeTick; }
	// Drawn in the UI's scene (PreviewItem): popups draw over it, so it never steps aside and
	// QML skips its overlap checks, tooltip dodging and frozen still.
	Q_PROPERTY(bool previewInScene READ previewInScene NOTIFY previewInSceneChanged)
	void setPreviewInScene(bool on)
	{
		if (on != m_previewInScene) {
			m_previewInScene = on;
			emit previewInSceneChanged();
		}
		updatePreviewHidden();
	}
	bool previewInScene() const { return m_previewInScene; }
	void updatePreviewHidden()
	{
		const bool hidden = !m_previewInScene && (m_dialogOpen || m_openPopups > 0 || !m_coverers.isEmpty());
		if (hidden != m_previewHidden) {
			m_previewHidden = hidden;
			if (hidden)
				++m_freezeTick; // fresh still for this freeze
			emit previewHiddenChanged();
		}
		emit previewVisibilityRequested(!hidden);
	}
	int thumbTick() const { return m_thumbTick; }
	Engine *engine() const { return m_engine; }

	// going live
	bool streaming() const { return m_engine->streaming(); }
	bool live() const { return m_engine->live(); }
	bool unsavedShow() const { return m_projectFile.isEmpty() && !(doc() == m_cleanDoc); }
	bool liveLocked() const { return m_engine->live() && m_engine->settings().liveLock && !m_liveUnlock; }
	// LOBS-006: the one check for edits that move, resize, crop, reorder or remove layers in the
	// scene viewers see. Refuses (with a toast) while live lock is on, unless Alt is held.
	bool liveEditAllowed(const QStringList &ids);
	// The same check for adding layers to a scene, and for changing a source that is on air.
	bool liveSceneEditAllowed(const QString &sceneId);
	bool liveSourceEditAllowed(const QString &sourceId);
	void settleObsLists(const QString &scope, const QString &id, int index); // see plugins::settleLists
	QVariantMap streamStats() const { return m_engine->streamStats(); }
	QVariantMap settings() const;
	QVariantList encoders() const { return m_engine->encoders(); }
	QStringList services() const { return m_engine->services(); }
	Q_INVOKABLE QVariantList servers(const QString &service) const { return m_engine->servers(service); }
	Q_INVOKABLE bool saveSettings(const QVariantMap &changes);
	// Profiles: named sets of stream, video and recording settings (as OBS's), e.g. Twitch,
	// YouTube, recording only. Switching isn't allowed while live. Each returns an error or "".
	Q_INVOKABLE QStringList profiles() const;
	Q_INVOKABLE QString switchProfile(const QString &name);
	Q_INVOKABLE QString newProfile(const QString &name); // a copy of the current one, switched to
	Q_INVOKABLE QString renameProfile(const QString &from, const QString &to);
	Q_INVOKABLE QString deleteProfile(const QString &name);
	Q_INVOKABLE QVariant sourceSetting(const QString &id, const QString &key) const { return doc().sources.value(id).settings.value(key); }
	Q_PROPERTY(QString dataFolder READ dataFolder CONSTANT)
	// Asset Library: the project's assets folder (next to the saved show, or LOBS's data folder).
	Q_PROPERTY(QString assetsFolder READ assetsFolder NOTIFY assetsChanged)
	// Sources panel: per-source device line and problems, refreshed every second; sourceTick
	// moves the thumbnails along.
	Q_PROPERTY(QVariantMap sourceStatus READ sourceStatus NOTIFY sourceStatusChanged)
	Q_PROPERTY(int sourceTick READ sourceTick NOTIFY sourceStatusChanged)
	Q_PROPERTY(QVariantList assets READ assets NOTIFY assetsChanged)
	Q_PROPERTY(QString assetPath READ assetPath NOTIFY assetsChanged)
	Q_PROPERTY(QVariantList missingAssets READ missingAssets NOTIFY sourceStatusChanged)      // current folder, relative ("" = top)
	Q_PROPERTY(QVariantList assetCrumbs READ assetCrumbs NOTIFY assetsChanged) // [{name, path}] from the top
	QString dataFolder() const;
	QString assetsFolder() const;
	QVariantMap sourceStatus() const { return m_sourceStatus; }
	int sourceTick() const { return m_sourceTick; }
	Q_INVOKABLE void toggleInScene(const QString &sourceId); // add to / remove from the edited scene
	QVariantList assets() const;
	Q_INVOKABLE void importAssets(const QVariantList &urls); // copies into the current assets folder
	QString assetPath() const { return m_assetDir; }
	QVariantList assetCrumbs() const;
	Q_INVOKABLE void openAssetFolder(const QString &path);  // absolute path of a folder in assets
	Q_INVOKABLE void goToAssetPath(const QString &relative); // breadcrumb
	Q_INVOKABLE void assetUp();
	Q_INVOKABLE QString createAssetFolder();                 // "New folder" in the current folder; returns its path
	Q_INVOKABLE bool renameAsset(const QString &path, const QString &name);
	Q_INVOKABLE void copyAssets(const QStringList &paths);   // to the Windows clipboard (works with Explorer)
	Q_INVOKABLE void cutAssets(const QStringList &paths);
	Q_INVOKABLE void pasteAssets();                          // into the current folder
	Q_INVOKABLE bool clipboardHasFiles() const;
	Q_INVOKABLE void moveAssets(const QStringList &paths, const QString &destFolder);
	Q_INVOKABLE void trashAssets(const QStringList &paths);
	// Folder color: kept in a hidden ".lobsfolder" file inside the folder, so it survives moves.
	Q_INVOKABLE void setFolderColor(const QString &folder, const QString &color);
	static QString folderColor(const QString &folder);
	QVariantList missingAssets() const;
	mutable QVariantList m_missingCache;
	mutable lobs::Doc m_missingDoc;
	mutable QElapsedTimer m_missingAge;
	mutable bool m_missingValid = false;
	Q_INVOKABLE void findMissingAssets();
	Q_INVOKABLE void relinkAsset(const QString &sourceId, const QString &file);
	Q_INVOKABLE void revealAsset(const QString &path);       // Explorer, with the file selected
	Q_INVOKABLE void openAssetsFolder();
	Q_INVOKABLE void trashAsset(const QString &path);
	Q_INVOKABLE void assetDragMove(const QString &path, double x, double y);
	Q_INVOKABLE void assetDrop(const QString &path, double x, double y);
	Q_INVOKABLE void placeAsset(const QString &path);
	Q_PROPERTY(bool editSceneHasBackground READ editSceneHasBackground NOTIFY docChanged)
	bool editSceneHasBackground() const;
	Q_INVOKABLE void addBackground();
	// Stages a feature on the sample show for screenshots (LOBS_DEMO=<name>): editor, tools,
	// crop, layers. Dialogs (transitions, setup, settings) are opened by the QML.
	Q_INVOKABLE void runDemo(const QString &name);
	// Projectors: the program output or a scene, on a monitor (full screen) or in a window.
	Q_INVOKABLE QVariantList screens() const; // [{index, name}]
	Q_INVOKABLE void openProjector(const QString &sceneId, int screenIndex); // sceneId "" = program; screen -1 = window
	Q_INVOKABLE void closeProjectors(); // before the engine shuts down
	void requestCanvasMenu(QPointF windowPos) { emit canvasMenuRequested(windowPos.x(), windowPos.y()); }
	// Transitions (stored in the show): the list, the one Show uses, and editing them.
	Q_PROPERTY(QVariantList transitions READ transitionsCached NOTIFY transitionsChanged)
	Q_PROPERTY(QString currentTransition READ currentTransition NOTIFY docChanged)
	QVariantList transitions() const;
	QString currentTransition() const { const lobs::Transition *t = doc().transition(); return t ? t->id : QString(); }
	Q_INVOKABLE QString addTransition(const QString &type);
	Q_INVOKABLE void removeTransition(const QString &id);
	Q_INVOKABLE void setCurrentTransition(const QString &id);
	// field: "name", "durationMs", or a setting key (direction, color, luma_image, path...)
	Q_INVOKABLE void updateTransition(const QString &id, const QString &field, const QVariant &value);
	Q_INVOKABLE void previewTransition(const QString &id);
	Q_INVOKABLE void stopTransitionPreview();
	Q_INVOKABLE QStringList lumaWipes() const; // image names for Luma Wipe
	Q_PROPERTY(QStringList swatches READ swatches NOTIFY docChanged)
	QStringList swatches() const { return doc().swatches; }
	Q_INVOKABLE void setSwatches(const QStringList &list);
	// Screen eyedropper: screenColorPicked(hex) or screenColorCanceled() follows.
	Q_INVOKABLE void pickScreenColor() { m_eyedropper.start(); } // a black layer at the bottom of the scene being edited
	Q_INVOKABLE void addGraphic(const QString &kind); // "text" or "color", straight onto the scene
	Q_INVOKABLE bool layoutHas(const QString &file, const QString &name) const;
	Q_INVOKABLE bool firstTime(const QString &key);
	Q_INVOKABLE bool uiFlag(const QString &key, bool fallback) const;   // small remembered UI states
	Q_INVOKABLE QString uiValue(const QString &key, const QString &fallback) const;
	Q_INVOKABLE void setUiValue(const QString &key, const QString &value);
	Q_INVOKABLE void notify(const QString &msg) { showToast(msg); } // a toast from QML
	Q_INVOKABLE void copyText(const QString &text) const;
	// Plugins that use OBS's interface: their panels (one window) and Tools menu items.
	Q_INVOKABLE QVariantList pluginDocks() const; // [{id, title}]: each becomes a LOBS panel
	Q_INVOKABLE QStringList pluginTools() const;
	Q_INVOKABLE void triggerPluginTool(const QString &name) const;
	Q_INVOKABLE QVariantList searchAssets(const QString &query) const; // every folder; adds "where"
	Q_INVOKABLE void setUiFlag(const QString &key, bool on);
	QVariantMap sourceInfo(const lobs::Source &s) const;               // what the inspectors show for an input
	Q_INVOKABLE bool envFlag(const QString &name) const { return qEnvironmentVariableIsSet(name.toUtf8().constData()); }
	Q_INVOKABLE QString envValue(const QString &name) const { return qEnvironmentVariable(name.toUtf8().constData()); } // true once per key (one-time UI migrations)
	Q_INVOKABLE void toggleStreaming();
	Q_INVOKABLE void startTestStream(); // Twitch: reaches Inspector, never the channel
	bool preparingStream() const { return m_preparingStream; } // asking Twitch for its Enhanced Broadcasting plan
	bool testStreaming() const { return m_engine->testStreaming(); }
	// Graphic tools: select | text | rect | ellipse | arrow | pencil | laser. Drawing tools make a
	// layer in the edited scene; the laser draws a fading trail over the output, not a layer.
	Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged)
	Q_PROPERTY(QString toolFill READ toolFill WRITE setToolFill NOTIFY toolChanged)
	Q_PROPERTY(QString toolStroke READ toolStroke WRITE setToolStroke NOTIFY toolChanged)
	Q_PROPERTY(int toolStrokeWidth READ toolStrokeWidth WRITE setToolStrokeWidth NOTIFY toolChanged)
	QString tool() const { return m_tool; }
	void setTool(const QString &t);
	QString toolFill() const { return m_toolFill; }
	void setToolFill(const QString &c);
	QString toolStroke() const { return m_toolStroke; }
	void setToolStroke(const QString &c);
	int toolStrokeWidth() const { return m_toolStrokeWidth; }
	Q_PROPERTY(int laserTrailMs READ laserTrailMs WRITE setLaserTrailMs NOTIFY toolChanged)
	int laserTrailMs() const { return m_laserTrailMs; }
	void setLaserTrailMs(int ms);
	void setToolStrokeWidth(int w);
	QPointF fromCanvas(QPointF c) const; // canvas px -> preview point (self-tests)
	// Replay buffer, virtual camera, live stats and hotkeys.
	Q_PROPERTY(bool replayActive READ replayActive NOTIFY engineChanged)
	Q_PROPERTY(bool vcamActive READ vcamActive NOTIFY engineChanged)
	Q_PROPERTY(bool vcamAvailable READ vcamAvailable NOTIFY engineChanged)
	Q_PROPERTY(QVariantMap liveStats READ liveStats NOTIFY engineChanged)
	Q_PROPERTY(QVariantList hotkeyActions READ hotkeyActionsCached NOTIFY hotkeyActionsChanged)
	bool replayActive() const { return m_engine->replayActive(); }
	bool vcamActive() const { return m_engine->vcamActive(); }
	bool vcamAvailable() const { return m_engine->vcamAvailable(); }
	QVariantMap liveStats() const { return m_engine->liveStats(); }
	QVariantList hotkeyActions() const;
	Q_INVOKABLE void toggleReplay();
	Q_INVOKABLE void saveReplay();
	Q_INVOKABLE void toggleVirtualCam();
	Q_INVOKABLE void setHotkey(const QString &action, const QString &combo);
	Q_INVOKABLE QString keyName(int key, int modifiers) const { return Hotkeys::keyName(key, modifiers); }
	Q_INVOKABLE void showRecordings();
	void runHotkey(const QString &action, bool pressed);
	void rebuildHotkeys();
	Q_INVOKABLE void setLiveUnlock(bool on);
	// audio mixer
	QVariantList mixer() const;
	QVariantMap levels() const { return m_levels; }
	Q_INVOKABLE void setAudio(const QString &sourceId, const QString &field, const QVariant &value, bool final);

	// canvas input (from the preview window), in window-logical pixels
	void canvasPress(QPointF p, Qt::KeyboardModifiers mods, bool dbl);
	void canvasMove(QPointF p, Qt::KeyboardModifiers mods);
	void canvasRelease();
	void setPreviewSize(QSizeF s, double dpr = 1) { m_previewSize = s; m_previewDpr = dpr > 0 ? dpr : 1; emit previewScaleChanged(); }
	// Pixel-perfect preview (on by default): see previewScaleFor in engine.h.
	Q_PROPERTY(bool pixelPerfect READ pixelPerfect WRITE setPixelPerfect NOTIFY previewScaleChanged)
	Q_PROPERTY(int previewPercent READ previewPercent NOTIFY previewScaleChanged) // canvas pixels shown at, per screen pixel
	bool pixelPerfect() const { return m_pixelPerfect; }
	void setPixelPerfect(bool on);
	int previewPercent() const { return int(std::round(canvasScale() * m_previewDpr * 100)); }
	Qt::CursorShape cursorAt(QPointF p) const;
	// Typing into a text layer right on the canvas (after the text tool makes one).
	bool textEditing() const { return !m_textEditId.isEmpty(); }
	bool textKey(QKeyEvent *e); // true if the key went into the text
	void endTextEdit();
	Q_PROPERTY(QString toolFont READ toolFont WRITE setToolFont NOTIFY toolChanged)
	Q_PROPERTY(QString defaultTextFont READ defaultTextFont CONSTANT)
	Q_PROPERTY(bool toolBold READ toolBold WRITE setToolBold NOTIFY toolChanged)
	Q_PROPERTY(bool toolItalic READ toolItalic WRITE setToolItalic NOTIFY toolChanged)
	QString toolFont() const { return m_toolFont; }
	QString defaultTextFont() const { return lobs::defaultTextFont(); }
	bool toolBold() const { return m_toolBold; }
	bool toolItalic() const { return m_toolItalic; }
	void setToolFont(const QString &f) { m_toolFont = f; setUiValue("tools/font", f); emit toolChanged(); }
	void setToolBold(bool b) { m_toolBold = b; setUiValue("tools/bold", b ? "1" : "0"); emit toolChanged(); }
	void setToolItalic(bool b) { m_toolItalic = b; setUiValue("tools/italic", b ? "1" : "0"); emit toolChanged(); }
	// Height the canvas takes when shown in a w x h logical box (same scale rule as the preview).
	Q_INVOKABLE double previewVideoHeight(double w, double h, bool pixelPerfect) const
	{
		const double d = m_previewDpr;
		if (w <= 1 || h <= 1)
			return 0; // squeezed to nothing (never negative: that sized the native preview below zero)
		return previewScaleFor(w * d, h * d, doc().canvasW, doc().canvasH, pixelPerfect) / d * doc().canvasH;
	}

signals:
	void docChanged();
	void pluginsChanged();
	void recentShowsChanged();
	void clipboardChanged();
	// The big lists, each emitted only when its contents changed (so QML rebuilds only those).
	void scenesChanged();
	void poolChanged();
	void layersChanged();
	void inspectorChanged();
	void inspectorTransformChanged();
	void historyChanged();
	void mixerChanged();
	void transitionsChanged();
	void hotkeyActionsChanged();
	void engineChanged();
	void toastChanged();
	void assetsChanged();
	void previewScaleChanged();
	void projectChanged();
	void visibleMenusChanged();
	void canvasMenuRequested(double x, double y); // right-click on the preview (preview coords)
	void serverPings(const QVariantList &servers);
	void bandwidthProgress(const QVariantMap &p);
	void bandwidthDone(const QVariantMap &result);
	void screenColorPicked(const QString &hex);
	void screenColorCanceled();
	void toolChanged();
	void sourceStatusChanged();
	void thumbTickChanged();
	void previewVisibilityRequested(bool visible);
	void previewHiddenChanged();
	void previewInSceneChanged();
	void streamStatsChanged();
	void settingsChanged();
	void levelsChanged();

private:
	void onStoreChanged();
	void showToast(const QString &msg);
	void beginStream(bool test);
	bool m_preparingStream = false;
	class QNetworkAccessManager *m_net = nullptr;
	void updateOverlay();
	void autosave();
	QPointF toCanvas(QPointF p) const;
	double canvasScale() const;
	QString hitTest(QPointF canvasPt) const;
	bool isLockedLayer(const QString &id) const;
	QStringList layersIn(const QRectF &r) const;
	QString topAncestor(const QString &id) const;
	int hitHandle(QPointF canvasPt) const; // 0..3 corner, -1 none
	QString singleSelection() const;

	Engine *m_engine;
	lobs::Store m_store;
	QString m_docPath;
	Hotkeys *m_hotkeys = nullptr;
	QString m_projectFile; // the saved show this session belongs to ("" = unsaved)
	void rememberProject(); // saves it as the current show and puts it first in recentShows
	lobs::Doc m_cleanDoc;
	QStringList m_clipIds; // copied or cut layers (top-level ones of the selection)
	lobs::Doc m_clipDoc;   // the show when they were copied, so a cut layer can still be pasted // an untitled show matching this has nothing to lose (see unsavedShow)
	QFileSystemWatcher m_assetWatch;
	QVariantMap m_sourceStatus;
	int m_sourceTick = 0;
	QTimer m_statusTimer;
	QHash<QString, QVariantList> m_deviceCache; // kind -> devices, refreshed every few seconds
	// Layers placed before their source knew its size (window and screen capture start a moment
	// later), with when to give up: fitted to their real size once known, unless moved meanwhile.
	// Checked every 50 ms while any wait, so the stretched first frames barely show.
	QHash<QString, qint64> m_sizeWhenKnown;
	QTimer m_sizeTimer;
	void waitForSize(const QString &instanceId);
	void updateBlockedLayers(); // the preview's "Needs Screen Recording" placeholders
	void sizeNewLayers();
	void refreshSourceStatus();
	void watchAssets();
	void adoptExternalFiles(); // copy files the show uses into the assets folder
	QString copyIntoAssets(const QString &file, const QString &destDir = {});
	QString m_assetDir; // current Assets folder, relative to the assets root
	QString currentAssetDir() const;
	bool insideAssets(const QString &path) const;
	int filesInUse(const QString &path) const; // layers' files at or under path
	int usesOf(const QString &file) const;
	QHash<QString, QStringList> scenesByFile() const;
	QVariantMap assetEntry(const QFileInfo &fi, const QHash<QString, QStringList> &scenesByFile) const;
	void repointFiles(const QString &from, const QString &to);
	lobs::Transform assetTransform(const QString &path, QPointF canvasPoint) const;
	QString assetSource(lobs::Doc &d, const QString &path) const; // find or make the source for a file
	QString m_editScene;
	QStringList m_selection;
	QString m_selectAnchor; // the layer last clicked (without Shift): one end of a Shift-click range
	QSet<QString> m_collapsedGroups;
	bool hasSelectedDescendant(const QString &groupId) const;
	bool nestsSceneInItself(const QString &sourceId, const QString &sceneId); // and says so
	QString m_poolSelection;
	QString m_toast;
	QTimer m_toastTimer, m_saveTimer;
	QElapsedTimer m_unsavedSince; // first edit since the last autosave
	// Imported layers whose size waits on the engine learning each source's natural size.
	QVector<lobs::ObsPlacement> m_pendingPlacements;
	QTimer m_placeTimer;
	int m_placeTries = 0;
	void resolvePlacements(bool last);
	QSizeF m_previewSize;
	double m_previewDpr = 1;
	bool m_pixelPerfect = true;
	QString m_tool = "select", m_toolFill, m_toolStroke;
	int m_toolStrokeWidth = 6;
	int m_laserTrailMs = 500;
	Eyedropper m_eyedropper;
	void forgetProject();
	struct DeviceList { QVariantList items; qint64 at = 0; };
	QHash<QString, DeviceList> m_sourceDevices; // by source id (the Inspector's device menu)
	QHash<QString, QVariant> m_modelCache; // last value of each list sent to QML
	QTimer m_inspectorTimer; // drags: Inspector updates once per frame
	void refreshModels();
	void pushInspector(bool dragging);
	QString m_hotkeySig; // scenes and sources the global hotkeys were built for
	bool m_autosaveFailed = false; // toast once per failing streak
	bool m_autosaveBusy = false, m_autosaveAgain = false; // a background autosave is writing
	int m_visibleMenus = 0;
	QList<QPointer<QWindow>> m_projectors;
	QString m_toolFont = lobs::defaultTextFont();
	bool m_toolBold = true, m_toolItalic = false;
	QString m_textEditId; // layer being typed into
	QTimer m_caretTimer;
	bool m_caretOn = false;
	void layoutEditedText(); // box to fit the text, caret at its end
	void drawTextGhost(QPointF at, double size); // the "A" while dragging out a size
	QTimer m_thumbTimer;
	int m_thumbTick = 0;
	QVector<OverlayBox> m_dropBoxes;
	bool m_liveUnlock = false;
	QString m_liveKey, m_liveLabel;
	void liveEdit(const QString &key, const QString &label, const std::function<void(lobs::Doc &)> &fn, bool final);
	bool m_dialogOpen = false;
	int m_openPopups = 0;
	QSet<QString> m_coverers;
	bool m_previewHidden = false;
	int m_freezeTick = 0;
	QTimer m_levelTimer;
	QVariantMap m_levels;

	// drag state
	enum class Drag { None, Move, Resize, Create, Laser, Crop, CropPan, Marquee } m_drag = Drag::None;
	QRectF m_marquee;           // canvas px, while dragging out a selection rectangle
	QStringList m_marqueeBase;  // the selection it adds to (Shift), or empty
	bool m_cropMode = false;
	static bool pageFollowsLayer(const lobs::Doc &d, const QString &id);
	static void sizePage(lobs::Doc &d, const QString &id, const lobs::Transform &t);
	bool m_previewInScene = false;
	lobs::Doc m_cropEntry; // the show when crop mode started (Esc goes back to it)
	QHash<QString, double> m_cropAspects; // per layer, this session; else read from its crop
	int m_perfMoves = 0;
	qint64 m_perfTotalUs = 0, m_perfMaxUs = 0;
	qint64 m_perfApplyUs = 0, m_perfEditUs = 0, m_perfHotkeyUs = 0, m_perfQmlUs = 0, m_perfInspectorUs = 0; // drag timing, logged on release
	qint64 m_perfCommitUs = 0, m_perfChanges = 0;
	bool m_modelsStale = false; // a drag skipped some list refreshes; refreshModels() catches up after
public:
	// Time spent per phase of applying changes since the last call (ms), then reset. For profiling.
	// QML reads these lists many times per change. Each read returns the copy refreshModels()
	// last pushed instead of rebuilding it (LOBS-025).
	QVariantList scenesCached() const { const QVariant v = m_modelCache.value(QStringLiteral("scenes")); return v.isValid() ? v.toList() : scenes(); }
	QVariantList poolCached() const { const QVariant v = m_modelCache.value(QStringLiteral("pool")); return v.isValid() ? v.toList() : pool(); }
	QVariantList layersCached() const { const QVariant v = m_modelCache.value(QStringLiteral("layers")); return v.isValid() ? v.toList() : layers(); }
	QVariantMap inspectorTransform() const { return m_modelCache.value(QStringLiteral("inspectorTransform")).toMap(); }
	QVariantMap inspectorCached() const { const QVariant v = m_modelCache.value(QStringLiteral("inspector")); return v.isValid() ? v.toMap() : inspector(); }
	QVariantList historyCached() const { const QVariant v = m_modelCache.value(QStringLiteral("history")); return v.isValid() ? v.toList() : history(); }
	QVariantList mixerCached() const { const QVariant v = m_modelCache.value(QStringLiteral("mixer")); return v.isValid() ? v.toList() : mixer(); }
	QVariantList transitionsCached() const { const QVariant v = m_modelCache.value(QStringLiteral("transitions")); return v.isValid() ? v.toList() : transitions(); }
	QVariantList hotkeyActionsCached() const { const QVariant v = m_modelCache.value(QStringLiteral("hotkeyActions")); return v.isValid() ? v.toList() : hotkeyActions(); }
	Q_INVOKABLE QVariantMap takePerfCounters();
private:
	QVector<QPointF> m_createPts; // canvas px while drawing with a graphic tool
	void finishCreate();
	QPointF m_dragStart;
	int m_handle = -1;
	QStringList m_dragIds;
	lobs::Doc m_dragBase;
	QVector<OverlayGuide> m_guides;
};
