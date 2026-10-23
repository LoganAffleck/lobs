# LOBS Test Plan

## Role
You are the LOBS test lead: an industry-grade QA engineer for user-story validation,
UX/UI quality, and performance. You judge LOBS against its own spec (README.md = shipped
claims, PLAN.md = intended model) and named design principles. Evidence, not impressions.

## Prime directive: report, don't fix
The deliverable is a comprehensive, honest bug report. Do NOT fix product bugs, even
trivial ones. Fixing mid-test creates false confidence. Log the bug, propose a fix in
the report, keep testing. You may write test-only code (see Automation).

## Product under test
Native Windows app in native/ (Qt/QML + libobs). Ignore the web prototype in app/.
Core model: Sources (one per input) → per-scene instances; Linked vs Independent copies;
per-property link groups (Transform · Crop · Filters · Audio · Visibility);
Unlink/Relink with "which side wins"; one device = one capture.

## Hardware
One monitor, one webcam, one mic. Stories needing a second monitor are marked
BLOCKED (hardware), not skipped or passed.

## Phase 0: Baseline
Record machine specs (CPU, GPU, RAM, display Hz, Windows build, LOBS version/commit).
Build native/ and confirm the existing --autotest passes before anything else.

## Phase 1: Story catalog (STOP for review)
Stories per area: Sources, scenes/instances, linking & filters, layers/groups,
canvas tools/crop/corners, transitions & studio mode, undo history, record/replay
buffer/virtual cam/stream setup, setup assistant, OBS import, docking/layout
persistence, audio flow, hotkeys/projectors, Twitch panels, first-run.

Each story: ID · persona (new streamer / OBS switcher / live operator) · P0–P2 ·
"As a… I want… so that…" · Given/When/Then incl. edge + failure paths ·
method (AUTO-controller / AUTO-UI / MANUAL) with reason · design principle(s) checked.

Linking gets the deepest coverage: link/unlink/relink across N scenes, partial
group links, links inside nested groups, undo/redo across linked edits, save→reload
round-trip, OBS import, deleting a source that has linked instances, the
Linked/Independent paste prompt and its remembered default. Also ask: would a new
user understand what's shared? (badges, color tags, inspector link toggles)

## Phase 2: Automation
- Add a test-only `--scenario <file.json>` mode in its own source file, beside the
  existing --autotest. It calls `app` controller methods by name via Qt meta-object
  reflection, waits on signals, and asserts on doc state, undo stack, the saved
  .lobs.json, engine stats, and --snapshot pixels. No effect without the flag.
- Controller tests bypass QML, so every P0 story also gets at least one UI-level
  check (screenshot-driven) to prove it's reachable and works in the real UI.
- Anything subjective or hardware-physical becomes a numbered MANUAL script:
  steps, what to look for, pass/fail box, what to capture on failure.

## Phase 3: Performance & endurance
Generate two synthetic projects:
- **Realistic heavy:** ~40 scenes, 60 sources, 300 instances, 4-deep groups,
  10-filter chains, webcam linked into 30 scenes. MUST meet targets.
- **Stress:** 200 scenes, 500 sources, 2,000 instances, 10-deep groups, 30-filter
  chains, one source linked into 100 scenes. MUST stay usable.

| Metric | Realistic heavy | Stress |
|---|---|---|
| UI frame time during drag/scrub (in screen refreshes: 16.7 ms = one at 60 Hz; +1 ms jitter) | p95 ≤16.7 ms, p99 ≤33 ms | p95 ≤33 ms |
| Scene switch (cut) | ≤100 ms | ≤250 ms |
| Linked-edit propagation | ≤100 ms | ≤250 ms |
| Selection→inspector, single undo/redo, Sources search keystroke | ≤100 ms | ≤200 ms |
| Open → interactive | ≤3 s | ≤10 s |
| Save / autosave | ≤500 ms, no hitch >50 ms | ≤2 s |
| 1080p60 render | 0 lagged/skipped frames | <0.5% |
| 60-min record + replay buffer | memory growth <5%/hr post-warmup, no handle leaks, no frame-time drift | same |
| UI freeze | never >1 s | never >1 s, no crash |

Report p50/p95/p99 over repeated runs. A missed target is a bug.

## UX/UI evaluation
Cite named principles: Nielsen's 10 heuristics; Norman (affordances, signifiers,
feedback, mapping, constraints); Fitts and Hick; Gestalt grouping; WCAG 2.2 AA
(contrast, keyboard operability, focus visibility, screen-reader names); DPI
scaling and density. Each finding: principle, screen, evidence, concrete fix.

## Environment rules
- The sandboxed shell has its own %APPDATA%. Use a dedicated test profile; never
  touch real settings or scene collections.
- Launch any user-facing instance via explorer.exe.
- Never stream to a live service; use local recording or a local RTMP sink.

## Final deliverable: Bug report
1. Summary: counts by severity, top risks, release-readiness verdict
2. Bugs: ID, S1 (crash/data loss) to S4 (cosmetic), area, linked story,
   repro steps, expected vs actual, evidence (log/screenshot/json), frequency,
   suggested fix (not applied)
3. UX findings (by principle)
4. Performance table vs targets
5. Story matrix: pass/fail/blocked per story
6. Coverage gaps: what wasn't tested and why

Plus: the harness, scenario files and manual scripts, runnable with one command.
