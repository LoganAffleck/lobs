# Fix pass 2: prompt for the main agent

QA independently verified your first fix pass. Read `qa/VERIFY.md` first, then the entries named below in `qa/BUGS.md`. The same ground rules as `qa/FIX_PROMPT.md` apply:
- prove each fix with the named story;
- don't weaken tests (a real test fix goes in its own `qa: test fix` commit);
- one bug per commit;
- update BUGS.md statuses;
- don't push or open a PR.

QA's regression probes live in `qa/scenarios/90-regression.js`. Don't edit them; if you think one is wrong, say why.

## 1. Live lock: apply the product decision, close the back doors (LOBS-006 reopened, LOBS-033)

**Decision (product owner, 2026-10-20):** visibility and opacity are **live controls**. While live and locked they stay allowed on the on-air scene, from both the Layers eye icon and the Inspector, because a show runner hides and shows overlays and CTAs mid-show.

Everything else on air stays locked: move, size, crop, corners, filters, rename, duplicate, reorder, group, add or delete layers, delete source, delete the on-air scene, source settings, link and reset.

- Allow `setVisibleFlag` and `setOpacity` on air. `toggleVisible` already allows it, so both paths now agree.
- Guard `deleteSource` when any of the source's layers are on air, and guard `deleteScene` for the program scene. Use the same toast, with the Alt or Unlock override.
- Audit every committing function in `app.cpp` against the list above. QA's scan found 20+ with no `live*EditAllowed` call. List what you guarded and what you deliberately left open (audio controls, transitions, scenes that aren't on air).
- **LOBS-033:** remove the leftover blanket `if (liveLocked())` in `App::nudge`. Nudge on off-air scenes must work while live, and Alt must override on air.
- **Done when:** LY-06, RG-01 and RG-02 pass, and the functional suite stays 62/62.

## 2. Memory that grows with every visited scene (LOBS-034, LOBS-022 remainder)

- **Cause (from the code):** `li_video_render` creates `li->texrender` at the source's full size on first render and frees it only in `li_destroy` ([lobs_instance.c:140](../native/src/lobs_instance.c:140)).
- **Measured:** heavy keeps 17–56 MB per first-visited scene and never releases it (about 3.4 GB in total). Stress sits at 6.4 GB after open.
- **Confirm before you fix.** Use `qa/scenarios/_probe-leak2.js` (with vs. without frames, visited vs. revisited scenes) and say what you found.
- **Expected direction:**
  - free the render target when the instance stops showing (hide/deactivate);
  - skip it entirely when there's no crop or radius;
  - draw color sources without a canvas-sized texture;
  - consider one target per source rather than per layer.
- **Done when:**
  - RG-05 passes: ≤ 0.5 MB retained per switch across all 40 heavy scenes.
  - Memory after visiting every scene of the heavy project stays under 1.5 GB above the empty-show baseline.
  - The GDI count on the stress project is reported, with a cause if it's still above 1,000.

## 3. Stress targets still missed (LOBS-025) and heavy drag (LOBS-024)

From `runs/20261020-191455`, stress targets in brackets:
- Select scene p95 339 ms [≤ 250]
- Linked edit across 100 scenes p95 393 ms [≤ 250]
- Drag frame p95 114 ms [≤ 33]
- Scrub p95 120 ms [≤ 66]
- **Autosave stall up to 2.9 s [never > 1 s]:** move the JSON serialization off the UI thread, or snapshot cheaply and write in the background.
- **Heavy drag frame interval** p95 33.8 ms [≤ 16.7].

Profile each one before changing it, and report a before/after table in the REPORT.md §5 format (`61-perf-stress.js`, `63-perf-stress-b.js`, `62-perf-heavy-b.js`).

## 4. Small items
- **LOBS-035:** replace the OBS "Windows 10 Ransomware Protection" wording in the recording error with "Windows Security → Controlled folder access".
- **LOBS-021 remainder:** name the unnamed real controls in the AX-01 census sample (an OutputControls button, the dock tab bars, list rows).

## When you finish
1. Rerun:
   - `qa\run.ps1` (all scenarios, including `90-regression`)
   - `qa\run-output.ps1`
   - `qa\tools\crash_autosave.ps1 -Iterations 8`
2. Regenerate the matrix: `python -I qa\tools\matrix.py`.
3. Reply with:
   - fixed and verified (ID → commit → passing run)
   - not fixed and why
   - the live-lock audit list
   - before/after performance
   - any new bugs (added to BUGS.md)
