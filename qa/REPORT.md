# LOBS v0.1.1 QA Report

**Build:** `a51310e` (v0.1.1) · OBS engine 32.2.2 · **Date:** 2026-10-20
**Machine:** i9-13900KF · RTX 3080 Ti · 64 GB · one 4K monitor @ 59 Hz, 175% scaling · Windows 11 26300 · EOS Webcam Utility (virtual camera driver) · Scarlett Solo mic
**Scope:** the native app (`native/`), measured against README.md (shipped claims), PLAN.md (intended model) and the targets in [TEST_PLAN.md](TEST_PLAN.md). Bugs are reported, not fixed.

---

## 1. Summary

**Verdict: not release-ready for anything beyond small shows.** Correctness is better than expected: linking, undo, transitions, recording, replay, the virtual camera and streaming all work as written. **Performance and memory do not hold up.** Every UI-visible change leaks memory and costs time in proportion to the whole project. On a realistic heavy show, the editor drops to about 8 fps and scene switching degrades to multi-second freezes until the app stops responding. Several persistence paths can also silently lose work.

| Severity | Count | Bugs |
|---|---|---|
| **S1** crash / data loss / unusable | **3** | LOBS-001, LOBS-022, LOBS-025 |
| **S2** major function broken | **8** | LOBS-006, 007, 008, 009, 021, 023, 024, 026 |
| **S3** minor / workaround exists | **15** | LOBS-002, 005, 010, 011, 012, 013, 014, 015, 016, 018, 019, 020, 027, 028, 029 |
| **S4** cosmetic | **4** | LOBS-003, 004, 017, 030 |
| **Total** | **30** | Full detail with repro steps and evidence: [BUGS.md](BUGS.md) |

**Automated stories:** 62 run → **38 pass · 23 fail · 1 died** (the stress scene-switch run hung). Plus LOBS's own `--featuretest`: 7/7 pass.
**Manual stories:** 11 walkthroughs written ([MANUAL.md](MANUAL.md)), not yet done.

### Top risks, in order
1. **Memory leak on every change (LOBS-022, S1).** About 3 MB per scene switch on a tiny project and 17–87 MB on a heavy one. Clicks and edits leak about 10 MB each. A full run reached 5.4 GB and Windows marked LOBS "Not Responding". A streamer switching scenes through a show will hit this.
2. **The editor slows down with project size (LOBS-023, 024, 025).** Heavy project: clicking a layer takes 230–411 ms, dragging runs at about 8 fps, and a scene switch costs 310 ms (1.3–1.5 s for scenes with display capture). Stress project: a 13 s open, 6 s autosave freezes, and 15 scene switches didn't finish in 6 minutes. **The broadcast output itself stays clean:** 0 lagged frames.
3. **Silent data loss in persistence (LOBS-001, 007, 008, 009).** A "test" anywhere in the install path makes every launch throw away the show. Open drops the current project's unsaved edits. A failed New Project wipes the open show. A failed Save As breaks every later Save.
4. **Live-safety gaps (LOBS-006, 026).** Live lock doesn't cover the Inspector or Delete, and importing an OBS collection mid-stream switches what's on air.
5. **Accessibility (LOBS-021).** 22 of 201 interactive controls have an accessible name.

### What works well
- **Linking (the core idea):** linked and independent copies, per-group unlink, relink with "keep mine / use theirs", fan-out to 10 scenes, undo/redo across linked edits, save/reopen (byte-identical on a second save), and deleting linked things with exact undo. 10 of 12 linking stories pass. The 2 failures are groups and the Reset button.
- **One device, one capture:** the webcam placed in 5 scenes runs as exactly one libobs input.
- **Output:** mp4/mkv/mov recordings are valid 1080p60 H.264+AAC with the right durations. The replay buffer saves exactly N seconds. The virtual camera works. Streaming to RTMP works. A connection drop shows "reconnecting" within 1 s, and a bad server gives a clear error.
- **Transitions:** all 7 types land on the right scene in both directions, and 50 rapid switches mid-transition end on the right scene with no leaks.
- **Undo:** 16 edit types undo to the exact prior JSON, history jumps are exact, and scrubs are one step.
- **Import:** the OLD collection is perfect. In Untitled, the scenes that came over match OBS exactly, layer for layer and hidden for hidden. Broken files never touch the open show.
- **Robustness:** junk files are refused on Open, autosave survived 15 forced kills, an unwritable recording folder gives a clear error, and in-use assets can't be trashed.

---

## 2. Bugs (one line each; full entries in [BUGS.md](BUGS.md))

| ID | Sev | Area | Summary |
|---|---|---|---|
| LOBS-001 | S1 | Startup | Any "test" in the install path or arguments → self-test mode: the show is deleted at every launch, silently |
| LOBS-022 | S1 | Performance | Memory leaks on every scene switch, click and edit; long sessions end in "Not Responding" |
| LOBS-025 | S1 | Scale | Stress project (200 scenes / 3,270 layers) unusable: 13 s open, 6 s freezes, switching hangs, 271k handles, GDI at 20% of the OS limit |
| LOBS-006 | S2 | Live safety | Live lock bypassed by Inspector edits and Delete on the on-air scene |
| LOBS-007 | S2 | Persistence | Open drops the current project's unsaved edits from disk, with no prompt |
| LOBS-008 | S2 | Persistence | A failed New Project still replaces the open show and leaves a broken project path |
| LOBS-009 | S2 | Persistence | A failed Save As repoints the project at the bad path; later Saves fail |
| LOBS-021 | S2 | Accessibility | 22 of 201 interactive controls have an accessible name (WCAG 4.1.2) |
| LOBS-023 | S2 | Performance | Scene switching: p50 2.6 s / max 6.3 s (heavy, with the leak); 310 ms baseline; capture scenes 1.3–1.5 s |
| LOBS-024 | S2 | Performance | Heavy-project editor: drag ≈ 8 fps, click 230–411 ms, scrub 100–241 ms, linked edit 158–216 ms |
| LOBS-026 | S2 | Import / live | Importing an OBS collection while live puts the imported scene on air |
| LOBS-002 | S3 | Persistence | Autosave is truncate-then-write with no result check (code risk; 0/15 kill reproductions) |
| LOBS-005 | S3 | Status | "Lagging N%" is cumulative since start, so a startup hiccup reads as an ongoing problem |
| LOBS-010 | S3 | Projects | CON/NUL/COM1 and 250-character project names are accepted; "CON" makes a folder Explorer can't open |
| LOBS-011 | S3 | Linking | A linked group's own visibility and opacity don't link, and the group shows no link badge |
| LOBS-012 | S3 | Linking | Reset on a linked section silently resets every linked scene |
| LOBS-013 | S3 | Scenes | Every scene can be deleted (0 scenes, nothing on program) |
| LOBS-014 | S3 | Layers | Grouping layers from different levels silently leaves some out |
| LOBS-015 | S3 | Sources | Renaming a source doesn't rename its layers |
| LOBS-016 | S3 | Persistence | A parseable doc with a dangling layer id opens and shows a blank ghost layer |
| LOBS-018 | S3 | Sources | Delete source removes it from every scene with no warning |
| LOBS-019 | S3 | Persistence | Ctrl+S and toolbar Save always open Save As |
| LOBS-020 | S3 | Linking UX | "Copy to another scene" is only on a small unlabeled header icon, not in any context menu |
| LOBS-027 | S3 | Import | Scenes built with the old LOBS-for-OBS plugin are dropped without being named |
| LOBS-028 | S3 | Setup | A 1 Mbps upload is recommended 1,500 kbps |
| LOBS-029 | S3 | Audio | No echo/feedback warning for a mic on "Monitor and output" |
| LOBS-003 | S4 | Test hygiene | `--autotest` writes its recording into the user's Videos folder |
| LOBS-004 | S4 | Terminology | "Media Pool" is still in PLAN.md and code; the concept is **Sources** everywhere |
| LOBS-017 | S4 | Undo | No-op edits add undo steps |
| LOBS-030 | S4 | Robustness | Opacity outside 0–1 is stored and saved |

Withdrawn after investigation (harness or test errors, not app bugs): Ctrl+] not reordering (the modal setup assistant had focus), the stream drop "not reported" (it is, as "reconnecting" in the Go Live button), and R-02 "ungroup moves children" (it doesn't; the pixels were identical).

---

## 3. Spec gaps (PLAN.md features not built; these don't count against readiness)

| # | PLAN.md | Built |
|---|---|---|
| D1 | Audio as a per-instance link group | Audio lives on the source, so it's always shared; no per-scene levels |
| D2 | Paste prompts Linked / Independent, with a remembered default; copy/paste across scenes | A header-icon menu only (see LOBS-020); no prompt, no default, no Ctrl+C/V across scenes |
| D3 | Sources: bins, usage count, unused warning, "select all instances" | Usage shows in the Inspector (`uses`, `sceneCount`); the Sources panel wasn't checked in the UI pass |
| D4 | Audio signal-flow view | Mixer only (the flow view exists in the web prototype) |
| D5 | Echo warning | Only for desktop audio (see LOBS-029) |
| §3.5 | Groups "copyable/linkable like any instance" | Children link; the group itself doesn't (LOBS-011) |

---

## 4. UX findings (by principle)

| Principle | Where | Finding | Suggested fix |
|---|---|---|---|
| **H6 Recognition over recall** · Norman signifiers | Layers | The headline feature (Linked copy) is reachable only through a 16 px unlabeled icon (LOBS-020) | "Copy to ▸" in both right-click menus; Ctrl+C/V with the Linked/Independent prompt |
| **H5 Error prevention** | Inspector, linked sections | Reset reads as local but resets every linked scene (LOBS-012) | Label it "Reset in N scenes", or offer "Reset here only" |
| **H5 Error prevention** | Sources, Scenes | Delete source and Delete scene are one click, with no usage warning and no last-scene guard (LOBS-018, 013) | Confirm when used in more than one scene; disable on the last scene |
| **H4 Consistency** · Norman feedback | Groups | A linked group moves in sync but doesn't hide or fade in sync, and has no badge (LOBS-011) | Link the group's own props, or show "children linked" |
| **H4 Platform conventions** | Save | Ctrl+S always opens Save As (LOBS-019) | Save in place once the project has a file |
| **H1 Visibility of status** | Status bar | Lag % is cumulative and shows orange long after a startup blip (LOBS-005) | Rolling 10 s window |
| **H1 Visibility of status** | Going live | On a drop, the button shows "LIVE · reconnecting" in small orange text while the status bar still says "Live on rtmp://…" (mixed signals) | One state everywhere: make "Reconnecting…" the primary label, in amber, and update the status bar |
| **H1 / H9** | Import | Dropped scenes aren't named (LOBS-027); 0-scene "imports" replace the show | List what was left out; refuse empty imports |
| **H2 Match the real world** | Recording error | The message tells Windows 11 users about "Windows 10 Ransomware Protection" (inherited from OBS) | Say "Windows Security → Controlled folder access" |
| **H6** | Layers | A duplicated layer gets the identical name ("A" → "A") | "A copy" or "A 2", like scenes do |
| **Mental model** | Sources vs Layers | Color/text/image sources aren't listed in Sources (by design, PLAN line 239), so "Delete source" and source rename for graphics only happen through layers. Two places hold "things in my show" | Fine for v0.1; say so in the Sources empty-state and help text |
| **WCAG 2.2 · 4.1.2** | Everywhere | Icon-only buttons have no names (LOBS-021) | `Accessible.name` defaulting to `tip` |
| **Positive** · Norman feedback | Live | The "Live · layers are locked. Hold Alt to move one." banner, "On air" chips, and the clear "Couldn't connect to the server" message are good, plain-language status | Keep this voice everywhere |
| To verify (CV-08) | Menus over the preview | In one capture the canvas went black instead of showing the frozen frame while a menu was open (`runs/20261020-152029/40-ui/lk01u-menu.png`) | Check by hand (M-04) |

---

## 5. Performance vs targets

**Heavy** (40 scenes, 104 sources, 424 layers, 200 filters, webcam linked into 30 scenes) must **meet** the targets.
**Stress** (200 scenes, 704 sources, 3,270 layers, 500 groups, webcam linked into 100) must **stay usable**.

| Metric | Target (heavy / stress) | Heavy | Stress |
|---|---|---|---|
| Open → interactive | ≤ 3 s / ≤ 10 s | **0.96 s** ✅ | **13.0 s** ❌ |
| Save | ≤ 500 ms / ≤ 2 s | **8 ms** ✅ | **68 ms** ✅ |
| UI stall around autosave | ≤ 50 ms / ≤ 2 s | 51 ms (borderline) | **6.0 s** ❌ |
| Select scene for edit, p95 | ≤ 100 / ≤ 250 ms | **5.0 s** ❌ (310 ms with no leak) | did not finish ❌ |
| Show scene (cut), p95 | ≤ 100 / ≤ 250 ms | **457 ms** ❌ | did not finish ❌ |
| Linked edit, p95 | ≤ 100 / ≤ 250 ms | **216 ms** ❌ | not run (blocked) |
| UI frame interval while dragging, p95 | ≤ 16.7 / ≤ 33 ms | **209 ms** (p50 131) ❌ | not run |
| Click layer → Inspector, p95 | ≤ 100 / ≤ 200 ms | **411 ms** (fresh) ❌ | not run |
| Single undo, p95 | ≤ 100 / ≤ 200 ms | **97 ms** ✅ | not run |
| Memory per edit | < 512 KB | **0.9–9.6 MB** ❌ | — |
| Render at 1080p60 | 0 lagged frames / < 0.5% | **0 lagged** ✅ (also with all scenes active) | not run |
| 60-min endurance | < 5%/hr growth | not run: fails by inference from LOBS-022 | — |
| Never freeze > 1 s | — | **❌ 6.3 s max** | **❌** |

The pattern: **the output path is fast** (render 0.2–0.9 ms/frame, 0 lagged frames). **The editor path is O(whole project) per change and leaks.** From the code: every change, including a click, runs a full `Engine::apply` over all sources, re-registers global hotkeys, and re-evaluates every QML model bound to `docChanged`. The fix direction is in LOBS-022/024.

---

## 6. Story matrix

The latest result per automated story, regenerated with `python -I qa/tools/matrix.py`. `runs/` holds every result, snapshot and recording.

| Story | Result | Notes / bug |
|---|---|---|
| **Linking** LK-01 · 02 · 03 · 04 · 05 · 06 · 07 · 08 · 10, LK-01u, SC-03 | ✅ | |
| LK-09 linked group | ❌ | LOBS-011 |
| LK-12 reset linked | ❌ | LOBS-012 |
| LK-11 linking, cold | ⏳ manual | M-01 |
| **Sources** MP-01 one capture | ✅ | |
| MP-03 rename/delete | ❌ | LOBS-015, 018 |
| MP-02 drag to canvas | ⏳ manual | M-03 |
| **Scenes** SC-02 edit vs program, SC-01u | ✅ | |
| SC-01 delete edge cases | ❌ | LOBS-013 |
| **Layers** LY-01 · 02 · 04 · 05 | ✅ | LY-02 edge case → LOBS-014 |
| LY-06 live lock | ❌ | LOBS-006 |
| **Canvas** CV-02 crop · CV-03 corners | ✅ | |
| CV-06 junk values | ❌ | LOBS-030 |
| CV-01 · 04 · 05 feel/tools/laser | ⏳ manual | M-04, M-05 |
| **Transitions** TR-01 all types · TR-03 rapid | ✅ | |
| **Undo** UH-02 | ✅ | |
| UH-01 | ❌ | LOBS-017 only (all 16 edit types undo exactly) |
| **Output** OUT-01 · 01b · 02 · 03 · 04 · 04b, featuretest 7/7 | ✅ | |
| **Persistence** PJ-02a · 02b · 03 | ✅ | autosave crash test 15/15 valid |
| PJ-01 · 01b · 01c | ❌ | LOBS-007, 008, 009, 010, 016 |
| V-02 save shortcut | ❌ | LOBS-019 |
| PJ-04 "test" in path | ❌ | LOBS-001 |
| **Import** IM-01-OLD · IM-02 | ✅ | |
| IM-01-Untitled | ❌ | LOBS-027 |
| IM-03 import while live | ❌ | LOBS-026 |
| **Setup** UI-00 first run skip | ✅ | |
| SA-02 recommendations | ❌ | LOBS-028 |
| SA-01 first run feel | ⏳ manual | M-02 |
| **Audio** AU-04 (featuretest) | ✅ | |
| AU-02 | ❌ | LOBS-029 |
| AU-01, AU-03 | ⏳ manual | M-06 |
| **Accessibility** AX-01 | ❌ | LOBS-021 |
| AX-02 keyboard only | ⏳ manual | M-11 |
| **Shortcuts** V-01 | ✅ | |
| **Perf** PF-07 render (heavy) · PF-07a | ✅ | |
| PF-01 · 02 · 03 · 04 · 05 · 06 (heavy); PF-01 · 02 (stress) | ❌ | LOBS-022…025 |
| DK-01 · 03 docking/DPI, HK-01 hotkeys, TW-01 Twitch | ⏳ manual | M-08, M-09, M-10 |
| DK-02 multi-monitor | ⛔ blocked | no second monitor |

---

## 7. Coverage gaps (what wasn't tested, and why)

- **The 11 manual walkthroughs are pending.** M-01 (linking, done cold) matters most, because it tests whether the model makes sense, which automation can't judge.
- **Stress PF-03…PF-07 and the 60-minute endurance run (PF-08) weren't run.** They're blocked by LOBS-022/025: the outcome is known to fail and the runs would only hang the app. Rerun after the leak is fixed.
- **The Twitch bandwidth test** (approved) wasn't run: it needs your stream key, and the QA profile is isolated and has none. I never enter keys. Run it from your installed LOBS (M-02). Server pings and recommendations were tested.
- **Hardware:** the only camera is a virtual driver (EOS Webcam Utility), so "one device = one capture" is unverified on a physical UVC webcam. There's no second monitor (DK-02). The machine is high-end, so mid-range hardware will be slower than these numbers.
- **Environment:** your installed LOBS (PID 26812) was running throughout and shared the camera and GPU. Only 175% DPI was exercised. Disk-full and power-loss weren't simulated (LOBS-002 remains a code risk).
- **Partial coverage:**
  - Assets: no cut/copy/paste/move, folder colors or search.
  - Scenes: scene-grid scroll performance (SC-04) and folders/favorites (SC-05) weren't checked.
  - Transitions: the preview dialog (TR-02) wasn't checked.
  - The Sources panel's usage counts in the UI (D3) weren't checked.
  - Media/browser/window/game capture weren't placed in perf projects; graphics stood in, so real capture costs (see LOBS-023's display-capture numbers) are underrepresented.
- **Method limit:** controller-level tests skip QML wiring. P0 flows got UI-level checks (clicks and keys into the real QML windows), but drag-and-drop from Sources, docking and native dialogs need hands (or the manual list).

---

## 8. How to rerun

```bash
qa/build-qa.bat
```
```bash
powershell -File qa/run.ps1
```
```bash
powershell -File qa/run-output.ps1
```
```bash
powershell -File qa/tools/crash_autosave.ps1 -Iterations 15
```
```bash
python -I qa/tools/gen_project.py heavy qa/runs/fixtures
```
```bash
python -I qa/tools/matrix.py
```

- `build-qa.bat` builds the QA variant into `native/dist/LOBS-qa`. The CMake option `LOBS_TESTS` is OFF by default, so the shipped build is unchanged.
- `run.ps1` runs every scenario in `qa/scenarios` (or name some). `run-output.ps1` runs the output scenario with a local ffmpeg RTMP receiver.
- `gen_project.py` takes `heavy` or `stress`.
- Every run uses an isolated settings profile and a throwaway document. Recordings go to the run folder.

**Harness files:**
- [native/src/test_runner.cpp](../native/src/test_runner.cpp): the `--scenariotest` runner. It exposes JavaScript access to the controller, engine, UI input, snapshots and process metrics.
- [qa/scenarios](scenarios): stories as JavaScript.
- [qa/tools](tools): the project generator, crash test and matrix tool.
- Hooks in `main.cpp` and `CMakeLists.txt` are behind `#ifdef LOBS_TESTS` / `option(LOBS_TESTS)`.
