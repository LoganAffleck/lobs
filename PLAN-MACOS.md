# LOBS on macOS — Plan

Goal: LOBS runs on macOS (Apple Silicon first) as a signed, notarized `LOBS.app` with the same
document model, UI and features as Windows, except where macOS has no equivalent.

## Where we start

The good news: almost all of LOBS is already portable.

- **UI:** Qt 6 Quick + QML + KDDockWidgets, all cross-platform. 41 QML files, no platform code.
- **Document model** (`native/src/model`): source kinds are abstract (`camera`, `display`, `mic`,
  `desktop_audio`…) and mapped to OBS IDs in one table (`engine.cpp:132`). Projects are portable
  by design.
- **libobs, whisper.cpp:** both build and run on macOS (whisper gets Metal for free).

The work is in about a dozen files that call Win32 directly. There are no `#ifdef`s or
platform layers today, so each one needs a split:

| File | Windows-only code | macOS replacement |
|---|---|---|
| `main.cpp` | Engine picked at runtime: delay-loaded `obs.dll`, `SetDllDirectory`, `PATH`, helper `.exe` mirroring, RTWorkQ startup, `MessageBoxW`, `setCurrent(engineBin)` for plugin lookup | libobs bundled in `LOBS.app/Contents/Frameworks`; plugins registered with `obs_add_module_path` from `Contents/PlugIns`; `QMessageBox` |
| `engine_manager.cpp` | Swappable engines in `<app>/engine/<ver>`, installed from OBS's Windows zip, PE export checks | Engine ships inside the app (see Decision 1); module off on macOS |
| `engine.cpp` | `GetModuleHandle`/`GetProcAddress` for OBS 33 loader; `text_gdiplus_v3` for text | `dlsym`; `text_ft2_source_v2` (see Phase 3) |
| `engine_output.cpp` | NVENC / AMF / QSV / x264 encoder ladder | VideoToolbox H.264/HEVC (`com.apple.videotoolbox.videoencoder.ave.avc`, `.hevc`), x264 fallback |
| `engine_extras.cpp` | Virtual camera install via elevated `regsvr32` | System extension (Phase 5) |
| `window_chrome.cpp` | Frameless window: `WM_NCHITTEST`, `DwmExtendFrameIntoClientArea`, min/max | Objective-C++: full-size content view, transparent title bar, native traffic lights; QML title bar leaves room for them |
| `hotkeys.cpp` | Qt key → Windows `VK_*` table | Qt key → macOS `kVK_*` table; global hotkeys need Input Monitoring permission |
| `settings.cpp` | Stream key in DPAPI | Keychain (`SecItemAdd` / `SecItemCopyMatching`) |
| `crash_report.cpp` | `MiniDumpWriteDump`, `SetUnhandledExceptionFilter` | Crash marker file + attach newest `~/Library/Logs/DiagnosticReports/LOBS-*.ips` |
| `app_updater.cpp` | Downloads `LOBS-Setup-*.exe`, `WinVerifyTrust` | Sparkle 2 (EdDSA-signed appcast) or our own: `.dmg` + `SecStaticCodeCheckValidity` against our Team ID |
| `shellmeta.cpp` | Thumbnails / media duration from the Windows shell | QuickLook `QLThumbnailGenerator` + `AVAsset` |
| `app.cpp:2831` | Reveal in Explorer | `NSWorkspace activateFileViewerSelectingURLs` |
| `lobs_frame.c` | `SRWLOCK` | `pthread_rwlock_t` (works on both; could just switch) |
| `test_runner.cpp` | `GetProcessMemoryInfo`, `EnumWindows` dialog dismissal | `task_info`; dismiss via Qt window list |
| QML | `"Segoe UI"`, `"Consolas"` fonts; `Ctrl+` labels in tooltips/help | System font / Menlo; show `⌘` in labels (Qt already maps `Ctrl+` shortcuts to Cmd) |

### Sources on macOS

| LOBS kind | Windows | macOS |
|---|---|---|
| camera | `dshow_input` | `macos-avcapture` |
| display | `monitor_capture` | `screen_capture` (ScreenCaptureKit, display mode) |
| window | `window_capture` | `screen_capture` (window / application mode) |
| game | `game_capture` | **none** — hide the kind on macOS |
| mic | `wasapi_input_capture` | `coreaudio_input_capture` |
| desktop_audio | `wasapi_output_capture` | `sck_audio_capture` (ScreenCaptureKit, macOS 13+) |
| text | `text_gdiplus_v3` | `text_ft2_source_v2` |
| image, media, browser, color | same IDs | same IDs |
| NVIDIA audio/video FX filters | `nv-filters` | **none** — hide |

Engine modules for macOS: `image-source obs-browser obs-ffmpeg obs-filters obs-outputs
obs-transitions obs-webrtc obs-x264 rtmp-services text-freetype2 mac-capture mac-avcapture
mac-videotoolbox` (+ `mac-virtualcam` in Phase 5, `mac-syphon` optional).

## Decisions to make up front

1. **How the engine ships.** On Windows the engine is a separate folder LOBS can update on its
   own. On macOS, writing into a signed `.app` breaks its signature, and loading dylibs from
   outside it needs `disable-library-validation`, which weakens hardened runtime.
   **Recommendation:** bundle libobs and its plugins inside `LOBS.app`; an engine update is an
   app update. `engine_manager` stays Windows-only.
2. **Where the engine comes from.** (a) Extract `libobs.framework` and `PlugIns/` from OBS's
   official macOS DMG and re-sign them with our ID (same idea as Windows, quickest), or
   (b) build obs-studio 32.2.2 from source. **Recommendation:** (a) to get running, move to (b)
   before Phase 5, because the virtual camera extension must carry our own bundle ID and Team ID.
3. **Minimum macOS and architectures.** **Recommendation:** macOS 13 Ventura (needed for
   ScreenCaptureKit desktop audio), Apple Silicon only at first; add Intel (a second build or
   universal) once the arm64 app is solid.
4. **Apple Developer Program** ($99/yr) for a Developer ID certificate and notarization.
   Without it, Gatekeeper blocks the app outright on current macOS, so it's needed before any
   release (not before development).
5. **Updater.** Sparkle 2 is the standard on macOS and handles download, verify, replace and
   relaunch. **Recommendation:** Sparkle, with the appcast hosted next to the website; keep the
   existing GitHub-release flow on Windows.

## Phases

### Phase 0 — Builds and opens on a Mac ✅ (2026-10-24)

Done: `native/build.sh` builds `dist/mac/LOBS.app` with the engine inside (from OBS's DMG, staged
by `deps/stage-engine-mac.sh`), the editor opens, and the engine loads all 14 modules. Win32 code
sits behind `platform.h` and `*_win` / `*_mac` files. Privacy permissions are handled
without blocking (`permissions.h`): ungranted sources aren't created, a bar names what's
missing with Allow or Open System Settings, and a grant applies the show again.

Left over, for Phase 1:
- Ad-hoc signing gives every build a new identity, so macOS forgets granted permissions on each
  rebuild. Sign dev builds with one stable (self-signed) identity.
- Screen Recording often needs LOBS to quit and reopen after it's granted; say so in the bar.
- Device pickers for ScreenCaptureKit store string ids; the window id is an int there.
- The QA runner (`test_runner.cpp`, `qa/*.ps1`) is still Windows-only.

Original scope:
- `CMakeLists.txt`: `WIN32` / `lobs.rc` / Win libs only on Windows; `MACOSX_BUNDLE`,
  `Info.plist` (camera, microphone usage strings), `.icns`, entitlements file on macOS. Link
  libobs as a framework instead of `obs.lib` + `/DELAYLOAD`.
- `native/build.sh`, `deps/build-kddw.sh`, `deps/build-whisper.sh` (Metal on), and a script that
  extracts the engine from the OBS DMG into `Contents/Frameworks` + `Contents/PlugIns`.
- Split each file in the table above into `foo.cpp` + `foo_win.cpp` / `foo_mac.mm` (or
  `#ifdef Q_OS_WIN` for small cases). Stub what isn't done yet.
- Move writable files out of the app folder: logs, crash markers, downloads →
  `~/Library/Application Support/LOBS` and `~/Library/Logs/LOBS` (`QStandardPaths`). Fonts and
  icons → `Contents/Resources`.
- **Done when:** `LOBS.app` launches, shows the editor, loads a project, and the canvas renders
  color, image and media layers.

### Phase 1 — Capture, permissions and encoding ✅

Done (2026-10-24):
- Only source kinds and filters whose OBS module registered are offered, so macOS shows no Game
  Capture and no NVIDIA Audio Effects (Windows unchanged).
- VideoToolbox H.264 (Apple silicon `ave.avc`, Intel `h264.gva`) ahead of x264. Verified with
  `--streamtest` against a local RTMP server: 1080p60, ~6.2 Mbps, 0 dropped frames, while
  recording a valid 1080p60 MP4 (needed `obs-ffmpeg-mux` in `Contents/MacOS`).
- Browser source works: CEF finds its helpers by the app's name, so they ship as `LOBS Helper*.app`;
  `obs_set_cmdline_args` is passed through; CEF gets time to close pages before shutdown.
  `--engine-smoketest` passes all checks (its transition check now uses the show's transition).
- Permissions: Input Monitoring for global hotkeys, "Reopen LOBS" for permissions macOS applies
  only to a new process, devices filled in once Screen Recording is granted.
- ScreenCaptureKit window ids sent as integers; plugin data (the browser cache) moved out of
  `~/Library/Preferences` into Application Support.
- `LOBS_SIGN_IDENTITY` signs dev builds with one certificate so permissions survive rebuilds.

- Hands-on check passed with permissions granted: camera (FaceTime HD), display, window and
  desktop audio all capture live. Fixes from it: a blank placeholder in OBS's device lists was
  saved as display "0"; camera devices are listed through a probe source like screens; the Add
  dialog picks the window (required) or camera/display before adding; new window and display
  layers fit their real size instead of stretching to the canvas (Windows too).
- Signed with a "LOBS Dev" certificate, granted permissions survive rebuilds (verified).

- Setup assistant: a "Camera, mic and screen" step after the welcome (macOS only) with each
  permission's state and Allow / Open System Settings / Reopen LOBS.
- The preview dims a layer whose source waits on a permission and labels it ("Needs Camera");
  never drawn into the output.
- Quality recordings use VideoToolbox HEVC (verified: hardware, Main, 1080p60 MP4); streams,
  replays and "same as stream" recordings stay H.264.
- Displays, windows and cameras are listed through macOS directly (same values OBS's modules
  take), so no probe sources and no "Invalid target display ID" log noise.
- Fixed in passing: double-encoded text in the setup assistant ("Browseâ€¦"), on both platforms.

### Phase 2 — Feels like a Mac app ✅

Done (2026-10-24), checked by hand:
- Native menu bar: LOBS (About, Settings… ⌘,), File (New Project ⌘N, Open ⌘O, Import from OBS ▸,
  Save ⌘S, Save As ⇧⌘S), Edit (undo/redo, clipboard, duplicate, delete, group, layer order),
  Window (panels, Show All, Reset Layout), Help. New / Open / Import / Save / Window leave the
  top bar on macOS. Edit actions go to the focused text field first, then the layers.
- The Delete key (Backspace on a Mac keyboard) deletes the selected layer.
- Shortcut labels in tooltips and help read ⌘ ⇧ ⌥ (`chrome.keys`).
- Full screen: the window-button inset goes away; Settings' header sits below the window buttons.
- Clicks on anything drawn over the top bar reach it instead of moving the window.
- Default text font Helvetica Neue and Menlo for code on macOS (Segoe UI and Consolas on Windows).
- Eyedropper works with Screen Recording allowed.

- Retina: the preview already rendered at device resolution, but its handles, outlines and
  labels were sized in device pixels (half size at 2x, smaller than their 9-point grab area);
  they're sized in points now. Also fixes Windows at 150% / 200% scaling.
- File > Open Recent (last 10 shows still on disk, Clear Menu); Window menu has Minimize ⌘M,
  Zoom and Enter/Exit Full Screen ⌃⌘F.

### Phase 3 — Feature parity
- **Text:** move text layers off GDI+. FreeType2 has fewer options (no per-side outline width,
  different background handling). Either map LOBS's text settings onto `text_ft2_source_v2` per
  platform, or render text ourselves (we already draw shapes) so it looks identical on both.
  **Recommendation:** render it ourselves; text that looks different per OS breaks shared projects.
- Keychain for the stream key; global hotkeys; live captions (whisper + Metal); browser source
  and Twitch panels (CEF helper apps must be in the bundle and signed with entitlements).
- OBS import: read `~/Library/Application Support/obs-studio/basic/scenes` and map macOS OBS
  source IDs (resolves the "macOS-only sources don't come over" gap in `PLAN.md`).
- Opening a project made on the other OS: devices won't match, so show "pick a camera" on those
  sources instead of failing.

### Phase 4 — Ship it
- Sign everything inside out (frameworks, plugins, CEF helpers, app) with Developer ID and
  hardened runtime; notarize with `notarytool`; staple; build a `.dmg`.
- Sparkle updater with an EdDSA-signed appcast; release script that produces both platforms.
- Crash reports from macOS diagnostic reports.
- `BUILDING.md` gets a macOS section; README drops "for Windows".

### Phase 5 — Virtual camera
- Build `mac-virtualcam` from obs-studio source as a CoreMediaIO system extension with our
  bundle ID; needs the `system-extension.install` entitlement (granted via Apple) and the app
  in `/Applications`. Biggest unknown; can ship after v1 on macOS.

## QA

- Port `qa/run.ps1` / `build-qa.bat` to a shell or Python runner that can drive both platforms.
- Tag scenarios that need Windows-only features (game capture, NVENC, NVIDIA FX) and skip them
  on macOS; add macOS scenarios for permissions and ScreenCaptureKit sources.
- Port `make_speech.ps1` (the `say` command can make the captions clip on a Mac).

## Risks

- **CEF in a re-signed bundle:** obs-browser expects its helper apps at fixed paths with fixed
  names; getting the layout and entitlements right is fiddly. Try it in Phase 0, not Phase 3.
- **Text rendering differences** if we stay on FreeType2 instead of our own renderer.
- **Virtual camera entitlement** depends on Apple approving the request.
- **Windows regressions** from splitting files: keep the Windows build green on every step
  (a CI job for each platform would make this much safer).
