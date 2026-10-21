# LOBS User Story Catalog (Phase 1, for review)

Spec sources: README.md (shipped claims) and PLAN.md §2–3 (intended model). Stories checked
against the code as it stands at `a51310e`.

**Personas:** **N** new streamer · **S** OBS switcher (power user) · **L** live operator (mid-show)
**Methods:** **AC** auto, controller-level (`--scenario`) · **AU** auto, UI-level (screenshot-driven) · **M** manual walkthrough (you)
**Principles:** H1–H10 = Nielsen heuristics (H1 visibility of status, H2 match real world,
H3 user control/undo, H4 consistency, H5 error prevention, H6 recognition over recall,
H7 flexibility, H8 minimalism, H9 error recovery, H10 help) · Norman (affordance, signifier,
feedback, mapping, constraint) · Fitts · Hick · Gestalt · WCAG

Every P0 story gets an AC test **plus** at least one AU check proving the UI reaches it.

---

## 0. Spec drift found while writing stories

The code doesn't match PLAN.md in these places. Each needs a call (see *Review questions*):

| # | PLAN.md says | Code has |
|---|---|---|
| D1 | Link groups: Transform · Crop · Filters · **Audio** · Visibility | 4 groups; audio lives on the **source**, so it's always shared across scenes ([doc.h:39](../native/src/model/doc.h:39)) |
| D2 | Paste prompts **Linked / Independent** with a **remembered default** | "Copy to ▸ scene ▸ Linked copy / Independent copy" menu; no remembered default |
| D3 | Sources panel: "in use by N scenes" count, unused-source warning, "select all instances", bins/folders | Not yet verified. `Source.bin` exists; UI to confirm |
| D4 | Audio **signal-flow view** (source → filters → bus → output) | Mixer exists ([MixerView.qml](../native/qml/MixerView.qml)); a flow view exists only in the web prototype |
| D5 | Echo warning when monitor and output would loop | Not yet verified |
| D6 | Relink "asks which side wins" | Present: "Keep this scene's values" / "Use X's values" ([FSection.qml:78](../native/qml/FSection.qml:78)) ✓ |

---

## 1. Sources

**MP-01 · P0 · N · AC+AU** · Add a source once, use it everywhere
As a new streamer I want to add my webcam once and place it in several scenes so I don't set it up repeatedly.
- G: empty show, 3 scenes · W: add Camera source, place it in all 3 · T: Sources has 1 source; 3 instances; libobs has **one** capture input (engine source count); no "device in use" error.
- Edge: place the same source twice in one scene → 2 instances, still 1 capture.
- Principles: H2 (matches the Resolve/Premiere media-pool model), Gestalt (one item in Sources ↔ many uses)

**MP-02 · P0 · N · AU+M** · Drag from Sources to canvas
- W: drag a Sources item onto the preview, drop at a point · T: the instance lands centered at the drop point; a drop indicator shows during the drag; Esc/out-of-bounds cancels with no layer added.
- Principles: Norman feedback, affordance (drag must look possible), Fitts (drop target size)

**MP-03 · P1 · N · AC** · Rename and delete a source
- W: rename → every instance's displayed name updates (unless the layer was renamed separately). Delete a source used in 3 scenes → **warns with the usage count** before deleting; undo restores the source and all 3 instances with their props and links.
- Principles: H5 error prevention, H3 undo

**MP-04 · P1 · S · AU** · See where a source is used (D3)
- T: Sources shows a use count; an unused source is flagged; the user can jump to or select all instances.
- Principles: H1 visibility, H6 recognition

**MP-05 · P1 · N · AC** · Device status
- G: camera source; W: unplug/disable the device (or pick a missing device ID) · T: the source shows a problem state within ~1 s (status timer); re-enabling recovers without restarting the app.
- Principles: H1, H9

**MP-06 · P2 · S · AC** · Every source kind can be created
- W: for each of `app.kinds`, addSource + place · T: no crash; an instance with non-zero size (video kinds); audio kinds appear in the mixer.

**MP-07 · P1 · N · AC+AU** · Source settings take effect live
- W: change a source setting (color, text, browser URL, file) · T: every instance updates; one undo step per committed change.

## 2. Scenes & instances

**SC-01 · P0 · N · AC+AU** · Create, rename, reorder, delete scenes
- Edge: delete the scene that's currently in Program → defined behavior (falls back to another scene, no black output or crash); delete the last scene → prevented or creates an empty one; duplicate names are allowed or uniquified consistently.
- Principles: H5, H4

**SC-02 · P0 · L · AC+AU** · Edit one scene while another is live
- G: Scene A in Program · W: click Scene B (edit only) · T: Program still shows A; the preview shows B; the UI clearly marks which is live vs edited. Show/double-click → B goes to Program via the current transition.
- Principles: H1 visibility of status (the main live-safety signal), Norman mapping

**SC-03 · P0 · S · AC** · Duplicate scene: independent vs linked
- W: "Duplicate" → edit a layer's transform in the copy → the original is unchanged. "Duplicate with linked layers" → edit transform/crop/filters/visibility in the copy → the original changes for each linked group.
- Edge: duplicate a scene with nested groups; duplicate twice (3 scenes share links).

**SC-04 · P1 · L · AU** · Scene grid with live thumbnails
- T: the grid shows every scene with a thumbnail that updates; Program and edit markers are visible; it scrolls smoothly with 200 scenes (PF-04).
- Principles: H6, Hick (grid vs long list)

**SC-05 · P2 · S · AC** · Scene folders/favorites (if exposed in UI)
- T: `Scene.folder`/`favorite` round-trip and are reachable in the UI; otherwise logged as a D3-style spec gap.

## 3. Linking (deepest coverage)

**LK-01 · P0 · S · AC+AU** · Linked copy shares all groups
- W: select a layer, Copy to ▸ Scene B ▸ Linked copy; then in B change transform, crop, add a filter, toggle visibility · T: A reflects every change; both show a 🔗 badge in Layers and a "Linked · Scene A/B" chip on each inspector section.
- Principles: Gestalt (shared badge/color), H1

**LK-02 · P0 · S · AC** · Independent copy shares nothing
- Same as LK-01 with Independent copy → A unchanged, no link badges. The capture is still one input (MP-01).

**LK-03 · P0 · S · AC+AU** · Per-group unlink
- G: linked pair · W: in B, "Unlink in this scene" on **Transform only**; then move B and add a filter in B · T: the move doesn't affect A; the filter **does** appear in A; badges/chips update per section (Transform chip gone, others remain).
- Principles: Norman mapping (control sits on the group it affects), H1

**LK-04 · P0 · S · AC+AU** · Relink: which side wins
- G: an unlinked pair with different transforms · W: link Transform with "Keep this scene's values" → both take B's value; repeat with "Use A's values" → both take A's.
- Edge: relink when A was deleted meanwhile → option absent/disabled, no crash.
- Principles: H5 (destructive choice made explicit), H2 wording

**LK-05 · P0 · S · AC** · Link fan-out across N scenes
- G: one layer linked into 10 scenes · W: change crop in scene 7 · T: all 10 update; the chip reads "Linked · 9 scenes"; unlinking scene 7 leaves the other 9 linked to each other.
- Edge: unlink every member one at a time → the final member has no stale "Linked" chip (link GC).

**LK-06 · P0 · S · AC** · Undo/redo across linked edits
- W: linked edit in B → undo → both A and B revert; redo → both reapply. Unlink → undo → relinked with the original shared value. The History panel shows meaningful labels ("Link Transform with Scene A").
- Principles: H3

**LK-07 · P0 · S · AC** · Save → reload preserves links
- W: build a mixed link graph (full, partial, 3-way, groups), save, reopen · T: identical link topology and values; byte-stable after a second save (idempotent serialization).

**LK-08 · P0 · S · AC** · Deleting things that are linked
- (a) delete one linked instance → peers keep the value and stay linked to each other; (b) delete a scene holding a link member → peers unaffected; (c) delete the **source** → all instances removed, links GC'd, no dangling link IDs in the saved JSON. Undo of each restores the exact prior graph.

**LK-09 · P1 · S · AC** · Linked layers inside groups
- G: a group containing a linked camera layer · W: copy the group Linked to Scene B; move the group in B; change the child's crop in B · T: define and verify: the group transform links if the group is linked; the child's crop propagates. Nested 3 deep behaves the same.
- Edge: ungroup in B → child links survive.

**LK-10 · P1 · S · AC** · Filters on linked vs independent instances
- T: a filter added on a linked instance shows on both; on an independent copy, filter chains diverge; reordering/toggling a filter in a linked chain propagates; per-filter slider scrub = one undo step on release.

**LK-11 · P1 · N · M** · Is linking understandable? (UX study)
- Task-based walkthrough for you, with no help text: "Make the camera the same size in both scenes, but blur it only in Gameplay." Record time, wrong turns, and whether the "Linked" chip/badge was noticed before or after the mistake.
- Principles: H1, H6, H10, Norman signifiers, Gestalt similarity

**LK-12 · P1 · S · AC** · Reset group
- W: "Reset" on a section (resetGroup) on a linked instance · T: defined behavior: resets all peers (it's shared) **and** says so, or unlinks first. Verify which, and judge against H5.

**LK-13 · P2 · S · AC** · Audio linking (D1)
- T: verify that volume/mute/filters on a source apply in every scene (by design). Judge whether a user can get per-scene audio levels at all; if not, log as a spec gap vs PLAN.

## 4. Layers & groups

**LY-01 · P0 · N · AC+AU** · Reorder: drag and Ctrl+] / Ctrl+[
- T: render order matches the list (top draws on top; check with a snapshot pixel test using overlapping colors); a big delta goes to front/back; one undo step per move.

**LY-02 · P0 · N · AC+AU** · Group / ungroup (Ctrl+G, Ctrl+Shift+G)
- T: the group transforms as a unit (moving or scaling the group moves its children proportionally); ungroup keeps on-screen positions exactly (snapshot before = after).
- Edge: group layers from different nesting levels; group a single layer; drop a group into itself (must be refused).

**LY-03 · P1 · N · AU** · Double-click steps into groups
- T: the first click selects the group, a double-click selects the child; Esc/click-out steps back out. Breadcrumb or other indicator of the current group depth.
- Principles: H1, Norman mapping (matches Figma/Affinity)

**LY-04 · P1 · L · AC** · Lock / hide
- T: a locked layer can't be moved on the canvas (click passes through or selects without dragging); hidden layers don't render in Program; both states show in Layers.

**LY-05 · P1 · N · AC** · Duplicate (Ctrl+D), delete, multi-select
- T: duplicate offsets or stays in place consistently; multi-select delete is one undo step; shift/ctrl click semantics match Windows conventions (H4).

**LY-06 · P0 · L · AC+AU** · Live lock (PLAN §4b)
- G: streaming or recording (live) · W: drag a layer · T: the move is refused without unlock/modifier, with clear feedback **why**; after unlock, moves work; undo still works.
- Principles: H5, Norman constraint + feedback

## 5. Canvas, tools, crop & corners

**CV-01 · P0 · N · AU** · Move/resize with handles, snapping guides
- T: corner handles resize, Shift keeps aspect (or the reverse, whichever matches the convention), guides appear on center/edges, arrow keys nudge 1 px / Shift 10 px.
- Principles: Fitts (handle hit area at 4K + DPI), Norman feedback

**CV-02 · P0 · N · AC+AU** · Crop by handles and aspect presets
- W: crop mode → drag edge handles; then presets 16:9, 4:3, 1:1, 9:16 · T: crop values match; the preset crop is centered; crop is per-instance (or per link group). Snapshot confirms the pixels.
- Edge: crop past the opposite edge → clamped, never negative size.

**CV-03 · P1 · N · AC+AU** · Rounded corners up to circle
- T: 50% on a square = circle (snapshot: corner pixels transparent, edge midpoints opaque); works with crop and opacity; linked via the Crop group.

**CV-04 · P1 · N · AU** · Graphic tools: text, rect, ellipse, arrow, pencil
- T: each makes a layer in the edited scene, using the current fill/stroke/width; shapes stay sharp when scaled 4× (snapshot edge sharpness); text editing on canvas supports typing, backspace, caret, IME basic.

**CV-05 · P1 · L · AU+M** · Laser pointer shows on stream, never added to the scene
- T: the laser trail appears in Program/recording (check the recording frames); layer count unchanged; the trail fades in the configured ms.

**CV-06 · P1 · N · AC** · Numeric fields with math
- T: `1920/2` → 960; invalid input reverts with no corrupt value; scrub = one undo step.
- Principles: H5, H7

**CV-07 · P2 · N · AU** · Eyedropper
- T: picks screen color into the fill; Esc cancels; preview isn't frozen afterwards.

**CV-08 · P1 · N · AU** · Preview behind menus/dialogs
- T: opening any menu/dialog/tooltip over the preview doesn't leave a stale frozen frame or a hidden preview after closing (the native preview sits above QML: high bug risk). Hammer test: open/close 200 popups quickly, then confirm the preview is live.

## 6. Transitions & Program

**TR-01 · P0 · L · AC+AU** · Each transition type works
- For each of fade, cut, swipe, slide, fade-to-color, luma wipe, stinger: add, set duration/options, Show a scene · T: Program ends on the target scene; for timed transitions, the duration is within ±1 frame (capture during a recording); no black flash on cut.

**TR-02 · P1 · L · AU** · Live transition preview
- T: preview plays in the dialog without affecting Program.

**TR-03 · P1 · L · AC** · Rapid scene switching
- W: Show 50 times in 10 s, including mid-transition · T: no crash, Program ends on the last requested scene, no leaked transition sources.

**TR-04 · P2 · S · AC** · Stinger with missing file
- T: a clear error/fallback, never a hang.

## 7. Undo history

**UH-01 · P0 · N · AC+AU** · Every edit is undoable with a readable label
- T: for each controller edit type, undo restores exactly (doc JSON equality) and the label describes it; scrubs and drags = one step.

**UH-02 · P1 · N · AC** · Jump in history panel
- T: jumping back 20 steps and forward again restores exact states; a new edit after jumping back drops the redo branch (and the panel shows that).

**UH-03 · P1 · S · AC** · History depth & memory
- W: 1,000 edits · T: all undoable (or there's a documented cap); memory grows bounded (PF-06).

## 8. Output: record, replay, virtual cam, stream

**OUT-01 · P0 · N · AC** · Record start/stop
- T: valid file (ffprobe: expected resolution, fps, codecs, duration ±1 frame), status shows recording time, "show recordings" opens the folder.
- Edge: output folder missing/read-only → clear error, no silent failure; disk nearly full → handled.

**OUT-02 · P1 · L · AC** · Replay buffer
- W: start replay, wait 30 s, save · T: a file of the configured length is saved; toast shows the filename; works while recording.

**OUT-03 · P1 · N · AC+M** · Virtual camera
- T: start → another app (e.g. a browser camera test) sees LOBS output; stop releases it. If `vcamAvailable` is false, the UI explains why.

**OUT-04 · P0 · L · AC** · Streaming to a local RTMP sink (never a live service)
- T: connects, stream stats update (bitrate, dropped frames), stop is clean; a wrong key/URL gives a clear error; a network drop mid-stream gives reconnect behavior and status.
- Principles: H1, H9

**OUT-05 · P1 · N · AC** · Settings round-trip
- T: encoder, resolution, fps, bitrate, recording path/format save, survive restart, and take effect on the next output.

## 9. Setup assistant & first run

**SA-01 · P0 · N · M+AU** · First run on a clean profile
- T: a new user is guided to a working scene with no dead ends; skip is possible; the wizard doesn't reappear after finishing; sample show loads.
- Principles: H10, H8, Hick

**SA-02 · P1 · N · AC** · Server ping and recommendations
- T: servers sorted fastest-first; recommendSettings returns sane values for 1/3/6/10/50 Mbps (bitrate ≤ ~75% of upload, resolution/fps step down sensibly).
- Bandwidth test against Twitch: **needs your OK** (see Review questions).

**SA-03 · P2 · N · M** · Stream key help
- T: the "where to get your key" link opens the correct page per service.

## 10. OBS import

**IM-01 · P0 · S · AC** · Import a real OBS collection
- G: synthetic OBS collections (scenes, nested scenes, groups, filters, transforms, crops, hidden items, audio settings) · T: counts match; transforms/crops within 1 px; filters mapped or reported as unsupported; **one** source in Sources per OBS source even when used in many scenes.
- Uses `--importtest`. Also the collections at `%APPDATA%\obs-studio` if present (read-only).

**IM-02 · P1 · S · AC** · Import robustness
- T: malformed/old-format/huge JSON → error message, no crash, current show untouched.

## 11. Projects & persistence

**PJ-01 · P0 · N · AC** · New / Save As / Open project
- T: a folder with `<name>.lobs.json` + assets; open restores everything; checkNewProject rejects invalid names (reserved names like `CON`, illegal chars, existing folder).

**PJ-02 · P0 · N · AC** · Autosave and crash recovery (LOBS-002)
- W: edit, kill the process at random points during saves (stress doc, 200 iterations) · T: the doc always reloads to a valid recent state.

**PJ-03 · P1 · N · AC** · Asset library
- T: import, folders, rename, cut/copy/paste, move, trash, folder color, search across folders; "in use" protection when trashing an asset a layer uses; missing-asset detection + relink.

**PJ-04 · P0 · S · AC** · Startup mode detection (LOBS-001 regression)
- T: install paths containing "test", "Test", "latest"; doc path args → normal mode.

## 12. Docking & layout

**DK-01 · P1 · S · AU+M** · Rearrange and persist panels
- W: float, dock, tab, resize panels; restart · T: the layout restores exactly; a "reset layout" exists and works.
- Single monitor: a floating panel restores on-screen even if it was saved off-screen (simulate by editing saved geometry).

**DK-02 · BLOCKED (hardware) · S** · Multi-monitor layout restore and fullscreen projector on monitor 2.

**DK-03 · P2 · N · AU** · Minimum window size and 4K/DPI scaling
- T: at 125/150/200% scaling and a small window, no clipped or overlapping controls; the preview keeps priority.

## 13. Audio

**AU-01 · P0 · N · AC+M** · Mic shows level and records
- T: Scarlett mic meter moves when speaking; the recording contains audio at the expected level; mute silences both the meter path and the output.

**AU-02 · P1 · N · AC** · Audio effects chain
- T: add/remove/reorder/enable/disable each `audioFxTypes`; values scrub with one undo step; order affects processing.

**AU-03 · P1 · N · AC+M** · Monitoring and echo warning (D5)
- T: monitor off / monitor only / monitor+output behave correctly; a warning appears where echo is possible.

**AU-04 · P2 · S · AC** · Tracks, balance, mono, sync offset
- T: settings persist and affect the recording (ffprobe track count; channel analysis for mono/balance).

## 14. Hotkeys & projectors

**HK-01 · P1 · L · AC+M** · Global hotkeys
- T: bind record/scene/replay hotkeys; they fire while LOBS isn't focused; conflicts are detected or warned; bindings persist.

**HK-02 · P1 · L · AU** · Windowed projector (program and scene)
- T: opens, shows correct content, closes cleanly, closes on app exit (closeProjectors).

## 15. Twitch panels

**TW-01 · P2 · L · M** · Chat and stream info panels
- Needs **you** to sign in (I never enter passwords). T: chat loads and updates; stream title/category edits apply; signed-out state is clear.

## 16. Accessibility & UX (cross-cutting)

**AX-01 · P1 · all · AC** · Screen-reader names
- T: UIA tree exposes named, role-correct controls. **Expected fail:** 0 `Accessible.name` uses in QML (pre-logged as a likely finding).

**AX-02 · P1 · all · AU** · Keyboard-only operation
- T: core flow (add source, place, switch scene, record) possible with keyboard only; focus is always visible.

**AX-03 · P1 · all · AC** · Contrast
- T: all text/icon colors in the `Pal` palette meet WCAG AA against their backgrounds (computed from QML values).

**UX-01 · P1 · all · M** · Heuristic review of every panel
- Structured review of each panel against H1–H10, producing UX findings, not pass/fail.

## 17. Performance & endurance (targets in TEST_PLAN.md)

| ID | Pri | Method | What |
|---|---|---|---|
| PF-01 | P0 | AC | Open/save heavy and stress projects |
| PF-02 | P0 | AC | Scene switch latency, 100 switches, p50/p95/p99 |
| PF-03 | P0 | AC | Linked-edit propagation, source linked into 30 / 100 scenes |
| PF-04 | P0 | AU | UI frame time during canvas drag, inspector scrub, Layers/grid scroll (uses the existing `m_perf*` drag timing + external frame sampling) |
| PF-05 | P1 | AC | Sources search per keystroke, selection→inspector at 2,000 instances |
| PF-06 | P1 | AC | 1,000-step undo/redo; memory growth |
| PF-07 | P0 | AC | 1080p60 render, 0 lagged/skipped frames, heavy project with webcam + filters |
| PF-08 | P0 | AC | 60-min record + replay buffer endurance: memory, handles, GDI/USER objects, frame time drift |
| PF-09 | P1 | AC | Autosave hitch while dragging in stress project |
| PF-10 | P1 | AC | Cold start time; startup with stress project as current doc |

---

## Summary

| Area | Stories | P0 |
|---|---|---|
| Sources | 7 | 2 |
| Scenes | 5 | 3 |
| Linking | 13 | 8 |
| Layers | 6 | 3 |
| Canvas/tools | 8 | 2 |
| Transitions | 4 | 1 |
| Undo | 3 | 1 |
| Output | 5 | 2 |
| Setup | 3 | 1 |
| Import | 2 | 1 |
| Projects | 4 | 3 |
| Docking | 3 (1 blocked) | 0 |
| Audio | 4 | 1 |
| Hotkeys/projectors | 2 | 0 |
| Twitch | 1 | 0 |
| Accessibility/UX | 5 | 0 |
| Performance | 10 | 6 |
| **Total** | **85** | **34** |

Pre-logged expectations (to confirm or refute in testing, not yet bugs): AX-01 likely fails;
D1/D2/D4 are likely spec gaps; CV-08 and LK-12 are high-risk areas.

---

## Review decisions (2026-10-20)

1. PLAN.md items that aren't built are **spec gaps**. They're listed separately and don't count against release readiness.
2. Twitch bandwidth test: **approved** (short test stream; respect Twitch TOS; clean test content only).
3. Twitch panels: manual. I'll ask when it's time to sign in.
4. OBS import: real collections (copies, read-only) in addition to synthetic ones.
5. Prioritization left to QA.
6. Terminology: the concept is **Sources** everywhere, never "Media Pool" (LOBS-004).
