# LOBS — Plan

**A standalone, native streaming and recording app: OBS's engine (libobs) with a new UI and source model. You install LOBS instead of OBS.**

## 1. The problem

OBS is a great engine wrapped in a frustrating authoring model:

| Pain | Why it happens in OBS |
|---|---|
| Setting up scenes is tedious and fragile | Each scene is a flat list of scene items; everything is manual, per-scene |
| No real grouping | "Groups" exist but are clunky, can't be shared across scenes, and break with transforms |
| Copying sources between scenes is annoying | Copy/paste vs. "Add Existing" vs. "Duplicate" behave differently and aren't obvious |
| Need different filters or sizing per scene → add the source again | Filters live on the **source**, not the **scene item**, so one camera with two looks means two sources (and sometimes two device captures fighting over one webcam) |
| Hard to tell what's shared vs. unique | No visual sign of which sources are the same object |

## 2. Core concept: Sources → Scene Instances

The idea comes from the project bins in DaVinci Resolve and Adobe Premiere. In LOBS the panel is called **Sources**.

```
 Sources (global, added once)             Scenes (canvases)
 ┌────────────────────────┐        ┌──────────────────────────┐
 │ 🎥 Facecam (Brio)      │──drag─▶│ Scene "Just Chatting"     │
 │ 🖥 Display 1            │        │   Facecam  (instance A)   │
 │ 🎮 Game Capture        │        │   Display 1 (instance)    │
 │ 🎙 Mic (Shure)          │        ├──────────────────────────┤
 │ 🖼 Overlay.png          │──drag─▶│ Scene "Gameplay"          │
 └────────────────────────┘        │   Game Capture            │
                                   │   Facecam  (instance B)   │
                                   └──────────────────────────┘
```

- **Source** = a single input (device, window, file, browser, etc.). Added **once** to Sources. Owns only *input* settings (device, resolution, URL). One device = one capture, always.
- **Instance** = a source placed on a scene. Owns its **transform, crop, filters, blend mode, opacity, visibility, audio mix**. Changes are **local to that scene by default**.
- Drag from the pool onto the canvas (or into the layer list) to make an instance.

### Copying between scenes: Linked vs. Independent

When you copy or paste an instance (or a group) into another scene, you pick:

- **Linked** — shares one property set. Edit it in one scene, it changes everywhere. Shown with a 🔗 badge and a shared color tag.
- **Independent** — a snapshot copy. Diverges freely.
- **Per-property linking (advanced):** link the *filters* but keep the *transform* separate (e.g. same color grade on the facecam everywhere, different size per scene). Shown in the inspector with a lock/link toggle next to each property group: Transform · Crop · Filters · Audio · Visibility.
- **Unlink / Relink** anytime. Relink asks which side wins.
- "Select all instances of this source" and "Show linked instances" from the pool or the layer list.

### Implementation on libobs

- LOBS links libobs directly and ships the standard OBS modules (capture, audio, filters, encoders, outputs). OBS itself is never installed or run.
- The pool's **Source** maps to one `obs_source_t` input.
- An **Instance** maps to a built-in `lobs_instance` source (renders the shared input, cropped) placed as an `obs_sceneitem_t`. Filters live on the instance, so each layer has its own filter chain while the device is captured once. Proven in Phase 0.
- LOBS keeps its own document model (`.lobs.json`) as the source of truth and applies it to libobs. A one-way **import from OBS scene collections** helps people switch.

## 3. MVP feature set

1. Sources panel with bins/folders, search, a thumbnail preview, an "in use by N scenes" count, and an unused-source warning.
2. Drag-to-place onto the canvas; layer list with real nesting.
3. Per-instance transform, crop, filters, opacity, blend mode.
4. Copy/paste across scenes with a Linked / Independent prompt (with a remembered default).
5. **Real groups**: nestable, transformable as a unit, and copyable/linkable like any instance.
6. Inspector panel (property groups with link toggles).
7. Studio mode (Preview / Program) and transitions.
8. Streaming and recording through libobs outputs, with LOBS's own settings screens: stream service and key, encoder, resolution and FPS, recording path and format, audio devices.
13. **Live preview** rendered by libobs directly in the LOBS window.
9. Import of OBS scene collections.
10. **Undo history + live lock:** command-based undo/redo with a history panel; layers locked while live (moving one needs a modifier key or an unlock).
11. **Scene grid view:** thumbnail/button grid with folders, search and favorites, alongside the list view.
12. **Audio signal-flow view:** each audio source → filters → bus → output, with an echo warning, a pre/post monitor tap point and OS volume shown.

## 4. Proposal — taking scene comp to the next level

Ideas from Adobe (Photoshop / After Effects / Premiere), Affinity (Designer / Photo), and DaVinci Resolve. Grouped by value. Every item is optional and beyond the MVP.

### A. Layout & precision (Affinity / Photoshop / Figma)
- **Smart guides & snapping** to the canvas, other layers, the center, and safe areas, with live distance readouts.
- **Align & distribute** toolbar; **constraints / anchors** so layers stay pinned to edges when the canvas resolution changes (1080p ↔ 1440p ↔ vertical).
- **Artboards / multi-canvas:** one scene → landscape *and* 9:16 vertical outputs with independent layouts of the same instances (huge for simulcasting to TikTok/Shorts).
- **Numeric transforms** with math in fields (`1920/2`), plus a selectable pivot point.
- **Rulers, custom guides, title/action-safe overlays.**

### B. Non-destructive effects (Photoshop adjustment layers / Affinity live filters)
- **Adjustment layers:** a color grade / blur / LUT layer that affects everything beneath it in the stack, instead of filtering each source.
- **Masks:** shape, feather, gradient, and **clipping masks** (clip the webcam into a rounded frame layer). Optional AI person-segmentation mask.
- **Blend modes** as a first-class property (Multiply, Screen, Overlay, Add…).
- **Layer styles:** drop shadow, stroke, glow, corner radius, without separate image sources.
- **Effect presets** — save a filter stack and drop it on any instance.

### C. Color (DaVinci Resolve)
- **Node-based color page** per instance: a primary wheel → a secondary qualifier → a LUT, with a before/after split.
- **Scopes:** waveform, vectorscope, histogram on the program feed.
- **Shot match:** match the webcam's color to a reference instance.
- **Shared grades** via the linking system (link only the *Color* property group).

### D. Reuse & systems (Affinity Symbols / Figma Components / AE precomps)
- **Symbols / Components:** build a "Facecam frame" (camera + border + name tag), reuse it in every scene; edits to the master propagate, with **per-instance overrides** (e.g. different name text).
- **Nested scenes as precomps** with a proper "open/enter" editing view.
- **Templates & scene kits:** starting/BRB/ending sets, shareable as files.
- **Variables / data binding:** text layers bound to a global variable (`{guest_name}`, `{game}`), editable from one panel or the API.

### E. Motion (After Effects / Premiere)
- **Keyframed scene transitions ("Move" done right):** instances that exist in both scenes animate between their positions, with easing curves.
- **Mini timeline** per scene for intro animations (e.g. a lower-third slides in 0.5s after the scene goes live).
- **Motion presets:** fade/slide/scale-in/out per instance on show/hide.
- **Stingers and lower-thirds** authored inside the app, not exported from elsewhere.

### F. Workflow & UI (Resolve pages / Adobe workspaces)
- **Pages/workspaces:** *Build* (layout), *Color*, *Audio* (Fairlight-style mixer with buses and sends), *Live* (minimal stream-control UI). Fully dockable panels.
- **Real undo/redo history panel** (OBS's undo is limited).
- **Command palette** (Ctrl+K) for every action.
- **Layer color tags, lock, solo, and isolate.**
- **Multi-select editing** across instances (change opacity on 5 layers at once).
- **Versioned scene collections** with snapshot/restore and diffing.

### G. Live operation
- **Scene playlists / show rundowns** (like a broadcast rundown) with hotkeys and a Stream Deck integration.
- **Multiview** of all scenes, live thumbnails.
- **Per-output overrides:** different scene for the recording vs. the stream (e.g. no chat overlay on the recording).
- **Remote control API** (WebSocket compatible with obs-websocket where possible).

## 4b. Other user complaints worth designing for

Taken from OBS forum threads and feature-request posts (see Sources). These are anecdotal signals, not vote counts.

| Complaint | What users report | LOBS response |
|---|---|---|
| **Scene list doesn't scale** | Scrolling a long list to switch scenes is "a major pain"; people ask for a button grid | Scene grid/thumbnail view, folders, search, and favorites, alongside the list view; rundowns (§4G) |
| **Accidental moves, weak undo** | Top-listed request: Ctrl+Z for accidental drags/resizes in the preview | Full command-based undo history (already in the model); **lock** layers by default in Live mode; drag needs a modifier while live |
| **Dock layout is rigid** | Preview can't be undocked; docks below the preview are forced full-width; floating docks don't restore after restart; plugins (jrDockie, DockX) exist just to save layouts | Free docking grid, saved and hotkeyed workspaces (§4F), reliable restore of multi-monitor layouts |
| **Audio routing is opaque** | Global "Desktop/Mic" sources vs. per-scene audio sources confuse people; filters land on the wrong source; hidden ducker/limiter filters cause mystery cutouts; monitor-and-output causes echo; Windows volume mixer silently lowers OBS | Audio sources live in Sources like video; a **signal-flow view** showing every source → filters → bus → output; a warning when monitor and output would echo; show OS-level volume in the mixer |
| **Mixer clutter** | Can't easily hide unused device audio; meters look mono; sort order changes between versions | Hide/pin channels, true stereo meters, user-defined fixed order, channel groups |
| **"Monitor doesn't match stream"** | Users hear unfiltered audio through monitoring while recording is filtered | Monitor tap-point selector (pre/post filters, pre/post fader), clearly labeled |
| **UI density & scaling** | Excess padding steals preview space; controls too small when shrunk; high-DPI/4K scaling bugs | Compact / comfortable density setting; per-monitor DPI-aware UI; preview gets priority space |
| **Modal dialogs & hangs** | Filter dialogs (e.g. chroma-key color pick) freeze for minutes; property windows block the main view | Non-modal inspector with live preview; heavy work off the UI thread; eyedropper picks directly from the canvas |
| **Discoverability of hidden state** | Filters, hidden sources, and transforms are invisible from the scene view | Badges on layers for filters, links, audio, and hidden/locked state; "what's affecting this?" panel |

**Added to the MVP (§3, items 10–12):** undo history + live lock, the scene grid view, and the audio signal-flow view. These fix the most common complaints with the least new tech.

**Sources:** [Feature requests thread](https://obsproject.com/forum/threads/a-whole-bunch-of-feature-requests.39133) · [General feature suggestions (undo)](https://obsproject.com/forum/threads/general-feature-suggestions.49680) · [Removable preview dock](https://obsproject.com/forum/threads/removable-preview-dock.113832/) · [Docks below preview](https://obsproject.com/forum/threads/docks-directly-below-obs-preview.165684/) · [Undocked docks not restoring](https://obsproject.com/forum/threads/undocked-docks-not-staying-in-place-on-startup.154775) · [jrDockie plugin](https://obsproject.com/forum/resources/jrdockie-save-and-load-window-and-dock-layouts.1955/history) · [Filtered audio monitoring](https://obsproject.com/forum/threads/monitor-of-filtered-audio-requesting-help.166498/latest) · [Mic cuts out (hidden filters)](https://obsproject.com/forum/threads/mic-audio-cuts-out-as-soon-as-desktop-audio-starts.171141/latest) · [Windows volume at 0](https://obsproject.com/forum/threads/no-desktop-audio-recording-while-using-headset.81078/latest) · [UI glitch](https://obsproject.com/forum/threads/obs-ui-glitch-unusable.137829/post-506483) · [UI scaling](https://obsproject.com/forum/tags/ui-scaling/)

## 5. Architecture sketch

```
┌──────────────── LOBS.exe (Qt 6 + QML) ───┐
│ Sources    │ Canvas │ Layers │ Inspector │   QML UI; canvas = native libobs display
└──────────────────┬───────────────────────┘
                   │ commands (undoable)
┌──────────────────▼───────────────────────┐
│ Document model: Sources, Instances,      │   JSON; link graph; symbols; history
│ Link groups, Symbols, Scenes, Outputs    │
└──────────────────┬───────────────────────┘
                   │ reconciler (diff → libobs calls)
┌──────────────────▼───────────────────────┐
│ libobs (bundled): sources, scenes,       │   engine + OBS modules shipped with LOBS,
│ filters, encoders, outputs, services     │   + built-in LOBS sources (instance, masks…)
└──────────────────────────────────────────┘
```

- **The document model is the truth;** libobs is a render target. This makes linking, symbols, undo, and versioning tractable.
- New effects (masks, adjustment layers, layer styles) ship as **libobs plugins/shaders**, so they also benefit from the engine's GPU pipeline.
- The document model, commands and reconciler are plain C++ with no UI dependency, so they can be unit-tested.

## 6. Phases

| Phase | Goal | Exit criteria |
|---|---|---|
| **0. Spike ✅** | Prove per-instance filters on one shared input in libobs | One webcam capture, two instances with different filters/crops, no extra device capture, acceptable GPU cost |
| **0b. Native spike ✅** | LOBS.exe embeds libobs with no OBS install; live preview in a Qt window; per-layer filtered copies of one camera; record a clip | Camera previews live, 2 filtered layers from one capture, a 10 s MP4 records |
| **1a. Editor core ✅ (mostly)** | C++ document model + undo (ported from the web prototype, same `.lobs.json` format); reconciler doc → libobs; QML scenes list, Sources, layers, inspector; canvas select/move/resize drawn by libobs; Take to program | Build a 3-scene show in LOBS.exe from a blank doc; every edit undoable; save and reopen |
| **1b. Editor depth (in progress)** | Add-source dialog with device pickers (camera, display, window, game, audio); filters panel; scene grid with live thumbnails; snapping and guides; history panel | No source needs settings outside LOBS; layout feels as quick as the prototype |
| **2. Linking & groups** | Copy to scenes as linked/independent; per-property link toggles; real groups; select all instances of a source | Round-trip linked edits across scenes; undo works across linked edits |
| **3. Go live (MVP, streaming done)** | Stream (RTMP services + key) and record settings screens; encoder picker (x264/NVENC/QSV); studio mode + transitions; live lock; audio mixer + signal-flow view; browser source (CEF) bundled; OBS collection import; libobs built from source; installer | Someone uninstalls OBS and does a real stream with LOBS |
| **4+. Proposal items** | Prioritize from §4 by user feedback | Suggested first picks: snapping/align (A), masks & blend modes (B), components (D), vertical canvas (A) |

### Native architecture notes

- **Preview and canvas editing:** libobs draws the edited scene into a native child window. Because native windows sit above QML, selection boxes and handles are drawn by libobs in the same draw callback, and mouse input on that window drives move/resize. Editing a scene doesn't change program until **Take**.
- **Engine objects are named by document ids** (`src_…`, `ins_…`), so renames in LOBS never touch libobs; display names live only in the document.
- **The reconciler is incremental:** it diffs the document against what it created and only touches changed sources, items and filters.

## 7. Decisions

1. **Standalone app, not an OBS companion.** LOBS embeds libobs and replaces OBS. (Revised 2026-10-19; the first prototype drove a running OBS over obs-websocket.)
2. **UI stack → native C++ with Qt 6 + QML.** No Electron. Qt is what OBS uses, so embedding the libobs preview is a solved problem, and QML gives a modern GPU-drawn UI. Qt is LGPL. Rust/Slint was considered but libobs bindings for Rust are immature.
3. **Licensing → GPL-2.0-or-later**, required since LOBS links and ships libobs. With KDDockWidgets (GPL-2.0-only or GPL-3.0) linked in, a distributed build is effectively GPL-2.0 or GPL-3.0.
4. **Per-instance filters → built-in `lobs_instance` source** that renders a shared input, cropped. Each layer is one instance with its own filters.
5. **Engine builds → bundle OBS's release binaries for now, build libobs from source before shipping.** The spike copies libobs and modules from an OBS 32.2.2 install so the exe runs on its own; CI will build libobs from the obs-studio tag later.
6. **OBS compatibility → one-way import** of OBS scene collections (Phase 3).

### Going live (Phase 3, first part) — 2026-10-19

- **Streaming:** service picker from OBS's bundled rtmp-services list (Twitch, YouTube, 80+ more) with per-service servers, or a custom RTMP/RTMPS/SRT URL. The service's own limits are applied to the encoder settings. Auto-reconnect (20 tries, 2 s apart).
- **Stream key** stored encrypted with Windows DPAPI in `%APPDATA%/LOBS/LOBS/settings.json`, so only this Windows user can decrypt it. The UI never reads it back, only whether one is saved.
- **Encoders:** NVENC, AMD AMF, Quick Sync and x264, offered only when the GPU module registers them; "Automatic" picks the best (NVENC on this PC). Quality/Balanced/Speed presets are mapped per encoder. 2 s keyframes.
- **Video:** output 1080p/900p/720p/540p scaled from the 1920×1080 canvas, 30 or 60 fps; applied with a video reset when nothing is live.
- **Recording:** folder, MP4/MKV/MOV, and quality "same as stream" (shares the stream encoder), "high" or "indistinguishable".
- **Header while live:** pulsing dot, elapsed time, a health badge (kbps; amber/red on dropped frames or congestion, "Reconnecting"), dropped-frame count, End stream.
- **Live lock:** while streaming or recording, a red banner says layers are locked; moving or resizing needs Alt, nudging needs Unlock.
- **Audio tab:** per-source stereo peak meters (libobs volmeters, 20 Hz), a dB fader (-60 to 0 dB), mute, monitoring mode, the six tracks, and warnings (echo risk, monitor only, not in stream).
- **Verified:** `LOBS.exe --streamtest rtmp://127.0.0.1/live` against a local RTMP server (node-media-server): it received H.264 1920×1080 at 60 fps with AAC audio from NVENC at ~5.7 Mbps for 11 s, 0 dropped frames, while recording to MP4 from the same encoder (8.8 MB). Meters respond to a test tone (Desktop Audio around -10 dB). Live lock blocks drags while recording. **Real platform verified (2026-10-19):** you streamed for about a minute with your own key, with no dropped frames.
- **Resolution-independent text** (an OBS gripe fixed): each text layer redraws the text at the pixel height it occupies on the canvas, so resizing a text layer re-renders it sharply instead of scaling a bitmap whose resolution is set by font size. Content, color and font stay on the Sources source; there's no separate size setting. Also fixed: layer edges were blended as straight alpha, which gave text and other soft edges a harsh fringe.
- **Browser source (done 2026-10-20):** OBS's obs-browser 2.26.9 plugin and CEF 127 are copied into `runtime/obs-plugins/64bit`. The plugin is built against Qt 6.11, so LOBS moved from Qt 6.8.3 to **Qt 6.11.1**. aqtinstall 3.3 can't read Qt 6.11's per-compiler repository layout, so the archives were fetched from download.qt.io directly, checked against their SHA-1 files, and unpacked to `C:\Qt\6.11.1\msvc2022_64`. KDDockWidgets was rebuilt against 6.11.1. LOBS ships its own Qt6Widgets.dll, which obs-browser links. Sources have a URL, an optional page size (defaults to the canvas), and a Reload button; page audio is routed into the mixer. Verified with `LOBS.exe --browsertest <url> <png>`, which rendered CSS, JavaScript and a transparent background.
- **OBS import (done 2026-10-20):** the top bar's Import menu lists the collections in `%APPDATA%/obs-studio/basic/scenes`, and Open also accepts an OBS `.json`. The OBS file is only read, never written, and the import becomes a new LOBS document (one undo step).
  - **What comes over:** scenes in OBS order, and each input once in Sources (camera, display, window, game, image, media, browser, color, text, mic, desktop audio) with its device, volume, mute, monitoring and tracks.
  - **Layers:** OBS position, scale, alignment, bounding boxes, crop (plus Crop/Pad filters), rotation, visibility and lock. OBS stores scale factors rather than pixels, so layer sizes are resolved once each source reports its natural size; until then a capture uses the canvas size and other kinds their typical size.
  - **Filters:** every video filter in OBS's obs-filters module except Crop/Pad (merged into the crop), Video Delay (Async) and HDR Tone Mapping. A source's filters are linked across all of its layers, matching OBS's per-source behavior.
  - **Fallbacks:** monitors and cameras that aren't connected fall back to this PC's default device.
  - **Skipped:** the old `[LOBS]` sync helpers. Groups are flattened into separate layers; nested scenes, audio filters, macOS-only sources and mirroring don't come over yet. One summary line lists what didn't come over.
  - **Verified:** `LOBS.exe --importtest <obs.json> <dir>` on both of your collections (5 scenes and 15 sources, and 3 scenes and 6 sources). Positions match OBS, including a mirrored camera. Display capture rendered black in the tests, likely because the screen was locked or asleep; still to confirm by eye.
- **Audio effects (done 2026-10-20):** each audio source has an Effects chain in the mixer: Noise Suppression, Noise Gate, Gain, 3-Band EQ, Compressor, Expander, Limiter and NVIDIA Audio Effects. You can add, bypass, reorder and remove effects, and dragging a slider is one undo step. The chain runs on the shared input, using libobs's own filters and OBS's setting keys. Value edits update filters in place, so audio doesn't drop out while you drag. OBS chains import as-is, and unknown OBS audio filters still run without controls. Verified on your collection: three chains of up to 7 effects reached libobs in order, with OBS's on/off states.
- **Standalone install and OBS engine updates (done 2026-10-20):**
  - **App and engine are separate:** the app is in `dist/LOBS` and the engine in `dist/LOBS/engine/<version>`, unpacked from OBS's official release zip and checked against GitHub's SHA-256. Only the modules in `engine-modules.txt` are kept; OBS's UI plugins and its Qt copy are left out. `obs.dll` is delay-loaded from the engine chosen at startup, and LOBS never patches libobs.
  - **Updater:** Settings > OBS engine (plus a top-bar notice) checks GitHub's public release list once a day, with an optional beta channel. It downloads the official `Windows-x64.zip`, verifies its SHA-256, installs it next to the current engine, and switches over on restart.
  - **Safety:** a new engine runs on trial. Two failed starts roll back automatically, and that version isn't offered again. "Go back to" is available manually, and new OBS major versions wait for a LOBS update.
  - **Installer:** `installer/build-installer.bat` builds `LOBS-Setup-0.2.0.exe` with Inno Setup 6.7.3: 160 MB, per-user, no admin, and runs Microsoft's VC++ runtime installer only if it's needed.
  - **Verified:** check (up to date), install as update, trial pass, automatic rollback after two failed starts, a silent install to a clean folder (recording and browser source work, and obs.dll loads from the install), and a clean uninstall.
  - **Remaining OBS-folder module:** only OBS's system-registered Virtual Camera driver, which Windows loads during camera listing on PCs that have OBS.
- **Automatic engine compatibility (done 2026-10-20):** any new OBS release, including a new major version, is tried automatically instead of being held back by version number. Before switching, LOBS:
  1. Checks that the new `obs.dll` exports every libobs function LOBS.exe imports, read from LOBS.exe's delay-import table and obs.dll's export table.
  2. Runs a hidden `--engine-smoketest`: the engine starts and at least 10 modules load; a scene with color, text and browser sources, a color filter and an audio effect is built; the frame's colors are right, including browser pixels; and a 2 s recording works.

  Only an engine that passes is activated, still on trial with automatic rollback. A failure is remembered, and LOBS explains why it stays on the current engine.
- **OBS 33 support:** the RC2 test found two real breaking changes, and LOBS handles both at runtime, so one build runs on 32 and 33.
  - **New layout:** releases moved to `core/<module>/`, so the engine installer is now layout-agnostic.
  - **New module loading:** `obs_load_all_modules()` became an empty stub, and `obs_load_core_modules()` requires every OBS module, including its UI ones. On 33, LOBS loads exactly its kept modules with `obs_load_plugins()`.

  Verified: 33.0.0-rc2 passes all smoke checks (browser 2.27.3), updates through the full path and starts. 32.2.2 still passes everything. Self-tests now use a throwaway document and a temp recording folder.
- **Mirroring and transitions (done 2026-10-20):**
  - **Mirroring:** layers have Flip horizontal and Flip vertical buttons in the Inspector's Transform section. They're part of the Transform group, so linking applies. `lobs_instance` mirrors inside the layer's own box, so position and size don't change, and OBS's mirrored items (negative scale) import as flipped.
  - **Transitions:** the program output goes through an OBS transition source: Cut, Fade, Slide, Swipe or Fade to black, with 150 ms to 1 s durations. The picker sits next to Show in the canvas header and is saved in settings (default Fade 300 ms). Changing the type rebuilds the transition without a cut.
  - **Smoke test:** now also checks mirroring (pixels swap sides) and transitions (mid-way at 0.3 s, lands on the new scene). It never saves settings.
- **OBS must-haves (done 2026-10-20):**
  - **Hotkeys:** global hotkeys through libobs's hotkey system, which work while a game has focus. Actions: stream, record, replay on/off, save replay, virtual camera, show each scene, mute each audio source, and push-to-talk per source (muted unless held). Settings > Hotkeys records combos, Backspace clears one, and one key does one thing.
  - **Status bar:** live and recording time, kbps, dropped frames, CPU, fps against target, render time, and render-lag or encoder-skip warnings above 1%, plus low disk space. Clicking it opens a Stats panel.
  - **Replay buffer:** OBS's `replay_buffer` output with its own encoders, 15 s to 2 min (Settings > Recording). Top-bar Replay and Save replay buttons, plus hotkeys.
  - **Virtual camera:** OBS's DirectShow driver. LOBS uses an existing registration; otherwise it copies the driver to a stable folder and registers it once with admin.
  - **Verified:** `--featuretest` (replay saves a file, an injected hotkey fires, push-to-talk, virtual camera start/stop, stats), with settings and documents untouched. The engine smoke test still passes.
- **Sources and Asset Library (done 2026-10-20):**
  - **Sources** lists live inputs only (cameras, capture, browser, audio), grouped under labeled sections, with "N layers", "Not placed" or "Always mixed in" on each row.
  - **Assets**, a new panel under Sources, shows the project's `assets` folder, which sits next to the saved show (LOBS's data folder for an unsaved one), as a grid with thumbnails or a list.
    - **Adding files:** drag them in from File Explorer, where files and folders are copied in (a re-drop isn't duplicated), or use + Asset.
    - **Using them:** drag one onto the canvas, or double-click to place it at the file's size, fitted to the canvas. Each file is one shared source.
    - **Right-click:** Place, Show in File Explorer, or Move to Recycle Bin (only when unused).
    - **Watched folder:** the panel live-updates from the folder, and files a show uses from elsewhere are copied in automatically.
  - **Text and color** are made from Layers (+ Text / + Color). Their sources, and file sources, are removed with their last layer.
  - **Verified:** `--assettest` (7 checks).
- **Sources panel redesign (done 2026-10-20):**
  - **Rows:** a live thumbnail for each video input (refreshed every second), the real device and format on the second line, and an amber status pill only on problems ("No signal", "Not connected", "Waiting for a game", "Window not found"), checked only for inputs that are running.
  - **"In scene" toggle** on each video row, which becomes Add / Remove for the edited scene on hover; "Used in …" lives in the "⋯" menu.
  - **Audio rows:** a mini stereo meter and a mute button.
  - **Layout:** an All / Video / Audio filter and "+ Source" in the header; video first, a divider, then audio; no section headers or arbitrary color dots. Device lists are cached for 30 s, since listing cameras can stall the UI.
- **Asset folders (done 2026-10-20):**
  - **Navigation:** subfolders in the Assets panel, with breadcrumbs and Up (Backspace), folders listed first, and double-click or Enter to open one.
  - **Folder actions:** New folder (Ctrl+N, then rename), rename (F2) and Move to Recycle Bin (Del). Folders or files still used by layers are kept.
  - **Copy, cut and paste:** Ctrl+C / X / V through the Windows clipboard, interoperating with File Explorer; a cut is marked the way Explorer marks it and pastes once.
  - **Selection and drops:** multi-select with Ctrl- and Shift-click, Ctrl+A, and drag a file onto a folder to move it. Drops and imports go into the current folder.
  - **Path safety:** renames and moves repoint the show *and* every undo/redo state (`Store::rewriteAll`), so history never points at a moved file.
  - **Verified:** `--foldertest` (10 checks).
- **Crisp preview (done 2026-10-20):**
  - **Pixel-perfect snapping:** the preview snaps to an exact ratio (2x, 1x, 1/2, 1/3, 1/4) of the canvas in *physical* pixels when that keeps at least 80% of the full-fit size; one rule (`previewScaleFor`) drives both drawing and click mapping. A header chip shows the % and toggles pixel-perfect or plain fit.
  - **High-quality fallback:** when no exact ratio fits, the canvas renders at full size and is downscaled with Lanczos (or area averaging below 50%), the filters OBS uses for its output. Edges measured about 9% sharper at 94%.
- **Assets redesign (done 2026-10-20):**
  - **Header:** one row with the path (Up built in; the current folder as plain text, no duplicate "Assets" crumb), search, "⋯" (new folder, paste, add files, show in Explorer) and "+ Add".
  - **Filter and sort strip:** All / Images / Video / Audio, sort by name, date or size (remembered), and the grid/list toggle.
  - **Tiles:** Windows-shell thumbnails for images *and videos*, a duration badge for video and audio, the extension for other files, a green "in a scene" dot matching Sources, two-line names cut at the end, and tooltips with size, pixel size, duration and "Used in …". List rows show a thumbnail and a detail line.
  - **Drop hint and missing files:** an always-visible drop hint at the end of the grid. A missing-file banner covers layers whose file was moved or deleted outside LOBS: **Find it** searches every assets folder by name and repoints the show and its history; **Choose file…** relinks to a file you pick.
- Still to do in Phase 3: studio mode (preview/program side by side), nested scenes, CI running the smoke test against every OBS release and RC, LOBS's own app updates (needs a release host), code signing for the installer, libobs from source, installer, multiple audio tracks in recordings.

### UI system: Fluid Functionalism, ported to QML (Phase 1b) — 2026-10-19

The UI follows [Fluid Functionalism](https://www.fluidfunctionalism.com) (React/shadcn). Its components are reimplemented in QML as `native/qml/F*.qml`, using its documented tokens:

- **Surfaces:** the dark 8-step ladder (`#171717` → `#484848`); window s1, side panels s2, cards s3, menus s4, dialogs s5. Text is `#f5f5f5` or muted `#a3a3a3`; borders 12% white; hover +6%, selected +10%; focus/accent `#6b97ff`.
- **Motion:** three springs, fast 80 ms (hover, focus, sliders), moderate 160 ms (tabs, switches, menus), slow 240 ms with slight overshoot (dialogs); exits one tier faster.
- **Fluid hover** (`FluidList`): one highlight that glides to the nearest row; no dead zones, no blinking. Also on the x axis in `FTabs`.
- **Sizes and type:** compact step (28 px controls, 12 px body, 14 px icons); Inter Variable 4.1 bundled (`native/fonts`, OFL); weights 400/550/700; no uppercase or letter-spacing.
- **Icons:** Lucide (ISC), bundled as SVGs in `native/icons` and drawn by a C++ image provider (`image://icon/<name>?c=<color>`) that recolors and renders each one at the exact pixel size, so icons stay sharp at any DPI. `FIcon` is the QML wrapper; `FButton` takes `iconName` (icon-only or icon + label). No emoji remain in the UI.
- **Dockable panels** (KDDockWidgets 2.4.1, KDAB, GPL-2.0-only or GPL-3.0; built from source into `native/deps/kddw`): Scenes, Sources, Layers, Inspector, History and Audio Mixer are panels around a fixed canvas. Drag a tab to move or float a panel, drop it on another panel to tab them together, reorder tabs, close panels; the **Window** menu shows/hides each one, shows all, or resets the layout. The layout autosaves every 3 s to `layout.json` (a save with no open panels falls back to the default). Tabs, groups, separators and floating windows are restyled in `native/qml/dock/`. Moving panels is direct and keeps the layout still: docked panels use LOBS's own drag (`qml/dock/PanelDragController.qml`). The panel stays in its slot (dimmed, "Moving…") until you drop, a chip follows the cursor, and a blue zone shows where it lands: half of the hovered panel near its edges, the whole panel near its middle or tab strip (to tab them together), or a full-side strip near the layout's outer edges. The move happens once, on release, through KDDockWidgets' addDockWidget/addDockWidgetAsTab. Floating windows keep KDDockWidgets' drag (35% ghost, restyled tile targets). Not yet: reordering tabs by dragging along the strip. Double-click a tab to float it. The drop targets and preview are restyled in `native/deps/kddw-patches/` (applied by `build-kddw.bat`, since KDDockWidgets loads those two files from fixed paths). The Audio Mixer is its own panel and reflows into columns when wide. Menus and dialogs now hide the native preview while open, so nothing is covered.
- **Settings** is a full page (Ctrl+,): sidebar with Stream, Recording, Video, Audio and About; grouped cards; every change saves immediately (the stream key has its own Save and Remove).
- Components: `FButton` (primary/secondary/tertiary/ghost/danger), `FInput`, `FNumField` (math), `FFillSlider` (the "Volume" fill slider), `FSwitch`, `FCheckbox`, `FTabs`, `FSelect`, `FMenu`/`FMenuItem`, `FDialog`, `FBadge`, `FSection`, `FPanelHeader`, `FluidList`. Tokens come from C++ as the `Pal` context property.

Also in 1b so far: drag a source from Sources onto the canvas (the engine draws the drop target; it lands where dropped), scene grid with live engine-rendered thumbnails, window picker for window capture, game capture modes (any fullscreen / specific window / hotkey). Fixed: layers created after a scene was showing didn't activate their input. The native preview hides while a dialog is open, since it sits above QML popups.

Still open in 1b: snapping guides polish, history panel styling details, Combobox/CommandMenu (Ctrl+K), menus that overlap the preview are still covered by it.

### Native editor (Phase 1a, `native/`) — 2026-10-19

- `native/src/model/` — C++ port of the document model (sources, instances, per-property links, groups, copy linked/independent) and the undo store; reads and writes the prototype's `.lobs.json` format.
- `native/src/engine.cpp` — incremental reconciler: inputs named by source id, one private scene per LOBS scene, one `lobs_instance` per layer with its own filters (opacity is a final color filter). The edited scene renders in the preview; **Take** sends it to program.
- `native/src/app.cpp` + `native/qml/` — scenes list, Sources (bins, usage counts), layers (reorder, lock, hide, group), inspector (transform with math, crop, filters with live sliders, visibility, link status + Unlink, device picker), history tab, add-source dialog, autosave to `%APPDATA%/LOBS/LOBS/current.lobs.json`.
- Canvas: click to select (groups first, double-click to go inside), drag to move with snapping to canvas and layer edges, corner handles to resize (aspect kept, Shift for free), arrow nudges. Boxes and handles are drawn by libobs over the preview.
- Verified by hand: live camera and display capture; move, resize and undo; editing a linked filter in one scene changes the other; the show survives a restart. `--autotest` builds a 2-scene show from a blank document through the editor commands and records 5 s (3.7 MB, 60 fps).
- Known rough edges: the inspector scrolls back on some edits; Basic-style checkboxes look off in the dark theme; 3-scene build-from-blank done via autotest commands, not yet by hand; no drag-and-drop from the Sources onto the canvas yet (double-click places).

### Native spike (Phase 0b, `native/`) — passed 2026-10-19

`LOBS.exe` (C++20, Qt 6.11.1 + QML) runs with OBS closed and without touching the OBS install: libobs and 14 modules (66 MB) are bundled under `native/runtime/`.

- Live preview drawn by libobs into a native child window inside the QML layout.
- One `dshow_input` capture of the EOS Webcam, shown as two `lobs_instance` layers: a wide shot with a warm grade and a tight 800×800 crop in black and white.
- Render: 60 fps, 0.40–0.68 ms per frame. A 10 s MP4 recorded (x264 6 Mbps + AAC, 7.6 MB) with 0 skipped frames.
- Screenshot: `native/docs/spike-0b.png`. Build: `nativeuild.bat`; self-test: `LOBS.exe --autotest`.
- Engine binaries are copied from OBS 32.2.2 for now (decision 5); building libobs from source is still to do.

### Web prototype (v0.1, `app/`) — now the design reference

The React prototype drove a running OBS over obs-websocket. It stays as the reference for layouts and interactions; the native app ports its document model and follows its screens.

- Document model with Sources → Instances, per-property link groups (Transform · Crop · Filters · Visibility), linked/independent copy, groups, full undo/redo history, live lock.
- UI: Sources, canvas with drag/resize, layer list, inspector, scene grid/list, history panel, audio signal-flow view.
- **Phase 0 passed (2026-10-19)** on OBS 32.2.2: one live 1080p camera as 20 instances, each with its own crop and color filter, on a 1080p60 canvas:

  | | Avg render time | FPS | Skipped frames | CPU | Memory |
  |---|---|---|---|---|---|
  | Baseline (1 instance) | 0.44 ms | 60 | 0 / 601 | 2.2% | 1870 MB |
  | 20 instances | 0.55 ms | 60 | 0 / 600 | 2.2% | 1885 MB |

  About +0.11 ms per frame for 19 extra filtered instances against a 16.7 ms budget, with the camera captured once. (Measured with nested scenes; the `lobs_instance` source draws one texture per layer, so it should cost about the same; not re-measured.)
- `obs-plugin/` holds the first `lobs_instance` source, built as an OBS plugin; verified on OBS 32.2.2. Its code moves into LOBS.exe.

## OBS parity gaps (audited 2026-10-20 against OBS 32.2.2's UI)
Features OBS users will look for that LOBS doesn't have yet, highest impact first.

**Must have before people switch**
- **Global hotkeys:** switch scene, start or stop stream and recording, mute and push-to-talk, show or hide a source. OBS has about 50 hotkey settings; LOBS has only in-app shortcuts.
- **Status bar:** live and recording timers, dropped frames, CPU, render and encode lag, and bitrate, plus a Stats dock. LOBS shows only fps and render time.
- **Replay buffer:** save the last N seconds to a file on a hotkey.
- **Virtual camera:** Start Virtual Camera, used for Zoom, Meet and Discord.
- **Source Properties dialog:** every setting of each source type, not just the main field: camera resolution, fps and format; capture cursor and method; browser FPS, refresh and shutdown; media restart and speed.
- **Media controls:** play, pause, restart and seek for media sources.
- **Interact:** click and type into browser sources, for logins and widgets.

**Expected in daily use**
- **Studio mode:** preview and program side by side with a Transition button, and per-scene transition overrides.
- **Projectors:** fullscreen or windowed projectors for program, preview, a scene or a source, plus Multiview.
- **Advanced audio:** sync offset, balance, downmix to mono, and per-source monitoring overrides.
- **Edit > Transform:** rotate 90 or 180, copy and paste transform, reset transform, center horizontally or vertically, and an edit-transform dialog with bounds type and alignment.
- **Copy and paste filters** between sources, and copy and paste sources as a reference or a duplicate.
- **Per-layer options:** blending mode, scale filtering, deinterlacing, and show or hide transitions on a source.
- **Preview tools:** zoom and pan, Lock Preview, safe areas and pixel-alignment guides, and screenshots of a source, scene or program.
- **Profiles:** separate stream and output settings sets, for example Twitch vs YouTube vs local recording.
- **Remux recordings:** MKV to MP4. Also Show Recordings, and a pause button while recording.

**Power users**
- Advanced output (separate stream and record encoders, multiple audio tracks per recording), the Auto-Configuration Wizard, custom browser docks (chat, activity feeds), scripts (Lua and Python), the obs-websocket API for Stream Deck and Touch Portal, log upload and crash reports, safe mode, export of LOBS shows, and "Check for Missing Files".

**LOBS already covers** undo and redo, flip, fit, center, layer order, grouping, filters (video and audio), transitions, browser sources, import from OBS, dockable panels, scene list and grid, the audio mixer with tracks and monitoring, streaming, recording, engine updates, and live lock.