# LOBS Bug Log

Running log; the final report is compiled from this. Severity: S1 crash/data loss · S2 major
function broken · S3 minor/workaround exists · S4 cosmetic. Bugs are reported, not fixed.

---

## LOBS-001 · S1 · Any "test" in the install path or arguments silently throws away the user's show

- **Area:** startup / persistence · **Found:** Phase 0 · **Status:** Fixed in 9aba8c5 · verified in `runs/20261020-181644` (was: Confirmed (repro 1/1))
- **Where:** [main.cpp:493](../native/src/main.cpp:493), [app.cpp:127](../native/src/app.cpp:127)
- **Cause:** `selfTest = app.arguments().join(' ').contains("test")`. `arguments()` includes
  argv[0], the full exe path, and the check is a plain case-sensitive substring.
- **Effect when it matches:** the document path becomes `%TEMP%\lobs-selftest.lobs.json` and
  that file is **deleted at startup**. The user's `project/file` setting is ignored, and the
  daily engine-update check is skipped. The user gets an empty show on every launch, and
  whatever they built in the previous session is erased on the next start. No error is shown.
- **Who hits it:** anyone whose path contains "test", for example a Windows account named
  `testuser`, `contestant` or `latest`, a portable copy in `D:\Tests\`, or a Downloads folder
  under such an account.
- **Repro:**
  1. Copy `native\dist\LOBS` to a folder whose path contains "test" (I used `...\scratchpad\latest-build`).
  2. Put a marker file at `%TEMP%\lobs-selftest.lobs.json`.
  3. Launch `LOBS.exe` normally, with no arguments.
- **Expected:** a normal launch; user document at `%APPDATA%\LOBS\LOBS\current.lobs.json`.
- **Actual:** the marker file is deleted, so the app took the self-test branch. The control
  launch from `native\dist\LOBS` left the marker alone.
- **Suggested fix:** check for exact flags (`--autotest`, `--snapshot`, …) in `arguments().mid(1)`,
  not a substring of the joined command line.

## LOBS-002 · S3 (was S2) · Autosave isn't atomic; a crash or power loss mid-write can wipe the show

- **Area:** persistence · **Found:** Phase 0 (code review) · **Status:** Fixed in 5620684 (atomic writes; never reproduced as a loss) · verified in `runs/20261020-181644` (was: Not reproduced in 15 kills (see the status update at the end); code risk remains)
- **Where:** [app.cpp:195](../native/src/app.cpp:195)
- **Cause:** `QFile::open(WriteOnly | Truncate)` then `write()`. The file is truncated to 0
  bytes before the new contents are written. The write result isn't checked, and there's no backup copy.
- **Risk:** a crash, kill or power loss between the truncate and the write leaves an empty or
  partial `current.lobs.json`. Stress docs (2,000 instances) make that window larger. A
  failure such as a full disk is silently ignored.
- **Planned verification:** kill the process while it saves a stress doc in a loop; check
  whether the doc reloads.
- **Suggested fix:** `QSaveFile` (write to temp + atomic rename), keep the last good copy, and
  show a toast when saving fails.

## LOBS-003 · S4 · Self-test recordings go to the user's real Videos folder

- **Area:** test hygiene · **Found:** Phase 0 · **Status:** Fixed in 6296bfe · verified in `runs/20261020-181644` (was: Confirmed)
- **Detail:** `--autotest` uses a throwaway document but records to the normal output path.
  Running it left `C:\Users\Logan\Videos\LOBS\LOBS 2026-10-20 14-51-23.mp4` (124 KB) in the
  user's recordings.
- **Suggested fix:** in self-test mode, send output to the temp folder.

## LOBS-004 · S4 · "Media Pool" is still used for what the product calls Sources

- **Area:** terminology / docs · **Found:** reported by product owner, confirmed by search · **Status:** Fixed in bb9e54e · verified in `runs/20261020-181644` (was: Confirmed)
- **Rule:** the panel and concept are **Sources**, everywhere. "Media Pool" should not appear.
- **User-visible UI:** clean. The panel is titled "Sources" and no visible string, tooltip or toast says "Pool".
- **Where it remains:**
  - [PLAN.md](../PLAN.md) (the spec), about 15 uses: §2 heading and diagram ("Core concept: Media Pool → Scene Instances"), §3 MVP item 1, the §4b audio row, the architecture diagram, Phase 1a/1b notes, and the summary at lines 309–310. Line 239 says "Sources (was Media Pool)", so the rename was never carried through the spec.
  - Code comments and identifiers: `app.pool` / `App::pool()` ([app.h:24](../native/src/app.h:24)), "Media Pool" comments in [app.h:98](../native/src/app.h:98), [app.h:169](../native/src/app.h:169), [app.cpp:1745](../native/src/app.cpp:1745), [engine.h:63](../native/src/engine.h:63), [engine.cpp:180](../native/src/engine.cpp:180), "pool source" in [obs_import.cpp](../native/src/model/obs_import.cpp) and [audio_fx.h](../native/src/model/audio_fx.h).
  - Dock id `uniqueName: "pool"` ([Main.qml:337](../native/qml/Main.qml:337)). It's internal, but saved layouts reference it; renaming it needs a layout migration.
- **Why it matters:** the spec is what contributors (and this QA pass) read. Two names for one concept drift into UI copy, help text and support answers.
- **Suggested fix:** rename in PLAN.md and comments now. Rename `pool` → `sources` in code when convenient, and migrate the dock id while keeping `"pool"` as a fallback when restoring old layouts.

## LOBS-005 · S3 · Status bar lag warning is cumulative, so a startup hiccup reads as an ongoing problem

- **Area:** status / feedback · **Found:** Phase 2 harness run · **Status:** Fixed in ed22188 · verified in `runs/20261020-181644` (was: Confirmed)
- **Evidence:** run `20261020-150656/05-render-lag`. 5 frames lagged in the first 198 after startup (2.5%), then **0 lagged frames** over 20 s idle and 10 s of snapshot load. The status bar shows "Lagging N%" from all frames since the engine started ([engine_extras.cpp:36](../native/src/engine_extras.cpp:36), [StatusBar.qml:46](../native/qml/StatusBar.qml:46)). An earlier harness grab showed "Lagging 24.2%" in orange while the app was rendering normally.
- **Why it matters:** H1 (status must be accurate). A live operator sees an orange performance warning when nothing is wrong, and learns to ignore the one warning that matters.
- **Suggested fix:** show lag over a rolling window (for example the last 10 s), or count only since the current output started (OBS counts per output session).
- **Open:** which startup or edit operation produced the 24% burst. Measured in PF-07.

## Code-review leads (not bugs until a test demonstrates them)

- **R-01 · linked copies of groups aren't linked.** `Doc::copyToScene` gives a copied **group** independent props even when `linked` is true; only its children link ([doc.cpp:352](../native/src/model/doc.cpp:352)). PLAN §3.5 says groups are "copyable/linkable like any instance". Verify in LK-09.
- **R-02 · ungroup doesn't bake the group transform into children** ([doc.cpp:415](../native/src/model/doc.cpp:415)). If a group's transform affects how its children render, ungrouping a moved or scaled group will make children jump. Verify in LY-02 with before/after snapshots.

---
## Phase 2 run findings (run `runs/20261020-151552` unless noted)

## LOBS-006 · S2 · Live lock can be bypassed: the Inspector moves and Delete removes on-air layers
- **Area:** live safety · **Story:** LY-06 · **Status:** REOPENED by QA verification (RG-02, back doors remain; see end of file) · was: Fixed in 85f5fbb · verified in `runs/20261020-181644` (was: Confirmed (scenario `20-scenes-layers`))
- **Repro:** 1) Put a scene with a layer on program. 2) Start recording, so `live` and `liveLocked` are both true. 3) Canvas drag and nudge are correctly refused. 4) Type a new X in the Inspector: the layer moves on air (x 200 → 900). 5) Press Delete: the layer is removed from the live scene.
- **Expected (PLAN §4b):** "layers locked while live (moving one needs a modifier key or an unlock)". The lock should cover every way of moving the layer, and destructive edits on the on-air scene should be guarded.
- **Cause:** the check lives only in the canvas drag path and in `nudge()` ([app.cpp:2937](../native/src/app.cpp:2937)). `setTransform`, `scrubTransform`, `setSize`, `fit`, `deleteSelected` and others don't check `liveLocked()`.
- **Suggested fix:** one guard for every edit that targets the program scene's layers, with the same "Live lock is on" toast.

## LOBS-007 · S2 · Opening another show silently drops the current project's unsaved edits from disk
- **Area:** persistence · **Story:** PJ-01 · **Status:** Fixed in 84630d1 + c069809 · verified in `runs/20261020-181644` (was: Confirmed)
- **Repro:** 1) Create project B and add a layer, without pressing Save. 2) Open project A. 3) Look at B's `.lobs.json`: the layer isn't there (the file only has its Background). Autosave goes to `current.lobs.json`, which now holds A, so B's edit survives only in this session's undo history.
- **Expected:** Open saves the current project first (as `createProject` already does), or asks "Save changes to B?".
- **Cause:** `App::open` ([app.cpp:1595](../native/src/app.cpp:1595)) replaces the doc without `saveProject()`. The QML Open dialog calls it directly ([Main.qml:43](../native/qml/Main.qml:43)).

## LOBS-008 · S2 · A failed "New project" still wipes the current show and leaves a broken project
- **Area:** persistence · **Story:** PJ-01 · **Status:** Fixed in 91d65af · verified in `runs/20261020-181644` (was: Confirmed)
- **Repro:** New project with a 250-character name (the dialog accepts it). The folder is created, then saving the file fails because the path is too long ("Couldn't save …"). By then `newDocument()` has already run: the open show is replaced with an empty one, and the project file points at a path that can't be written.
- **Cause:** [app.cpp:1562](../native/src/app.cpp:1562) calls `newDocument()` before it knows `saveAs` will succeed.
- **Suggested fix:** check name length and path length, write the new file first, and only then switch documents. Roll back the folder on failure.

## LOBS-009 · S2 · A failed Save As repoints the project at the bad path, so every later Save fails
- **Area:** persistence · **Story:** PJ-01b · **Status:** Fixed in 4876957 · verified in `runs/20261020-181644` (was: Confirmed)
- **Repro:** Save As to a good path, then Save As to an unwritable path (`Z:/…`). Toast: "Couldn't save". `projectFile` is now the bad path, so the next Save fails too.
- **Cause:** `App::saveAs` sets `m_projectFile` and the `project/file` setting **before** opening the file ([app.cpp:1578](../native/src/app.cpp:1578)). Writes are also non-atomic (see LOBS-002).

## LOBS-010 · S3 · Project names CON/NUL/COM1 and over-long names are accepted; "CON" makes a folder Explorer can't open
- **Area:** projects · **Story:** PJ-01 · **Status:** Fixed in 6b37c3e · verified in `runs/20261020-181644` (was: Confirmed)
- **Detail:** `checkNewProject` rejects `<>:"/\|?*` and a trailing period, but accepts reserved device names and 250-character names. `createProject("CON")` returned true and created `projects\CON\CON.lobs.json` (through Qt's long-path API). Explorer, cmd and most tools can't open or delete that folder without a `\\?\` prefix. Long names fail later (LOBS-008).
- **Suggested fix:** reject reserved names (CON, PRN, AUX, NUL, COM1–9, LPT1–9, with or without an extension) and cap the name so the full file path stays under MAX_PATH.

## LOBS-011 · S3 · A linked group's own visibility and opacity don't link, and the group shows no link badge
- **Area:** linking / groups · **Story:** LK-09 · **Status:** Fixed in 1bdecc2 after the 2026-10-20 product decision · verified in `runs/20261020-185532` (was: Confirmed (confirms lead R-01))
- **Detail:** Copying a group as **Linked** links its children but not the group itself (`links: {}`). Moving the group in B does move A's copy, because the move is applied to the linked children. Hiding the group or setting it to 50% opacity in B doesn't touch A. The Layers row for the group has no link badge, so the user can't tell which actions will propagate.
- **Expected (PLAN §3.5):** groups are "copyable/linkable like any instance".
- **Why it matters:** the same object behaves as linked for one action and unlinked for another, with no signifier (Norman; H4 consistency).

## LOBS-012 · S3 · "Reset" on a linked section silently resets every linked scene
- **Area:** linking / UX · **Story:** LK-12 · **Status:** Fixed in 1e9aa16 (test updated to the decision in 71a3153) after the 2026-10-20 product decision · verified in `runs/20261020-185532` (was: Confirmed)
- **Repro:** linked pair, crop L=50 in B, then Reset on the Crop section → A's crop is also 0. No toast or confirmation, and the Undo label is "Reset crop".
- **Note:** propagating is consistent with linking, but Reset reads as a local action. Say "Reset in 2 scenes" on the button or in a toast, or offer "Reset here only (unlink)" (H5, H1).

## LOBS-013 · S3 · Every scene can be deleted, leaving no scenes and nothing on program
- **Area:** scenes · **Story:** SC-01 · **Status:** Fixed in 90150ab · verified in `runs/20261020-181644` (was: Confirmed)
- **Detail:** `App::deleteScene` has no last-scene guard, and the menu calls it directly ([Main.qml:275](../native/qml/Main.qml:275)). After deleting all scenes: 0 scenes, and edit and program are both empty. Adding a source then creates it with nowhere to place it. Undo recovers. (Deleting the scene on program does fall back correctly.)
- **Suggested fix:** disable "Delete scene" on the last scene, or recreate an empty scene.

## LOBS-014 · S3 · Grouping layers from different levels silently leaves some out
- **Area:** layers · **Story:** LY-02 · **Status:** Fixed in 7bb39a4 · verified in `runs/20261020-181644` (was: Confirmed)
- **Repro:** select a layer inside a group, Ctrl+click a top-level layer, Ctrl+G → a new group containing **only** the first layer. The second layer isn't grouped and nothing says why.
- **Cause:** `Doc::groupLayers` keeps only ids from the first id's sibling list ([doc.cpp:386](../native/src/model/doc.cpp:386)).
- **Suggested fix:** move all selected layers to the first one's level, or refuse with a message.

## LOBS-015 · S3 · Renaming a source doesn't rename the layers that use it
- **Area:** Sources / layers · **Story:** MP-03 · **Status:** Fixed in dc73d00 · verified in `runs/20261020-181644` (was: Confirmed)
- **Repro:** source "Logo" placed in 3 scenes; rename the source to "Brand Logo" → every layer row still says "Logo" (inspector source name says "Brand Logo"). The layers were never renamed by hand.
- **Expected:** layers that still carry the source's name follow it. Only a layer renamed by hand keeps its own name.

## LOBS-016 · S3 · Opening a show with a dangling layer reference shows a blank ghost layer
- **Area:** persistence robustness · **Story:** PJ-01c · **Status:** Fixed in 0407f8a · verified in `runs/20261020-181644` (was: Confirmed)
- **Detail:** a v1 doc whose scene lists `ins_ghost` (an instance that doesn't exist) opens without complaint and shows an empty-named row in Layers. Junk files (empty, garbage, wrong version, truncated) are correctly refused, and the current show stays untouched. Only structurally-invalid-but-parseable docs get through.
- **Suggested fix:** validate on load: drop missing ids from scene layers and group children, and GC links (the same checks as the QA `linkIntegrity` helper). Toast if anything was repaired.

## LOBS-017 · S4 · Edits that change nothing still add an undo step
- **Area:** undo · **Story:** UH-01 · **Status:** Fixed in 56735a7 · verified in `runs/20261020-181644` (was: Confirmed)
- **Repro:** Fill a layer, then Fill again: the history grows by one ("Fill canvas") with no change, so the next Undo appears to do nothing.
- **Suggested fix:** in `Store::commit`, skip the entry when the document is unchanged.

## LOBS-018 · S3 · "Delete source" removes it from every scene with no warning
- **Area:** Sources / UX · **Story:** MP-03 · **Status:** Fixed in d612f99 · verified in `runs/20261020-181644` (was: Confirmed (code + controller))
- **Detail:** the Sources menu item calls `app.deleteSource` directly ([Main.qml:589](../native/qml/Main.qml:589)). A source placed in N scenes disappears from all of them in one click. Undo restores it exactly (verified), but nothing says "Used in 3 scenes" (H5 error prevention).

## To verify in the UI pass
- **V-01 · Ctrl+] / Ctrl+[ didn't reorder** when sent through the harness (LY-01). The shortcuts are declared ([Main.qml:33](../native/qml/Main.qml:33)). Confirm whether other shortcuts work through the same injection path before calling it a bug. Also add a manual check.
- **V-02 · Ctrl+S always opens Save As** ([Main.qml:38](../native/qml/Main.qml:38)) instead of saving to the open project (H4: Windows convention). Check what the toolbar "Save" does.

## LOBS-019 · S3 · Ctrl+S and the toolbar "Save" always open Save As, even for a saved project
- **Area:** persistence / UX · **Story:** V-02 · **Status:** Fixed in a697855 · verified in `runs/20261020-181644` (was: Confirmed (run `runs/20261020-152146/40-ui`))
- **Repro:** Save a show as `v02.lobs.json`, add a layer, press Ctrl+S → a native "Save As" dialog opens and the file isn't updated. The toolbar Save button does the same.
- **Cause:** `Shortcut { sequence: "Ctrl+S"; onActivated: saveDialog.open() }` ([Main.qml:38](../native/qml/Main.qml:38)). `App::saveProject()` exists but nothing in the UI calls it.
- **Why it matters:** H4 (platform convention: Ctrl+S saves in place). Combined with LOBS-007 (Open doesn't save), the quick way to protect your work doesn't exist, so users either re-pick the same file every time or don't save.

## LOBS-020 · S3 · "Copy to another scene" (Linked/Independent) is only on a small unlabeled icon
- **Area:** linking / discoverability · **Story:** LK-01u · **Status:** Fixed in 4933280 · verified in `runs/20261020-181644` (was: Confirmed)
- **Detail:** the canvas right-click menu and the Layers row right-click menu (screenshots `lk01u-rowmenu.png`, `lk01u-menu.png`) offer Rename, Duplicate, Group, order and Delete, but **not** Copy to. The only way in is the 16 px copy icon in the Layers header (tooltip "Copy to another scene"). That opens scene ▸ Linked copy / Independent copy. It works and the Inspector then shows "Linked · Beta".
- **Why it matters:** linking is LOBS's headline idea (PLAN §2), but it sits behind an icon with no label (H6 recognition over recall; Norman signifiers). There's also no paste-time prompt (spec gap D2).
- **Suggested fix:** add "Copy to ▸" to both context menus. Consider Ctrl+C / Ctrl+V across scenes with the Linked/Independent choice from PLAN §2.

## LOBS-021 · S2 · Screen readers get almost nothing: 22 of 201 interactive controls have an accessible name
- **Area:** accessibility (WCAG 2.2 · 4.1.2 Name, Role, Value) · **Story:** AX-01 · **Status:** Fixed in 927c0b2 (census fix a74ba77) · verified in `runs/20261020-181644` (was: Confirmed)
- **Detail:** census of visible interactive QML items: 201 interactive, 22 with `Accessible.name`. The QML has no explicit `Accessible.*` annotations. Icon-only buttons (Layers header group/copy/delete, the tool strip, help "?" buttons, scene grid/list toggle) expose no name at all. Text buttons only work where the Basic style happens to expose their text.
- **Who's affected:** blind and low-vision streamers using Narrator/NVDA, and voice-control users ("click Copy"), who need names to target controls.
- **Suggested fix:** `Accessible.name` (and `.role`) on FButton, FIcon-only buttons and list rows, defaulting to `tip` when there's no text; a CI check that uses the same census.

## Withdrawn
- **V-01 (Ctrl+] didn't reorder):** not a bug. All shortcuts work through the window (V-01 story: Ctrl+D, Ctrl+Z, Delete, Ctrl+], Ctrl+[, Ctrl+G). The failing LY-01 run had the first-run setup assistant open, which is modal and correctly takes the keys.
- **V-02:** confirmed, now LOBS-019.

---
## Phase 3 (performance) findings

## LOBS-022 · S1 · Memory leaks on every scene switch; long sessions end in "Not Responding"
- **Area:** performance / stability · **Stories:** PF-02, PF-03 · **Status:** PARTLY FIXED per QA verification (UI leak gone; per-scene GPU buffers remain → LOBS-034) · was: Fixed in 76589e2 · verified in `runs/20261020-181644` (was: Confirmed, reproduced in 3 separate runs)
- **Evidence:**
  - `runs/probe-growth` (heavy project, 30 switches): private memory **2,627 → 5,230 MB**, about **87 MB per switch**. OS handles stay flat (about 47.6k).
  - `runs/probe-growth2`, by project size:

    | Setup | MB per switch |
    |---|---|
    | Tiny project, 2 scenes | 2.9 |
    | Heavy project, 2 plain scenes | 16.9 |
    | Heavy project, 2 camera scenes | 23.1 |
    | Heavy project, 2 display-capture scenes | 24.2 |

    The leak grows with **project** size, not scene content.
  - Full heavy run (PF-02, 100 switches, then PF-03): after the switches, a run of 30 ordinary edits never finished. Windows marked LOBS **Not Responding** at **5.4 GB** working set, and the run was stopped after 4 minutes. In a fresh process the same edits take about 90 ms, so the hang comes from the accumulated memory.
- **Repro:** open any project and switch the edited scene back and forth (click scenes in the list). Watch Private Bytes in Task Manager.
- **Why S1:** a streamer switches scenes hundreds of times in a show. At the measured rates, a heavy show gains gigabytes per hour and ends in a frozen app mid-stream.
- **Leads for the developer:** `App::editScene` → `Engine::setEditScene` (just inc/dec showing) plus `emit docChanged()`, which re-evaluates every QML binding on `app.scenes/pool/layers/inspector/history/mixer/transitions`. The per-switch cost scaling with whole-project size points at that QML rebuild. Candidates: JS arrays/maps rebuilt per change and kept alive, the `Instantiator`-driven "Copy to" submenus (one `FMenu` per scene, rebuilt on every `app.scenes` change), and delegates retained by `FluidList`. Heap-profile one switch.

## LOBS-023 · S2 · Scene switching is slow on big projects, and capture scenes freeze the UI for 1.3–1.5 s
- **Area:** performance · **Story:** PF-02 · **Status:** Fixed in 76589e2 · verified in `runs/20261020-181644` (was: Confirmed)
- **Targets:** select or show a scene ≤ 100 ms p95 (heavy project).
- **Measured (heavy: 40 scenes, 104 sources, 424 layers, `runs/20261020-153*/60-perf-heavy`, 100 switches):**

  | | p50 | p95 | max |
  |---|---|---|---|
  | Select scene for edit | **2,633 ms** | **4,998 ms** | **6,295 ms** |
  | Show (cut) to program + frame | 195 ms | 457 ms | 666 ms |

  The first switch took 123 ms. The growth tracks LOBS-022.
- **Without the leak (fresh process, `probe-growth2`):** plain graphics scenes take a steady **310 ms** per switch, against 14 ms in a tiny project, so the cost scales with the whole document. Scenes containing **display capture take 1.3–1.5 s** per switch, and camera scenes take 0.4–0.9 s: activating a capture appears to block the UI thread.
- **Why it matters:** a live operator clicking between scenes gets a frozen UI for up to seconds, and the on-air switch lags behind the click.
- **Suggested direction:** make edit-scene changes not rebuild document-wide QML models (separate `editSceneChanged` from `docChanged`). Keep capture sources warm, or activate them off the UI thread.

## Performance results so far (heavy project)
| Metric | Target | Result |
|---|---|---|
| Open → interactive | ≤ 3 s | **0.96 s** ✅ |
| Save | ≤ 500 ms | **8 ms** ✅ |
| UI stall around autosave | ≤ 50 ms | **51 ms p95** (borderline; measurement includes ~10 ms timer slop) |
| Select scene for edit | ≤ 100 ms p95 | **5.0 s p95** ❌ (LOBS-022/023) |
| Show scene (cut) | ≤ 100 ms p95 | **457 ms p95** ❌ |
| Linked edit, 30 scenes (fresh process) | ≤ 100 ms | **87–113 ms** (borderline) |
| OS handles after opening heavy | — | 4.9k → **48.6k** (note; watch on stress) |

## LOBS-024 · S2 · The editor runs at about 8 fps on a heavy project (drag, scrub, click, edit)
- **Fix pass 3:** PF-04-heavy passes (`runs/20261023-175335`, `-175429`): drag frame p50 16.66 ms, p95 17.2–17.3 ms, max 18.9 ms, 0 lagged frames. Every frame is within one refresh. The failure was the harness (4b6ef3e, `qa: test fix`): moves on a 16 ms timer left some frames without a move, so they rendered nothing and the check read a 33 ms gap. PF-04 now drives one move per frame, and its budget is in screen refreshes plus 2 ms for timestamp jitter.
- **Area:** performance · **Stories:** PF-03, PF-04, PF-05, PF-06 · **Status:** Fixed in 76589e2 (heavy); PF-04-heavy UI frame p95 34 ms vs 16.7 target, see report · verified in `runs/20261020-181644` (was: Confirmed (`runs/20261020-155724/62-perf-heavy-b`, fresh process; `runs/probe-select`))
- **Measured, heavy project (40 scenes / 424 layers), against the targets:**

  | Action | Target | Measured |
  |---|---|---|
  | UI frame interval while dragging a layer | p95 ≤ 16.7 ms | **p50 131 ms, p95 209 ms, max 679 ms** (about 8 fps) |
  | Each canvas drag move | — | p50 111 ms, p95 189 ms |
  | Inspector scrub step | p95 ≤ 33 ms | p50 100 ms, p95 241 ms |
  | Click a layer → Inspector, fresh process | p95 ≤ 100 ms | **p50 230 ms, p95 411 ms** |
  | Click a layer, after earlier edits (leak) | — | **1.3–2.8 s per click** |
  | One edit (setTransform) | p95 ≤ 100 ms | p50 90 ms, p95 225–409 ms |
  | Linked edit across 30 scenes | p95 ≤ 100 ms | p50 158 ms, p95 216 ms |

- **The output stays clean:** 0 lagged frames over 20 s on program, and 0 over 10 s with every scene active. Only the editor UI is slow.
- **Cause (from the code):** every change, including selection (which isn't even an edit), runs `App::onStoreChanged`/`emit docChanged()`. That means a full `Engine::apply` over all sources (builds settings JSON and a signature for each), `rebuildHotkeys()` (re-registers global hotkeys on **every** edit), and re-evaluating every QML model. Cost is O(project) per keystroke or mouse move.
- **Suggested direction:** separate signals (`selectionChanged`, `editSceneChanged`, `docChanged`), incremental engine apply for the instance that changed, and hotkeys rebuilt only when scenes or sources are added or removed.

## LOBS-025 · S1 · The stress project (200 scenes, 3,270 layers) is unusable
- **Fix pass 3:** PF-04-stress passes 3 of 3 (`runs/20261023-174942`, `-175100`, `-175217`): drag frame p95 **23.3–24.0 ms** [≤ 35.3], was 50.3. GDI after open **382** (was 2,006), handles 196,384 (was 237,807).
  - **Drag (38a3cff):** with the UI's signals blocked the drag was already 16.8 ms p95 (`_probe-drag-frames.js`). The UI cost had two parts. First, every drag frame rebuilt the whole Inspector, and QML re-evaluated it all (link info naming 100 scenes), about 7 ms a frame. Its transform fields now read `app.inspectorTransform`, the only part rebuilt mid-drag. Second, each mixer strip read `app.levels`, converting the whole map per strip, 20 times a second; it's now read once per list. IN-02u checks the Inspector after a drag.
  - **GDI (fd66e18):** each of the 818 text layers kept a `text_gdiplus` renderer (~2.5 GDI objects). Parking now frees it, and it's rebuilt when the scene shows. LY-08 checks it draws the same on return. Open and switch times are unchanged.
  - **Still open:** levels still cost about 10 ms per update on stress (`t.signalCost`). The stress fixture gives all 704 sources an `audio` block (`gen_project.py` writes one per source), so the mixer has 704 strips, and LOBS marks a source `hasAudio` whenever the block exists. Real color, text and image sources have no audio. Either fix the generator or make the loader ignore `audio` on kinds that can't play sound; both are a QA or product call. Also open: handles still follow the OBS source count, and the LOBS-036 per-layer sharing.
- **Area:** performance / scale · **Stories:** PF-01, PF-02 (stress) · **Status:** Partly fixed in 76589e2. Stress still misses targets (PF-01/02/03/04/06-stress) · verified in `runs/20261020-181644` (was: Confirmed (`runs/20261020-160525/61-perf-stress`))
- **Measured:**
  - Open → interactive: **12.3–17.1 s** (p50 13.0 s; target ≤ 10 s).
  - UI freeze around autosave: **up to 6.0 s** (target: never > 1 s). Save itself: 68 ms.
  - **15 scene switches didn't finish in 6+ minutes.** The run was killed at its 8-minute cap.
  - After three opens: **11.5 GB private memory, 9.0 GB working set, 271k OS handles, 2,003 GDI objects** (from 2.0 GB, 5k handles, 37 GDI). Some of this is the leak in LOBS-022.
- **Risk beyond slowness:** Windows' default GDI limit is 10,000 objects per process. At about 3 GDI objects per source, a project with roughly 3,000 sources (or fewer, with more text layers) will hit the limit and fail to draw, or crash.
- **Remaining stress perf stories (PF-03…PF-07) were not run.** They depend on switching and selection, which this bug blocks. Logged as a coverage gap.

## LOBS-026 · S2 · Importing an OBS collection while live puts the imported scene on air
- **Area:** import / live safety · **Story:** IM-03 · **Status:** Fixed in 84630d1 · verified in `runs/20261020-181644` (was: Confirmed (`runs/20261020-161451/70-import`))
- **Repro:** put a scene on program and start recording (or streaming). Import → OBS collection. Program switches to the imported collection's current scene ("Be Right Back / Starting") immediately. The current show is also replaced without saving (same root cause as LOBS-007).
- **Cause:** `App::importObs` calls `editScene(...)` then `take()` unconditionally ([app.cpp:1663](../native/src/app.cpp:1663)).
- **Suggested fix:** while live, import without taking (or block import with "Stop streaming/recording first"), and save or confirm the current show first.

## LOBS-027 · S3 · OBS import silently drops scenes built with the old LOBS-for-OBS plugin
- **Area:** import · **Story:** IM-01 · **Status:** Fixed in 7db9513 · verified in `runs/20261020-181644` (was: Confirmed)
- **Detail:** `Untitled.json` (the owner's real collection) has 10 scenes. Five come over and match OBS exactly: same layers per scene, same hidden items (Camera Small, Kofi, Window Capture). The other 5 are skipped on purpose by `isLobsHelper()` ([obs_import.cpp:99](../native/src/model/obs_import.cpp:99)): `[LOBS] Pool`, plus **BRB, Gameplay, Just Chatting, Starting Soon**, which are real scenes whose layers were all plugin `lobs_instance` items. The toast reads "Imported 5 scenes and 15 sources… Not carried over: 2 sources with no LOBS equivalent, 1 device not connected". It never mentions the 4 user-visible scenes that disappeared.
- **Why it matters:** H1 / H9. The people most likely to have these scenes are early adopters who used the plugin. They lose their scenes without being told.
- **Suggested fix:** list the dropped scenes in the import note. Better: rebuild them from the plugin's `[LOBS] Pool` sources, since the `lobs_instance` items reference them.

## IM-02 note (pass, with one observation)
Truncated, empty and non-OBS files are refused ("Not an OBS scene collection") and the open show is untouched. Two parseable but degenerate files (`sources: []`, and wrong value types) import as a 0-scene or 1-scene show and replace the current one. Undo recovers. A 0-scene import probably shouldn't replace the show; this overlaps LOBS-013.

## LOBS-028 · S3 · Setup assistant recommends a bitrate above a slow connection's whole upload
- **Area:** setup assistant · **Story:** SA-02 · **Status:** Fixed in 9e39160 · verified in `runs/20261020-181644` (was: Confirmed (`runs/2026102016*/80-misc`))
- **Detail:** `recommendSettings(1000 kbps, "Twitch")` → **1,500 kbps** at 852×480@30. The bitrate is clamped to a 1,500 kbps floor ([app.cpp:1493](../native/src/app.cpp:1493)), so any upload under 2 Mbps is told to send more than 75% of it. At 1 Mbps it's told to send more than the line can carry. The result is dropped frames and buffering, with no warning. The 3, 6, 10 and 50 Mbps cases are sensible (2.2k / 4.5k / 6k / 6k, capped at Twitch's 6,000).
- **Suggested fix:** below about 2 Mbps, recommend 75% of the measured upload and say plainly that the connection is too slow for a stable stream.

## LOBS-029 · S3 · A mic set to "Monitor and output" gets no echo/feedback warning
- **Area:** audio · **Story:** AU-02 / AU-03 · **Status:** Fixed in 352dae0 · verified in `runs/20261020-181644` (was: Confirmed (spec gap D5, partly built))
- **Detail:** the mixer has warnings for monitoring **desktop audio** ("Echo risk…"), browser sources and track 1 ([app.cpp:2727](../native/src/app.cpp:2727)). A **microphone** monitored to output gets `warnings: []`. Through speakers, that's the classic feedback howl. PLAN §3.12 / §4b calls for "an echo warning".
- **Suggested fix:** warn on mic + "Monitor and output" ("You'll hear yourself; use headphones or the mic will feed back").

## LOBS-030 · S4 · Opacity isn't clamped: values outside 0–1 are stored and saved
- **Area:** inspector robustness · **Story:** CV-06 · **Status:** Fixed in d5432f8 · verified in `runs/20261020-181644` (was: Confirmed)
- **Detail:** `setOpacity(1.7)` and `setOpacity(-1)` are stored as-is and written to the show file. Width and height clamp to ≥ 1 and corner radius clamps to 50%, so opacity is the odd one out. The UI slider probably keeps values in range, but scripts, imports and hand-edited files don't.

## Status update: LOBS-002 (autosave non-atomic)
- **Not reproduced.** `tools/crash_autosave.ps1` killed LOBS 15 times on the stress project (4 MB doc) at random moments after autosave was running. All 15 autosaves were valid (`runs/20261020-161813-crash-autosave`). Also, continuous editing doesn't starve autosave (PJ-02b: 38 edits → 38 saves, longest gap 0.7 s). The truncate-then-write pattern and the unchecked write result are still in the code, so the risk remains for power loss and full disks (not tested). **Downgraded: S2 → S3 (code risk).**

## LOBS-031 · S2 · The Sources panel is always empty (regression from 76589e2)
- **Area:** Sources · **Story:** MP-04u · **Status:** Fixed in a5da0f7 · verified in `runs/20261020-181644`
- **Detail:** the Sources list's `RowsModel { rows: poolRowsFn() }` called the function unqualified. In the model's scope that name doesn't resolve, so the binding threw and the list stayed empty no matter how many cameras or captures were added. Introduced by the leak fix and found while writing the LOBS-018 UI story.

## LOBS-032 · S2 · Steady editing holds off autosave indefinitely
- **Area:** persistence · **Story:** PJ-02b · **Status:** Fixed in 888acb3 · verified in `runs/20261020-182650`
- **Detail:** every edit restarted the 400 ms autosave timer. When edits got fast (76589e2), one edit every 250 ms gave 0 autosaves in 20 s (`runs/20261020-181644/32-autosave-starve`). Autosave now also fires at most 2 s after the first unsaved edit.

## Fix-pass notes (2026-10-20)
- **OUT-03 (virtual camera)** died once in the full run `20261020-181644` and passed alone in `20261020-182650` and in `run-output` `20261020-182828-output`. Flaky, cause not found.
- **OUT-04:** `received.flv` reports 0.00 s duration in ffprobe although the story passes. The receiver probably closes before the FLV duration is written. Not investigated.
- **Live lock (follow-up to LOBS-006):** after the product decision, every edit to what's on air is locked: visibility, opacity, filters, rename, duplicate, link and reset, adding layers, and on-air source settings. Alt or Unlock overrides it, and Settings > Stream > Live lock turns it off (2dde2ba, LY-06).
- **OUT-02 / OUT-03** fail or die only when `50-output` runs after other suites in one `run.ps1` call (`20261020-185702`). Alone they pass (`20261020-185848`). Flaky, cause not found.
- **Open while live** replaced what was on air. Now refused, and Open/Import are disabled while live (2a7f1f3, PJ-06).
- **An untitled show was lost on Open or Import.** These now ask Save… / Don't save / Cancel when it has changes (3b214b1, PJ-07, PJ-07u).


---
## QA verification pass on the fix commits (HEAD `2f3ad18`, 2026-10-20)

Independent rerun by QA from a clean rebuild of both builds. Evidence: `runs/20261020-190750` (functional), `runs/20261020-191132` (QA regression probes, `90-regression.js`, written by QA after reading the fix diffs), `runs/probe-leak2-head`, and `runs/*-output` (output).

- **Functional suite: 62/62 pass** (was 38/62). Every story the fix pass claimed now passes in QA's run too.
- **Test changes reviewed:**
  - LK-12 now follows the product decision. It's stricter, which is fine.
  - LY-06, PJ-01d/01e/04–07, MP-04u and LK-01m add regression checks, which is fine.
  - AX-01 no longer counts static `Text` as a control. That's defensible, but it shrinks the denominator: **102 of 127 named (80.3%)**, just over the bar.
  - No check was weakened to hide a failure.
- **Reopened or new below:** LOBS-006 (reopened), LOBS-022 (partly fixed → LOBS-034), LOBS-033 (new), LOBS-035 (new), and one product question.

## LOBS-006 · REOPENED · Live lock still has back doors that change what's on air
- **Fix pass 2:** Fixed in 4d38c43 · verified in `runs/20261020-204324` (LY-06, RG-01..04 pass). Visibility and opacity are live controls. Guarded now: deleteSource (on-air layers), deleteScene (program), addBackground. New, Sample and New project are refused while live.
- **Story:** RG-02 (`90-regression.js`) · **Status:** Closed 2026-10-30. RG-01..04 pass in `runs/20261030-043538`, and LY-06 in `runs/20261030-043643`. A fresh scan of `app.cpp` for edits with no live-lock check found none that change what's on air: scene and source names, swatches, layer locks and mixer order don't show; visibility and audio are live controls; transitions apply to the next cut; cancel crop only puts back the crop there was. (Was: Reopened, fix 85f5fbb/2dde2ba incomplete; fixed in 4d38c43.)
- While recording, with live lock on and scene "OnAir" on program:
  1. **The Layers eye icon hides the on-air layer.** `App::toggleVisible` has no guard, while Inspector visibility (`setVisibleFlag`) is refused. Same action, two answers (H4).
  2. **Delete source removes the on-air layers.** `App::deleteSource` has no guard.
  3. **Deleting the on-air scene is allowed.** Program jumps to another scene ("OffAir") mid-recording. `App::deleteScene` has no guard.
- Also unguarded (not checked for on-air effect): `renameSource`, `addBackground`, `setSourceSetting` paths reached through other callers. A static scan of `app.cpp` found 20+ committing functions with no `live*EditAllowed` call; the three above change what viewers see.
- **Suggested fix:** route those three through `liveEditAllowed`/`liveSourceEditAllowed`/`liveSceneEditAllowed`. Or, if the eye icon should stay usable live (see the product question below), make the Inspector match it.

## LOBS-033 · S3 · While live, arrow-key nudge is refused on scenes that aren't on air
- **Status:** Fixed in abd8ac9 · verified in `runs/20261020-204324` (RG-01, RG-04)
- **Story:** RG-01 · **Status:** Confirmed (pre-existing; the old blanket check survived the new guard)
- **Repro:** while live, edit an off-air scene, select a layer, press an arrow key → "Live lock is on. Unlock to nudge while live." The layer doesn't move. Inspector edits, filters and adding sources on the same off-air scene all work.
- **Cause:** `App::nudge` calls the new `liveEditAllowed(m_selection)` (correct), then still has `if (liveLocked()) return showToast(...)`, which refuses regardless of scene. That second check also ignores Alt, despite the toast.
- **Why it matters:** preparing the next scene while live is the main reason the edit/program split exists (SC-02).

## LOBS-022 · Partly fixed · UI leak fixed; memory still grows with every scene you visit (see LOBS-034)
- **Fix pass 2:** Fixed with LOBS-034 (40cd61c, 40e039c) · RG-05 passes in `runs/20261020-204324`.
- **QA result:** RG-05 (`runs/20261020-191132`) and `runs/probe-leak2-head`:

  | Case | Retained per switch |
  |---|---|
  | Tiny project, with or without rendering | ~0 MB ✅ |
  | Heavy, 100 switches across all 40 scenes, rendering | **33.7 MB** (2,661 → 6,034 MB) |
  | Heavy, 50 switches with a frame each | **56 MB** |
  | Heavy, 50 switches without frames | 17 MB |
  | Heavy, the same 2 scenes back and forth | **0 MB** |

  Memory isn't returned after 10 s idle plus GC.
- **Reading:** the unbounded UI leak (QML rows and menus) is fixed, so a click retains about 0.4 MB and repeated switching between visited scenes retains 0. What remains is a one-time cost per scene rendered that is **never released**. That's LOBS-034. The fix commit's "≈0 MB per 100 switches" figure holds only for switches between already-visited scenes, or without rendering.
- **Speed is fixed:** switch + frame p95 **52 ms** (was 5.0 s), click + frame p95 **29 ms** (was 411 ms).

## LOBS-034 · S2 · Every layer keeps a source-sized GPU buffer after its scene stops showing
- **Status:** Fixed in 40cd61c (+ re-entry guard 40e039c) · verified in `runs/20261020-204324` (RG-05, LY-07)
- **Confirmed first** (`_probe-leak2`, run `20261020-192836`): revisiting 2 scenes retained 0.04 MB/switch; new heavy scenes 60 MB/switch, nothing freed after 10 s idle. Three per-layer source-sized textures: lobs_instance's offscreen copy, the scene item's scale-filter texture, and each OBS filter's target. The scale-filter one was the biggest (a canvas-sized color layer = 8 MB). Turning the filter off on a hidden layer doesn't free it, because hidden private scenes never tick.
- **Fix:** offscreen copy only for cropped layers, freed on hide; no scale filter for color and shape layers; hidden scenes' layers have their scene items and filters detached ("parked") and reattached right before the scene shows (edit, program, transitions, grid thumbnails, projectors).
- **Result:** heavy after visiting all 40 scenes: 1,966 → 415 MB above the empty show (`_probe-leak3`). RG-05 passes.
- **Area:** memory / scale · **Stories:** RG-05, PF-*-stress · **Status:** Confirmed by measurement; cause from code
- **Cause:** `li_video_render` creates `li->texrender` at the **source's** full size on first render, and frees it only in `li_destroy` ([lobs_instance.c:140](../native/src/lobs_instance.c:140), [:77](../native/src/lobs_instance.c:77)). There's nothing on hide or deactivate. Color sources are canvas-sized (1920×1080×4 = 8 MB each), the camera is 8 MB, and a 4K display capture is about 33 MB. Every layer of every scene ever shown keeps its buffer.
- **Effect:** memory ≈ the sum over **visited** layers. Heavy (424 layers) settles around 3.4 GB. Stress (3,270 layers) would need tens of GB, which matches the stress numbers below.
- **Suggested fix:** release the texrender when the instance stops showing (`hide`/`deactivate` callback), or share one per source (not per layer). Also, skip the texrender entirely when no crop or radius is set, and draw plain color sources directly instead of rendering a canvas-sized texture.

## LOBS-035 · S4 · The recording error still tells Windows 11 users about "Windows 10 Ransomware Protection"
- **Status:** Fixed in 1ded3e6 · verified in `runs/20261020-205127-output` (OUT-01b now reads "Windows Security → Controlled folder access can also block it…")
- Was a UX note in the report (not logged as a bug). Still present: OUT-01b `lastError`/status text. It comes from OBS. Suggested wording: "Windows Security → Controlled folder access".

## Product decision (2026-10-20, from the product owner): visibility and opacity are live controls
- While live and locked, **show/hide and opacity stay allowed** on the on-air scene, from both the Layers eye icon and the Inspector. A show runner needs to hide and show overlays or a temporary CTA mid-show.
- **Everything else on air stays locked:** move, size, crop, corners, filters, rename, duplicate, reorder, group, add or delete layers, delete source, delete the on-air scene, source settings, link and reset.
- The tests encode this: LY-06 and RG-02 (updated by QA). Both fail on `2f3ad18`, because the Inspector refuses hide and opacity while Delete source and Delete scene still slip through.

## Perf verification (HEAD `2f3ad18`, `runs/20261020-191455`)
- **LOBS-023:** verified fixed on heavy. Select scene p95 60 ms (was 5.0 s); Show (cut) p95 25 ms (was 457 ms).
- **LOBS-024:** mostly fixed.
  - Click p95 31 ms, edit p95 14 ms, linked edit p95 48 ms, scrub p95 24 ms.
  - **Still over target:** UI frame interval while dragging, p95 **33.8 ms** / p99 40.5 ms / max 94 ms (target p95 ≤ 16.7 ms). Typical (p50 17 ms) is about 58 fps; the worst 5% of frames drop to about 30 fps.
- **LOBS-025:** still partly fixed. Stress is now usable but misses targets:
  - Pass: open 8.3 s ✅, Show p95 190 ms ✅, click p95 149 ms ✅, edit p95 21 ms ✅.
  - **Fail:** select scene p95 339 ms (≤ 250), linked edit across 100 scenes p95 393 ms (≤ 250), drag frame p95 114 ms (≤ 33), scrub p95 120 ms (≤ 66), **autosave stall up to 2.9 s** (never > 1 s), memory 898 KB per edit. That's bounded by the 200-step history: about 175 MB.
  - Resources after open are unchanged: **6.4 GB private, 270k handles, 2,003 GDI objects**. This is LOBS-034, and the GDI risk from LOBS-025 remains.

## Fix pass 2 results (HEAD after 78ddb11)
- **LOBS-021 remainder:** 52cad55 names Go live, the switch tracks, the mixer and the scene and asset grids. Still unnamed: the MouseArea inside KDDockWidgets' TabBarBase (C++, QML can't attach a name to it).
- **LOBS-025:** partly fixed in 78ddb11. Stress PF-01 (autosave stall), PF-02, PF-03 and PF-05 now pass (`runs/20261020-204324`). Still failing:
  - **PF-04-stress** drag frame p95 79 ms [≤ 33] and scrub p95 87 ms [≤ 66]. Moves cost 3.5 ms; most of the frame is QML rendering the window (12 ms p50, 58 ms p95; `_probe-drag-stress`).
  - **PF-06-stress** memory per edit 1,021 KB (→ LOBS-036).
  - **GDI and handles after opening stress:** 2,003 GDI objects, 238,811 handles, 4.7 GB private. The 818 text layers each own a GDI+ text renderer (`text_gdiplus`), which accounts for the GDI count. The handle count tracks OBS source count (3,270 layer instances + 818 text renderers, each with OBS's per-source locks and events). Not fixed.
- **LOBS-024:** heavy PF-04 drag frame p95 33.6 ms [≤ 16.7] stays failing. Moves cost 1.5 ms and frames 2.8 ms (p50); the interval is exactly 2 vsyncs because the harness injects moves on a 16 ms timer against a 16.7 ms vsync (`_probe-drag`, `runs/20261020-202948`). I think the check measures the harness, not the app; the test is unchanged.

## LOBS-036 · S3 · Each edit on a very large show keeps about 1 MB of undo history
- **Story:** PF-06-stress · **Status:** Worked around: history is capped by show size (see below); per-layer sharing still open
- **Detail:** 200 edits on the stress project grow memory about 1,021 KB per edit (`runs/20261020-204324/63-perf-stress-b`). Each undo step is a whole Doc. Docs share data copy-on-write, but `instances` is one QHash of 3,270 entries: editing one layer copies the whole table.
- **Suggested fix:** share instances individually (for example a QHash of implicitly shared instance pointers), so an edit copies one layer. A model-wide refactor, so not done in this pass.
- **Stopgap (product owner: "we don't need to store all the history"):** undo history keeps up to 200 steps, but no more than about 64 MB worth (about 300 bytes per layer per step), with at least 30. Shows under about 1,000 layers keep all 200; the stress show keeps 64. PF-06-stress passes and UH-03 checks a normal show still keeps 200 (`runs/20261020-210508`). The per-layer sharing refactor would make each step small instead; it's still worth doing if huge shows need deep undo.

## LOBS-037 · S2 · LOBS freezes on exit after a show with audio devices and a browser source
- **Area:** shutdown / engine · **Found:** 70-import (2026-10-23) · **Status:** Fixed (see below); verified 3/3 in `runs/20261023-142232`, `-142308`, `-142344`
- **Where:** [main.cpp](../native/src/main.cpp) (`RtwqStartup` before libobs loads, `RtwqShutdown` after `engine.shutdown()`)
- **Symptom:** all 4 import stories pass, then the window stops responding until Windows ends it (exit 0xCFFFFFFF; `qa\run.ps1` shows TIMEOUT/Crashed). Real users would see the same "LOBS is frozen" on quit. The show is already saved by then, so nothing is lost.
- **Stack (cdb):** main thread `obs_shutdown` → `obs_wait_for_destroy_queue`; the libobs destroy thread in `win_wasapi` `WASAPISource::Stop()` → `WaitForSingleObject(idleSignal, INFINITE)`.
- **Cause:** win-wasapi picks its capture mode per source: RTWQ work queues if `RTWorkQ.dll` is *already loaded* when the source is created, else its own capture thread. OBS's `main()` loads RTWorkQ and calls `RtwqStartup()` up front and holds it for the whole run. LOBS didn't, so:
  1. Sources created early get capture threads (fine).
  2. Importing a collection with a browser source loads CEF, whose MFPlat import loads RTWorkQ. WASAPI sources created after that (IM-03 imports again) use RTWQ.
  3. Media Foundation later drops its RTWQ reference, which shuts the platform down. The RTWQ source's queued `startCapture` work item never runs.
  4. On exit `Stop()` sets `stopSignal` and waits for the work item to answer with `idleSignal`. It never comes.
- **Evidence from the hung process:** the stuck `WASAPISource` has `rtwq_supported = 1`, no capture thread, `stopSignal` set, `idleSignal` clear, and its reconnect thread idle waiting for `reconnectSignal`. The other two WASAPI sources were in thread mode and fine. The fixtures' foreign device ids keep sources in reconnect mode, which makes the timing near-certain, but the cause is LOBS not owning RTWQ. It's not an OBS bug.
- **Fix:** do what OBS does: `LoadLibrary("RTWorkQ.dll")` + `RtwqStartup()` at the top of `main()`, and `RtwqShutdown()` + `FreeLibrary` after `engine.shutdown()`. Every WASAPI source now uses RTWQ, and the platform stays up until the engine is gone.
- **QA check added:** `qa\run.ps1` now waits only `-ExitSec` (default 30) after `results.json` reaches `done`. If LOBS is still running, the scenario is killed and reported as `HUNG_ON_EXIT`, and the summary has an `ExitSec` column. On the unfixed build, 70-import reports `HUNG_ON_EXIT` after 66 s (`runs/20261023-142058`) instead of a 6-minute TIMEOUT. With the fix it exits 0 within 1 s, three runs in a row.
- **Full suite with the fix** (`runs/20261023-143229`): all 15 scenarios exit on their own within 0–6 s, and none crashed. The 4 story failures were already there before this change: PF-04-heavy and PF-04-stress (drag frame time, LOBS-024/025); CC-01 (no speech model in the isolated profile, also failed on 10-22); RG-05 (0.98 MB per switch, and 0.66/0.93 MB on unfixed HEAD in `runs/20261023-144103`, `-144122`, so it isn't from this change but it has regressed since `runs/20261020-204324`).

## LOBS-038 · S4 (QA harness) · RG-05 fails without a regression: it reads memory while ~250 MB is still being released
- **Test fix:** RG-05 now reads memory with `settledMB()` (`_lib.js`): gc, then wait for a 4 s window with under 5 MB of change, giving up at 15 s. Before each drop, freed memory holds for about 3 s after parking ends, then falls a few hundred MB about 1 s at a time, and GC doesn't hurry it (`_probe-rg05tail.js`, `runs/20261023-154027`). A 1 s window was fooled by that hold: 6 of 6 runs failed. On HEAD b60b9e5, 6 of 6 runs pass, at 0.15–0.25 MB/switch (`runs/20261023-154700` … `-155109`). A real leak never comes back, so settled readings still catch it.
- **Now visible:** clicks retain **0.41–0.48 MB each** in those 6 runs, just under the 0.5 limit. The old reading hid this: it showed −1 to −2 MB/click while the switching memory was still being released. It matches the ~0.4 MB/click noted under LOBS-022. It isn't a failure, but there's little margin left.
- **Story:** RG-05 (`90-regression.js`) · **Status:** Test fixed (`qa: test fix: RG-05 reads memory once it settles`). It was a test defect, with no culprit commit. LOBS-034 stays fixed.
- **Bisect:** no commit after the passing run makes RG-05 fail. The code that passed in `runs/20261020-204324` (78ddb11, rebuilt at 5e11028, a BUGS.md-only commit after it) **fails today, 8 of 8 runs**, at 0.77–1.60 MB/switch (`runs/20261023-144935`, `-145013`, `-145243`, `-145302`, `-145902`, `-145921`, `-150017`, ...). It fails with the generator's own fixture (`gen_project.py heavy`) as well as with the re-saved one, and with the default panel layout as well as with a layout left over from 10-21. Each later point looks the same: 62ecfd0 0.75/0.40/0.76, fe41711 0.88/0.50/0.46, HEAD b60b9e5 0.59/1.05.
- **What the numbers show:** `_probe-rg05.js` records private memory after the open, then over 3 rounds of the same 100 switches RG-05 makes. Each round ends with gc + 1 s (the RG-05 reading) and gc + 6 s:

  | Run | After open, 0 s → 8 s | Round 1/2/3, gc + 1 s | Round 1/2/3, gc + 6 s |
  |---|---|---|---|
  | 5e11028 `runs/20261023-151256` | 2,235 → 2,055 | 2,327 / 2,343 / 2,343 | 2,075 / 2,088 / 2,091 |
  | HEAD `runs/20261023-152226` | 2,218 → 2,048 | 2,320 / 2,319 / 2,324 | 2,068 / 2,069 / 2,073 |
  | HEAD `runs/20261023-152423` | 2,072 → 2,022 | 2,296 / 2,296 / 2,297 | 2,047 / 2,044 / 2,050 |

  Settled memory after 300 switches is within about 25 MB of the start (≈ 0.08 MB/switch), and later rounds add nothing. RG-05 reads `start` 0.5 s after the open, while the parking of hidden scenes is still freeing memory (it falls another 50–180 MB). It reads `after100Switches` 1 s after the last switch, while about 250 MB of the switching is still in use. The check compares two readings taken while memory is still dropping, so the result depends on timing. On 10-20 the start reading landed as high as the end one (start 2,533, end 2,522); today it lands lower (start ≈ 2,230, end ≈ 2,320).
- **Why so much is in flight:** during fast switching (one frame each), memory climbs 300–950 MB above the start and falls back in steps. `Engine::setEditScene` unparks each scene's layers (scene items, scale-filter textures, filters). Parking runs at most every 500 ms on `m_parkTimer` (`parkHiddenFilters`, [engine.cpp:659](../native/src/engine.cpp:659)), about 30 switches at this pace, and OBS frees what parking releases on its own threads a few seconds later. Nothing is kept. The swing is only a peak, but a low-RAM PC flicking through scenes would see it.
- **Suggested test fix:** before each reading, gc and wait until private memory stops moving (for example, less than 5 MB change over 1 s, up to 10 s). Or warm up with one round of 100 switches first, then measure a second round. Either way, RG-05 would measure what stays allocated, not how fast memory is freed.
- **Seen in passing:** 2 of about 25 runs here sat the full 6-minute timeout after writing results (the exit hang, LOBS-037).

## QA run at HEAD `b60b9e5` (v0.3.2, 2026-10-23, `runs/20261023-155329`)
- Built with the LOBS-037 exit fix (5c2c0f0) applied and the RG-05 test fix in place. **97 pass, 3 fail, no crashes.** Every process exited within 6 s of writing results.
- **PF-04-heavy (LOBS-024):** UI frame interval while dragging p95 **33.5 ms** [≤ 16.7], p99 66.7 [≤ 33]. Moves cost 0.9 ms, scrub steps p95 17.7 ms, and 0 frames lagged. Same as fix pass 2: p50 is exactly one vsync and p95 exactly two, which points at the harness injecting moves on a 16 ms timer against a 16.7 ms vsync.
- **PF-04-stress (LOBS-025):** drag frame interval p95 **50.3 ms** [≤ 33], p99 83 [≤ 66], p50 30.6. Better than 79 ms in fix pass 2 and 114 ms before that. Moves cost 3.6 ms, so most of each frame is QML rendering. Scrub p95 47 ms now passes [≤ 66].
- **CC-01:** not run, because no speech model is on this PC (`LOBS_WHISPER_MODEL` unset). Environment, not a bug. CC-01u (the panel) passes.
- **Stress resources after open:** still **2,006 GDI objects and 237,807 handles**, 4.1 GB private (LOBS-025; unchanged).
- **Passing and better than targets:** stress open 2.5 s, select scene p95 53 ms, Show p95 39 ms, linked edit across 100 scenes p95 65 ms, autosave stall max 56 ms, 181 KB per edit (LOBS-036 cap: 65 steps).
- **Not covered by any story:** the 0.3.x features: the self-updater (ab33024, which also swaps the OBS engine), crash reports (be68555), the splash window, the video panel's tab drag (d993b40), the title-bar top bar, and Audio Mixer reorder and pin (22fc1d5). Their stories were written by the feature commits themselves, or don't exist, so they've had no independent QA.

## LOBS-039 · S3 · Each layer click keeps twice as much memory since the filter catalog (70256d3)
- **Fix pass 3:** Fixed in be63a42 · verified in `runs/20261023-165825` (RG-05 passes) and `runs/20261023-170101` (FX-01…06u).
  - **Cause (measured, not the leads below):** the leak was in the UI. With `t.blockAppSignals` on, 100 clicks kept nothing (`_probe-clicks.js`); with the Inspector unloaded, nothing either. One of the 9 clicked layers shows 10 filters through a link. Each visit built 10 filter cards with their controls, and each move away destroyed them, keeping about 5 MB per round trip. This is older than 70256d3 (about 0.35 MB/click on bd7182d), and 70256d3 made each card bigger: Color Correction went from 5 settings to 8, two of them color fields. `t.churn` shows `FColorField` keeping about 28 KB per create/destroy (its picker `Popup` was built with the field). The other controls keep about 2 KB each in a real card, but much more in the live Inspector.
  - **Fix:** filter cards and their setting rows are kept once built. A layer with fewer filters hides the extra ones, which keep their last filter so their controls aren't rebuilt (`Main.qml`, Filters section). `FColorField` builds its picker the first time it opens.
  - **Result (`_probe-clicks.js`, settled MB per 100 clicks):** +97/+63/+72 before → **+33 once (building the cards), then +0/−1/+5**. RG-05 clicks pass. New UI checks: FX-05u (a reused card shows the new layer's filter and values) and FX-06u (the picker opens, applies a typed hex and opens again).
- **Story:** RG-05 (click check) · **Status:** Fixed (above) · was: Confirmed, 5 of 5 runs on `main` 3ca3966, regression from 70256d3 ("Layer filters match OBS's")
- **Measured (heavy, 100 `selectLayer` calls on scene 2, settled memory):**

  | Build | MB kept per click | Runs |
  |---|---|---|
  | v0.3.2 b60b9e5 + LOBS-037 | 0.40–0.48 ✅ | `runs/20261023-155329`, `-154700` … `-155109` |
  | `main` 3ca3966 (adds 70256d3) | **0.91–0.97** ❌ (limit 0.5) | `runs/20261023-160612`, `-161523`, `-161620`, `-161712`, `-161806` |

  Switching is unaffected (0.15–0.23 MB/switch), and `25-filters` passes.
- **Leads (from the diff, not profiled; wrong, see the fix above):** the Inspector view is rebuilt on every selection, and 70256d3 added to it:
  1. a `FileDialog` (`filterFilePicker`, [Main.qml](../native/qml/Main.qml)), which is a native dialog helper per Inspector build;
  2. `options: app.filterTypes()`, a fresh list for the "Add filter…" select each time;
  3. one `Loader` per filter setting, now up to about 10 for Chroma Key and Color Correction (was 3–4), plus `FColorField`/`FSelect` controls with popups.

  Heavy has 200 filters across its layers, so most clicks build several filter cards.
- **Suggested fix:** one `FileDialog` for the window, outside the Inspector component. Read `filterTypes()` once (it's a static catalog). Make sure filter-card popups are created on open, not with the card. Confirm with a heap profile of 100 clicks before and after.

## LOBS-040 · S2 · LOBS can crash when OBS threads log at the same time
- **Area:** stability / logging · **Found:** fix pass 3, a full 63-perf-stress-b run died in PF-03-stress (`runs/20261023-171134`) · **Story:** LG-01 (`80-misc.js`) · **Status:** Fixed in 8f840e0 · verified in 5 of 5 LG-01 runs (32 threads × 20,000 lines)
- **Crash (minidump `lobs-20261023-171143.dmp`):** an OBS task-queue thread → `blog` → LOBS's `logHandler` → `QTextStream(QIODevice*)` → `QObject::disconnect` → access violation in Qt's connection list.
- **Cause:** `logHandler` ([main.cpp](../native/src/main.cpp)) runs on whichever thread libobs logs from: audio, video, task queues. It built a `QTextStream` on the shared `QFile` for every line, with no lock. Each stream connects to the file's `aboutToClose` when built and disconnects when dropped, and the file's writes aren't thread-safe either. Two threads logging at once corrupt the heap.
- **Repro:** `t.logStorm(32, 20000)` (a test hook that calls `blog` from 32 threads) killed LOBS in 1 of 3 runs before the fix, with exit 0xC0000374 (heap corruption, so no crash report). A heavy show makes this more likely: the crash run logged thousands of filter removals while parking scenes.
- **Fix:** each line is built in a local buffer and written in one `write` under a mutex. Teardown clears `g_log` under the same lock. After: 5 of 5 runs pass, and all 640,000 storm lines are in the log whole, none interleaved.

## LOBS-041 · S3 (QA harness) · After one crashed self-test, every later one opens the crash prompt over its window
- **Area:** test hygiene / crash reports (be68555) · **Found:** fix pass 3 · **Story:** H-07 (`00-harness.js`) · **Status:** Fixed in 3c490b3 · verified in a run that started with an unseen dump (00-harness + 40-ui, 20/20 pass)
- **Symptom:** after the LOBS-040 crash at 17:11, 12 of 14 `40-ui` stories failed in every run: clicks, shortcuts and menus did nothing. Code from before the crash failed the same way.
- **Cause:** self-tests keep their own crash folder (`%LOCALAPPDATA%\qttest\LOBS\LOBS\crashes`). Any dump newer than `seen.txt` makes `crashReport.pending` true, and 900 ms after startup Main.qml opens the "Report on GitHub" dialog. No test dismisses it, so the modal stays over the window for the whole run, and for every run after.
- **Fix:** in a self-test, `main.cpp` marks a pending crash as seen instead of offering it. The dump stays in the folder, and `run.ps1` already reports crashes (exit code, `Crashed`).

## LOBS-042 · S2 · A hidden layer with a plugin filter can crash LOBS (seen with the Spout2 output filter)
- **Area:** plugins / filter parking · **Found:** Spout2 plugin check, 2026-10-27 (`runs/20261027-145622`) · **Story:** SP-03 (`scenarios/_probe-spout.js`) · **Status:** Fixed in the commit that adds this entry · verified: SP-04 and SP-05 pass 5 of 5, and 25-filters, 95-plugins and 20-scenes-layers pass (except LY-06's toast wording check, which isn't related)
- **Crash:** `0xC0000005` in `obs.dll` (offset `0x64aa4`). Minidump and log are `%LOCALAPPDATA%\qttest\LOBS\LOBS\crashes\lobs-20261027-145632.dmp` / `.log`.
- **Setup:** win-spout 1.13.0 (Off World Live). An image layer with a Spout Filter (`win_spout_filter`) sending as "LOBSAlpha", a Spout2 Capture receiving it, then the image layer hidden and the capture's composite mode changed 1 → 2 → 3 → 4. It died during the switch to 4.
- **What the log shows:** after the layer was hidden, `parkHiddenFilters` ([engine.cpp:813](../native/src/engine.cpp:813)) removed the Spout filter (`filter ... removed from source`). The plugin's destroy then freed GPU textures outside the graphics context (`gs_texture_destroy: called while not in a graphics context` ×3). About 140 ms later `apply()` re-created the filter on the same instance while it was still hidden, and LOBS crashed soon after.
- **Crash stack (minidump):** OBS's graphics thread → a win-spout callback → `obs_source_enabled` on a garbage pointer (`rcx=1`): the plugin's per-frame code ran on a filter context that had already been freed.
- **Cause:** parking destroys and re-creates a plugin filter, which OBS never does to a filter on a hidden source. A plugin with GPU state (win-spout keeps a texrender and a DX11 sender) gets torn down outside the graphics context and rebuilt right away, while its receiver is mid-reset. The "not in a graphics context" warnings show on every teardown, in passing runs too: win-spout leaks those textures whenever its filter is destroyed this way.
- **Also:** parking breaks win-spout 1.13's "continuous broadcast" (keeps sending while off-screen). In LOBS a hidden layer stops sending, because its filter no longer exists.
- **Does not affect** the VNyan → LOBS path: that uses only Spout2 Capture, which SP-02 and SP-03 pass consistently.
- **Churn, the trigger:** `apply()` unparked a parked layer whenever its scene was wanted, even when the layer was hidden and unchanged ([engine.cpp:506](../native/src/engine.cpp:506)). So every edit anywhere in the scene re-made a hidden layer's filters, and the park timer then dropped them again. SP-04 shows it on the old build: the first edit takes `parked` from 3 to 1 and `filters` from 0 to 2.
- **Fix:** (1) A parked layer stays parked while it's hidden and unchanged. (2) `parkHiddenFilters` never parks a layer with a plugin (`obs:`) filter, because OBS never drops a hidden source's filters, and plugins can work off-screen or hold GPU state. This also brings back win-spout's continuous broadcast for hidden layers. Built-in filters are parked as before (LY-07 and LY-08 pass).
