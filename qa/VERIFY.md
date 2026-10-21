# Fix-pass verification

**Verified:** HEAD `2f3ad18` (40 commits after the QA report) · 2026-10-20 · by the QA agent, independently
**Method:** clean rebuild of both builds, then QA's full suite plus new QA-written regression probes (`scenarios/90-regression.js`, `_probe-leak2.js`). The probes were written after reading the fix diffs, to look for back doors and regressions.

## Bottom line
The fix pass is real and large. **Correctness is green: 62/62 functional stories pass** (was 38/62). Heavy-project speed is fixed (scene switching 5.0 s → 60 ms p95; clicks 411 → 31 ms). Output, import, persistence and the crash test all pass.

**Still open:**
- **2 reopened / partly fixed:** LOBS-006 live-lock back doors, and LOBS-022 memory.
- **3 new:** LOBS-033, LOBS-034, LOBS-035.
- **LOBS-025 stress scale** still misses several targets.
- **1 product question** about live-lock scope.

**Verdict:** releasable for small and medium shows once LOBS-006 is closed. Not yet for very large shows (LOBS-034/025).

## Results
| Suite | Result | Run |
|---|---|---|
| Functional (9 scenarios, 62 stories) | **62 pass / 0 fail** | `runs/20261020-190750` |
| QA regression probes (6) | 3 pass / **3 fail** | `runs/20261020-191132` |
| Output (6 stories + ffprobe) | 6/6 pass; files valid 1080p60 H.264+AAC | `runs/20261020-191339-output` |
| Perf heavy (7 stories) | 6 pass / **1 fail** (drag frame p95) | `runs/20261020-191455` |
| Perf stress (7 stories) | 2 pass / **5 fail** | `runs/20261020-191455` |
| Autosave crash test (now atomic writes) | **8/8 valid** | `runs/20261020-191910-crash-autosave` |

## Bug status after verification
- **Verified fixed (29):** LOBS-001, 002, 003, 004, 005, 007, 008, 009, 010, 011, 012, 013, 014, 015, 016, 017, 018, 019, 020, 021*, 023, 026, 027, 028, 029, 030, 031, 032, 024**.
  - \* LOBS-021: 102 of 127 interactive controls named (80.3%). The census now excludes static text, which is defensible. Unnamed real controls remain: an OutputControls button, the dock tab bars and some list rows.
  - \** LOBS-024: everything in the heavy project is in budget except drag frame time, p95 33.8 ms against 16.7 ms (typical 58 fps, worst 5% about 30 fps).
- **Reopened:** **LOBS-006** (S2). The Layers eye icon hides on-air layers, Delete source removes on-air layers, and deleting the program scene switches what's on air mid-recording. All three have no live guard.
- **Partly fixed:** **LOBS-022** (S1 → S2). The unbounded UI leak is gone: 0 MB retained switching between visited scenes, about 0.4 MB per click. But each scene's first render keeps about 17–56 MB forever (→ LOBS-034).
- **Partly fixed:** **LOBS-025.** Stress open 8.3 s ✅, Show 190 ms ✅, click 149 ms ✅. Still failing: select scene 339 ms, linked edit 393 ms, drag p95 114 ms, scrub 120 ms, and an **autosave stall up to 2.9 s**. Still 6.4 GB / 270k handles / 2,003 GDI objects after open.
- **New:**
  - **LOBS-033** (S3): while live, arrow-key nudge is refused on off-air scenes. An old blanket check survived the new guard and also ignores Alt.
  - **LOBS-034** (S2): every layer keeps a source-sized GPU render target after its scene stops showing ([lobs_instance.c:140](../native/src/lobs_instance.c:140)). Memory ≈ the sum of visited layers: about 3.4 GB on heavy, tens of GB on stress.
  - **LOBS-035** (S4): the recording error still mentions "Windows 10 Ransomware Protection".

## Test changes made by the fix agent (reviewed)
Nothing was weakened to hide a failure.
- **LK-12:** rewritten to the product decision ("Reset is local") and stricter than before.
- **LY-06, PJ-01d/e, PJ-04…07, MP-04u, LK-01m:** new regression checks.
- **AX-01 census:** narrowed (see LOBS-021 above).

## Product decision (2026-10-20, from the product owner): visibility and opacity are live controls
- While live and locked, **show/hide and opacity stay allowed** on the on-air scene, from both the Layers eye icon and the Inspector. A show runner needs to hide and show overlays or a temporary CTA mid-show.
- **Everything else on air stays locked:** move, size, crop, corners, filters, rename, duplicate, reorder, group, add or delete layers, delete source, delete the on-air scene, source settings, link and reset.
- The tests encode this: LY-06 and RG-02 (updated by QA). Both fail on `2f3ad18`, because the Inspector refuses hide and opacity while Delete source and Delete scene still slip through.

## Not re-verified
The manual walkthroughs ([MANUAL.md](MANUAL.md)) are still pending. They matter more now: the UI was restructured (RowsModel, menus that build only while open, one shared context menu), and automation only spot-checks that panels refresh (RG-06 passes).
