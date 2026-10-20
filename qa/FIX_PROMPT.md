# Fix pass: prompt for the main agent

You are fixing the bugs found in the LOBS v0.1.1 QA pass. The QA agent tested; you fix. Read these first, in order:

1. `qa/REPORT.md`: summary, priorities, performance table, coverage gaps
2. `qa/BUGS.md`: each bug with repro, evidence, cause (file:line) and a suggested fix
3. `qa/TEST_PLAN.md`: the performance targets you're fixing toward

## Ground rules

- **Prove every fix with the harness.** Each bug names a story (LY-06, PJ-01, PF-02…). Reproduce the failure first with `qa\build-qa.bat` then `powershell -File qa\run.ps1 <scenario>`. Fix it, then show the same story passing. No fix is done on code reading alone.
- **Don't weaken tests to get a pass.** If you believe a test is wrong, explain why in your summary and change it in a separate commit labeled `qa: test fix`. Never fold a test change into a product fix.
- **Add a regression check** for every bug that doesn't already have an automated one (LOBS-001, LOBS-010, LOBS-019 and others). Put it in the matching `qa/scenarios/*.js`.
- **Keep the harness test-only.** `LOBS_TESTS` stays OFF by default. `native\build.bat` must still build the shipped app with no test code.
- **One bug (or one tight group with the same root cause) per commit.** The message names the IDs, e.g. `Fix LOBS-007: save the current project before Open`.
- **Update `qa/BUGS.md` as you go.** Set each entry's status to `Fixed in <commit>`, with the run folder that shows the passing story. Don't delete entries.
- **Terminology:** the concept is **Sources** everywhere, never "Media Pool" (LOBS-004).
- **Don't change user-visible behavior beyond what a bug asks.** Where a fix needs a product decision (for example D1/D2 spec gaps, or how Reset should behave on linked sections), stop and list it as a question rather than picking.
- **Be careful with the environment.** The shell's %APPDATA% is sandboxed. Launch user-facing instances with `explorer.exe`. Never stream to a live service; `qa\run-output.ps1` uses a local ffmpeg receiver. Don't kill the user's installed LOBS (`AppData\Local\Programs\LOBS`).

## Order of work

### 1. Quick data-loss fixes (do first, small)
- **LOBS-001:** self-test detection must match exact flags in `arguments().mid(1)`, never a substring of the joined command line. Regression check: launch from a folder whose name contains "test" (PJ-04).
- **LOBS-009:** `saveAs` must not set `m_projectFile` / `project/file` until the write succeeds.
- **LOBS-008:** `createProject` writes the new file before `newDocument()` and rolls back on failure.
- **LOBS-007 and LOBS-026:** Open and Import save the current project first, or ask. Import while live must not `take()`.
- **LOBS-002:** use `QSaveFile` for autosave, `saveProject` and `saveAs`; check write results and toast on failure.
- **LOBS-010:** reject Windows reserved names and over-long paths in `checkNewProject`.

### 2. Memory leak and editor performance (the release blocker)
LOBS-022 (S1), LOBS-023, LOBS-024 and LOBS-025.

- **Find the cause before you change anything.** Heap-profile one scene switch and one layer click on the heavy project: `python -I qa/tools/gen_project.py heavy qa/runs/fixtures`, plus the probes `qa/scenarios/_probe-growth.js`, `_probe-growth2.js` and `_probe-select.js`. Report what's leaking with evidence.
- **Leads from QA (unconfirmed):**
  - Every change, including selection and edit-scene changes, goes through `emit docChanged()`. That re-evaluates every QML model (`scenes`, `pool`, `layers`, `inspector`, `history`, `mixer`, `transitions`).
  - It also runs a full `Engine::apply` over every source and `rebuildHotkeys()`.
  - The `Instantiator`-built "Copy to" submenus are recreated per change.
- **Expected direction:** separate signals (`selectionChanged`, `editSceneChanged`, `docChanged`), an incremental engine apply, and hotkeys rebuilt only when scenes or sources are added or removed. Keep capture activation off the UI thread (display-capture scenes take 1.3–1.5 s per switch).
- **Done means:**
  - The leak is gone: memory flat over 100 switches, clicks and edits.
  - Heavy-project PF-01…PF-07 meet the targets (`60-perf-heavy.js`, `62-perf-heavy-b.js`).
  - Stress PF-01/02 stay usable (`61-perf-stress.js`). Then run `63-perf-stress-b.js`, which QA couldn't run.
  - Report before/after tables in the same format as REPORT.md §5.
- **Watch the GDI count** on the stress project (2,003; the OS limit is 10,000).

### 3. Live safety and accessibility
- **LOBS-006:** one live-lock guard for every edit on the program scene's layers (Inspector, fit, scrub, delete…), with the existing toast and the Alt/Unlock override.
- **LOBS-021:** `Accessible.name`/`role` on FButton, icon-only buttons and list rows, defaulting to `tip`. The AX-01 census should reach ≥ 80%.

### 4. S3 and S4 fixes
LOBS-005, 011 (*ask first:* should the group link itself?), 012 (*ask first:* Reset semantics), 013, 014, 015, 016, 017, 018, 019, 020, 027, 028, 029, 030, 003, 004.

## When you finish
1. Rerun everything: `powershell -File qa\run.ps1`, `qa\run-output.ps1`, `qa\tools\crash_autosave.ps1 -Iterations 15`.
2. Regenerate the matrix: `python -I qa\tools\matrix.py`.
3. Reply with:
   - fixed and verified (ID → commit → passing run)
   - not fixed and why
   - open product questions
   - new bugs you found (add them to BUGS.md with the next free ID)
   - before/after performance
4. Don't push, and don't open a PR, unless the user asks.
