# Third-party OBS plugins in LOBS — findings and plan

## How the engine loads modules today
- `native/engine-modules.txt` is an allowlist. It is used only when an engine is *staged*
  (`engine_manager.cpp` `stageEngine`): from OBS's release zip, LOBS keeps those
  modules' DLLs and data folders and deletes the rest. It is not checked at load time.
- `Engine::start` (`engine.cpp:62`) calls `obs_startup`, resets video and audio, then:
  - OBS 33+ (`<root>/core` exists): `obs_load_plugins` with `core/%module%`.
  - OBS 32 (what ships, `engine/32.2.2`): `obs_load_all_modules()`. The cwd is
    `<root>/bin/64bit`, so libobs's default search finds `../../obs-plugins/64bit/*.dll`
    and `../../data/obs-plugins/<name>`. No extra module paths are added.
  - Then `obs_log_loaded_modules()` and `obs_post_load_modules()`.
- Module config dir is `%APPDATA%/LOBS/plugin_config` (passed to `obs_startup`).

## Can third-party plugins load at all?
Yes, on OBS 32 libobs. `obs_add_module_path(bin, data)` before `obs_load_all_modules()`
adds a search path, and `%module%` in the path works the same way as OBS's own
`%ProgramData%/obs-studio/plugins/%module%/bin/64bit` user-plugin path. Things that block
or limit a plugin:
1. **obs-frontend-api.dll** (corrected after testing): the engine does ship it
   (`bin/64bit`), but it only forwards calls to a registered frontend, and LOBS registers none.
   So plugins that use it still load; their docks, menu items and event hooks do nothing
   (the log says "Tried to call obs_frontend_... with no callbacks!"). See "Next session".
2. **Qt.** LOBS ships Qt 6.11.1; OBS 32 plugins are built with OBS's Qt 6.8. Qt 6 keeps
   binary compatibility going forward, so a plugin built for 6.8 can run on 6.11. But LOBS
   doesn't ship every Qt module (e.g. `Qt6Widgets`), and a plugin's own `QDialog`
   properties window can't be shown anyway. Plugins that need Qt Widgets are reported as
   failed with the missing DLL's name.
3. **libobs version.** A plugin built for a newer libobs than 32.2 is refused by libobs
   (`obs_module_ver`), and that shows up in the log. We read it back and report it.
4. Failures in OBS 32 are only written to the log (`obs_load_all_modules` returns
   nothing). LOBS will load third-party modules itself, one at a time
   (`obs_open_module` + `obs_init_module`), so it can record the result for each one.
   A plugin that crashes on load takes LOBS down with it, same as in OBS. Mitigation: a
   "loading" marker file; if LOBS starts and finds one left behind, that plugin is
   skipped and marked "crashed LOBS on start — disabled".

## Where plugins live
- **Per-user folder**: `%APPDATA%/LOBS/plugins/<name>/` (on macOS, `~/Library/Application
  Support/LOBS/plugins`). This survives app and engine updates, needs no admin rights,
  and is never touched by the engine stager, which rebuilds `engine/<ver>` from scratch.
- Not in the app folder: `engine/<ver>` gets replaced on every engine update, and
  `Program Files` installs aren't writable.
- Layout: one folder per plugin, matching OBS 32's user-plugin layout:
  ```
  plugins/<name>/bin/64bit/<name>.dll
  plugins/<name>/data/...            (locale/, effects, shaders)
  ```
  Path given to libobs: `bin = plugins/%module%/bin/64bit`, `data = plugins/%module%/data`.
- Install converts what plugins actually ship. Release zips come in three shapes, and
  "Add" handles all of them:
  - OBS install-tree shape: `obs-plugins/64bit/<name>.dll` and `data/obs-plugins/<name>/` →
    copied to `plugins/<name>/bin/64bit` and `plugins/<name>/data`.
  - Already per-plugin: `<name>/bin/64bit/<name>.dll` and `<name>/data` → copied as is.
  - Wrapped in one extra top folder → unwrapped first.
  Installers (.exe) aren't supported; the user is told to pick the plugin's .zip instead.
- Remove: deletes `plugins/<name>` (to the Recycle Bin). Its sources stay in the show as
  "missing" (the same as a filter whose module isn't loaded) until the plugin is back.
- Changes take effect on restart (libobs can't unload modules), so the page shows
  "Restart LOBS to load/unload N plugins".

## What a plugin's sources and filters need from LOBS
- **Sources.** LOBS kinds are a fixed list (`kindInfo`, `allKinds`). Add a kind
  `obs:<source_id>`: `kindInfo` returns label = `obs_source_get_display_name`,
  icon `puzzle`, video/audio from `obs_get_source_output_flags`, size 0 (canvas) and
  then the source's own width/height once it has one. `obsKindOrNull` maps `obs:<id>` →
  `<id>`. Settings are passed through as the OBS settings object.
  Listed in Add Source under a "Plugins" group: every input id from
  `obs_enum_input_types` whose module is third-party, minus `OBS_SOURCE_CAP_DISABLED` and
  `OBS_SOURCE_DEPRECATED`.
- **Filters.** Audio filters already keep unknown ids as `obs:<id>` and pass them through.
  Video filters get the same: `obs_enum_filter_types` from third-party modules, split by
  `OBS_SOURCE_VIDEO`/`OBS_SOURCE_AUDIO`, listed under "Plugins" in each filter menu.
- **Transitions.** Out of scope for this pass. LOBS's transitions are a fixed set
  (`TransitionsDialog`). Listed on the Plugins page as "transitions: not supported yet".
- **Properties editor.** A generic editor driven by `obs_source_properties`:
  bool → switch; int/float → slider or number (with limits, step, suffix);
  text → field (multiline/password/info as such); path → file/folder picker;
  list → select (int/float/string items); color/color_alpha → color field;
  font → font picker; button → button (`obs_property_button_clicked`);
  group → section (checkable groups as switch); editable lists and frame rates → shown
  read-only as "edit in OBS". `obs_property_visible`/`enabled` are honored, and
  properties are rebuilt after each change because `modified` callbacks can reshape them.
  Plugin "Open settings" dialogs (Qt) and docks aren't hosted.
- **Saving.** `obs:` sources/filters save their settings as is in the show file, so
  removing and reinstalling a plugin brings them back.

## Plugins page (Settings → Plugins)
Shown for each plugin: name (module name, or `obs_get_module_name`), version
(from `obs_get_module_description`/version file if present, else the DLL's file version),
folder, status: Loaded · Failed (reason) · Not loaded yet (restart) · Disabled.
What each one provides: N sources, N filters, N transitions (unsupported).
Buttons: Add from folder… · Add from .zip… · Open plugins folder · Remove (per plugin).
Banner: "Restart LOBS to apply plugin changes" when the folder differs from what loaded.

## Test plan
Candidates (official GitHub releases only; each one gets asked about before downloading):
- **obs-shaderfilter** (exeldro) — a video filter and source; uses no frontend API in its
  filter path.
- **Move Transition** (exeldro) — mainly filters (move value, move source); also ships a
  transition, which we report as unsupported. Check whether it imports
  obs-frontend-api; if it does, it shows as "needs OBS's window", which is still useful
  to test.
- **obs-composite-blur** or **obs-gradient-source** as a third.
QA scenario `qa/scenarios/plugins.*`: plugin is listed as Loaded; its filter can be added to
a color layer; the filter changes the rendered pixels (preview frame differs from before).

## Out of scope for this pass
Plugin transitions, a stub obs-frontend-api, hosting Qt docks/dialogs, plugin outputs and
encoders (they load and work, but aren't offered in the Stream/Recording UI), macOS.

## What was built (2026-10-27)
- `native/src/plugins.{h,cpp}`: per-user folder (`%APPDATA%/LOBS/plugins`, or `LOBS_PLUGINS_DIR`),
  one-at-a-time loading after LOBS's modules with a status and reason for each, crash marker,
  install from .zip or folder, remove, turn on/off, and the generic `obs_properties` editor
  (describe, buttons, value conversion, and OBS's "modified" callbacks).
- Install handles three release layouts: OBS's install tree (`obs-plugins/64bit` +
  `data/obs-plugins/<name>`), OBS's per-user layout (`<name>/bin/64bit` + `<name>/data`), and the
  flat one (`<name>/<name>.dll` + `<name>/data`, e.g. Composite Blur). A plugin that's in use is
  updated or removed at the next start (`.lobs/pending`, `.lobs/remove.txt`).
- Settings > Plugins; plugin sources (`obs:<id>`) in Add Source; plugin filters at the end of the
  video and audio filter lists; `ObsProperties.qml` in the filter cards, audio effect cards and
  the source inspector.

## Test results (qa/scenarios/95-plugins.js, QA copy)
- Shader Filter 2.6.1, Composite Blur 1.6.1, Move Transition 3.2.1: all three load. Move Transition
  doesn't need the frontend API to load; it adds 9 filters. None of the three adds a source, so
  PL-03 (plugin source renders) ran no checks.
- Shader Filter: added from the filter list, settings listed (11 rows: number, bool, text, path,
  button, info), the shader turns the layer red, off brings it back, it survives save and open.
  obs-shaderfilter compiles a changed shader only on "Reload effect", the same as in OBS.
- Composite Blur drew the layer black: it starts with blur algorithm 0 ("None"), which isn't one
  of its choices, so it has no blur to draw. OBS's properties view commits a list's first choice
  when the value isn't one of them, as soon as the filter's properties open. LOBS now does the
  same when a plugin filter or source is added (`plugins::settleLists`, in the same undo step).
  With that, the blur softens the edge as it should (PL-02).
- Source Clone 0.2.3 (added for a source test): listed in Add Source, added, set to clone a red
  color source, and draws it (PL-03).
- UI (PL-07, screenshots in the run folder): Settings > Plugins lists all four with version, what
  each adds, the transitions note and the folder; the filter card shows the plugin's own settings;
  Add Source shows Source Clone with the plugin icon. All 8 stories pass.
- Run it: `$env:LOBS_PLUGINS_DIR = "$PWD\qa\runs\plugins"; qa\run.ps1 95-plugins` (the first
  launch installs from qa/plugins/*.zip, the second checks).

## Next session: plugins that add docks
Register LOBS as the frontend with `obs_frontend_set_callbacks_internal()` (what OBS's own app
does), answering the calls plugins use:
- `obs_frontend_add_dock_by_id(id, title, QWidget*)`: show the widget in a LOBS window (LOBS's
  KDDockWidgets is the Qt Quick flavor, so likely a floating window first). Needs LOBS to start
  as a QApplication; Qt6Widgets.dll already ships in the QA build.
- Tools menu items to a Plugins menu; events (scene changed, stream or record started, exit)
  from LOBS's state; scene, recording and streaming queries and controls mapped onto the engine;
  per-plugin config files.
- Calls tied to OBS's window (main window, preview, Studio Mode) return a safe empty answer, and
  the Plugins page says the plugin is partly supported.
Start with a spike: docks, menus and events only, tested on one popular dock plugin (ask before
downloading it). Also still open: plugin transitions, and plugins on the macOS branch.
