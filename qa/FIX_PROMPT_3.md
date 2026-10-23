# Fix pass 3: prompt for the main agent

QA ran the full suite on v0.3.2 (`b60b9e5`, `runs/20261023-155329`): 97 pass, 3 fail. Read the last two sections of `qa/BUGS.md` first (LOBS-038 and "QA run at HEAD `b60b9e5`"), then the entries named below. The ground rules from `qa/FIX_PROMPT.md` still apply:
- prove each fix with the named story;
- don't weaken tests (a real test fix goes in its own `qa: test fix` commit);
- one bug per commit;
- update BUGS.md statuses;
- don't push or open a PR.

QA's probes in `qa/scenarios/90-regression.js` stay as they are. If you think one is wrong, say why.

**Before you start:** work from `main`. It has LOBS-037 (the RTWQ exit fix), the RG-05 test fix and the new layer filters (`70256d3`, `25-filters.js`), so start from its latest full run rather than `20261023-155329`.

## 1. Drag frame time on the heavy show (LOBS-024, PF-04-heavy)

- **Measured:** frame interval while dragging: p50 16.75 ms, p95 33.5 ms [≤ 16.7], p99 66.7 ms [≤ 33]. Moves cost 0.9 ms, and 0 frames lagged.
- **QA's reading:** p50 is exactly one vsync and p95 exactly two. The harness injects moves on a 16 ms timer against a 16.7 ms vsync, so some frames get two moves and some none. That may be all the check is measuring.
- **Do first:** make the harness drive drags once per frame, from `frameSwapped` (or the window's `afterAnimating`), not from a timer. Put that in its own `qa: test fix` commit, then rerun `62-perf-heavy-b`.
  - If PF-04-heavy passes, you're done; report the before and after.
  - If it still fails, profile the frame (QML scene graph vs. sync vs. render) and fix the app.
- **Done when:** PF-04-heavy passes 3 runs in a row with a per-frame driver.

## 2. Drag frame time on the stress show (LOBS-025, PF-04-stress)

- **Measured:** p50 30.6 ms, p95 **50.3 ms** [≤ 33], p99 83 ms [≤ 66]. It was 79 ms in fix pass 2 and 114 ms before that. Moves cost 3.6 ms, so most of each frame is QML rendering the window (fix pass 2 measured 12 ms p50 and 58 ms p95; see `_probe-drag-stress.js`).
- **Do:** measure again with the per-frame driver from §1. Then profile one dragged frame with the QML profiler (`qmlprof-drag` in `qa/runs` is an earlier capture) and fix what dominates. Likely suspects are delegates and bindings that re-evaluate for every move: the Layers rows, Inspector fields and the selection overlay.
- **Done when:** PF-04-stress passes, with a before/after table in the REPORT.md §5 format.

## 3. GDI objects and handles on the stress show (LOBS-025 remainder)

- **Measured after opening stress:** **2,006 GDI objects** (the Windows limit is 10,000 per process) and **237,807 handles**, 4.1 GB private.
- **Cause (fix pass 2):** each of the 818 text layers owns a GDI+ text renderer (`text_gdiplus`). The handle count follows the OBS source count: 3,270 layer instances plus the text renderers, each with OBS's per-source locks and events.
- **Expected direction:** don't create text renderers for scenes that aren't showing (park them like LOBS-034 parks filters), or share one renderer per text source rather than one per layer. Report what's left with a cause.
- **Done when:** GDI on stress is under 1,000 after open, PF-01…07-stress still pass, and text renders the same (compare the snapshots in `20-scenes-layers`).

## 4. Memory per layer click is close to its limit (RG-05)

- With RG-05 measuring settled memory now, clicks retain **0.41–0.48 MB each** (limit 0.5): runs `20261023-154700` … `-155109`. The old reading hid this.
- **Do:** heap-profile 100 `selectLayer` calls on the heavy show. Find out what each click keeps: Inspector bindings, history, selection snapshots, or QML objects.
- **Done when:** you report the cause, and RG-05 click retention is under 0.25 MB, or you explain why ~0.45 MB is the bounded cost of something that stops growing.

## 5. Optional, only if time allows
- **LOBS-036:** share instances individually in `Doc` so an edit copies one layer, not the whole 3,270-entry table. Then raise the history cap again. PF-06-stress must stay passing.
- **Peak memory while flicking through scenes (LOBS-038 note):** fast switching briefly holds 300–950 MB above the start before parking gives it back. Ask the product owner before you change anything. If it's worth doing, park on a shorter timer while switches are rapid.

## Not in this pass
- **CC-01** needs a speech model on the PC (`LOBS_WHISPER_MODEL`). It's environment, not a bug.
- **The 0.3.x features** (updater, crash reports, splash, video panel drag, Audio Mixer reorder and pin) haven't had independent QA. QA will test them separately, so don't add their stories here.

## When you finish
1. Rerun:
   - `qa\run.ps1` (all scenarios)
   - `qa\run-output.ps1`
   - `qa\tools\crash_autosave.ps1 -Iterations 8`
2. Regenerate the matrix: `python -I qa\tools\matrix.py`.
3. Reply with:
   - fixed and verified (ID → commit → passing run)
   - not fixed and why
   - the harness change for §1 and what it showed
   - before/after performance and resource counts
   - any new bugs (added to BUGS.md)
