# Copy review: LOBS app UI

Scope: `native/qml/**/*.qml` and user-facing strings in `native/src/*.cpp|.h|.mm`. Comments, logs, `web/`, release notes and keys are out of scope. The full string inventory (file:line, every literal) is in the appendix. The proposed changes come first.

Status: **applied** (macOS menu bar kept in title case, per review).

## Terms (one name per concept)

| Concept | Use | Replace |
|---|---|---|
| A capture, device, page or file added once | **source** | "input" (Sources tooltips, mixer counts, Inspector captions, help) |
| A source placed in a scene | **layer** | — |
| The scene viewers see | **on air** (canvas badge, live lock) | "program" in tooltips and toasts ("Record the program scene", "On program: X"). The projector window keeps "Program" as its title. |
| Hearing a source yourself | **monitor / monitoring** | "hear it in your headphones" |
| Panel names | Scenes, Sources, Assets, Layers, Inspector, **Audio mixer**, History, Stats, Captions | "Audio Mixer" (the only title-case panel), "the Audio tab" (doesn't exist) |
| What size, crop and filters belong to | **per layer** | "per scene" (two layers of one source can sit in the same scene, so "per layer" is the accurate one) |
| Ellipsis | **…** | "..." in 7 places |

## Proposed changes

Legend: **T** = a QA scenario matches this text and will be updated in the same change.

### Top bar and menus (Main.qml)

| Where | Old | New | Why |
|---|---|---|---|
| Main.qml:82–178 | New Project…, Open Recent, Clear Menu, Import from OBS, Choose a File…, Save As…, About LOBS, Paste Linked, Bring to Front, Bring Forward, Send Backward, Send to Back, Enter/Exit Full Screen, Show All Panels, Reset Layout, LOBS on GitHub, Report a Problem… | Kept in title case | Decided in review: macOS menu bar convention. |
| Main.qml:377 **T** | Choose a file... | Choose a file… | Real ellipsis; matches the menu bar item. |
| Main.qml:350 | Start a new project in its own folder | New project in its own folder | Shorter. |
| Main.qml:2545 | Downloading... | Downloading… | Ellipsis. |
| Main.qml:2546 | LOBS closes, installs the update and opens again in about a minute. Your shows and settings stay as they are. | LOBS will close, install the update and reopen in about a minute. Your shows and settings are kept. | Plainer. |
| Main.qml:2547 | You have X. The update downloads in the background while you keep working. | You have X. The update downloads in the background. | Cut chatty clause. |
| Main.qml:2560 | Stop streaming, recording, the replay buffer and the virtual camera first. LOBS won't close while they're running. | Stop streaming, recording, the replay buffer and the virtual camera first. | Second sentence restates the first. |
| Main.qml:2592 | Sorry about that. LOBS saved a crash report on this PC. Reporting it on GitHub helps get it fixed. The issue asks for the log, which is safe to post. Keep the .dmp file private: it can hold what LOBS had in memory, like your stream key. | LOBS saved a crash report on this PC. Report it on GitHub and attach the log. Don't post the .dmp file: it can contain your stream key. | 5 sentences → 3; keeps the risk. |
| Main.qml:2615 | This show has never been saved. If you don't save it, its changes are lost. | This show hasn't been saved. Your changes will be lost if you don't save it. | Plainer. |
| Main.qml:2640 | It's used in N scenes (a, b). Deleting it removes it from all of them. You can undo this. | It's in N scenes (a, b) and will be removed from all of them. You can undo this. | One clause shorter. |

### Scenes

| Where | Old | New | Why |
|---|---|---|---|
| Main.qml:1776 help | Grid: shows live thumbnails of every scene. | Grid: live thumbnails of every scene. | Shorter. |
| Main.qml:1776 help | Right-click: rename, duplicate with linked or independent layers, or delete. | Right-click: rename, duplicate or delete. | The menu shows both duplicate options. |
| app.cpp:641 toast | On program: X | On air: X | Term. |
| app.cpp:1077, 1091 | A scene can't show itself, even through another scene | A scene can't contain itself, even through another scene | "show" is also a button and a document here. |

### Sources

| Where | Old | New | Why |
|---|---|---|---|
| Main.qml:774, MixerView.qml:175, 271, SourceFields.qml:50 | Muted. Click to unmute. / Mute | Unmute / Mute | Tooltip names the action, like the unmuted state. |
| Main.qml:877 | Not used in any scene yet | Not in any scene | Shorter. |
| Main.qml:890 | No audio inputs yet. / No video inputs yet. | No audio sources yet. / No video sources yet. | Term. |
| Main.qml:1777 help | Inputs: added once, shared by every scene. | Sources: add once, use in any scene. | Term. |
| Main.qml:1777 help | Per scene: size, crop and filters. | Per layer: size, crop and filters. | Term. |
| Main.qml:2186 | Camera · input settings apply to every scene that uses it. Size, crop and filters are set per layer. | Camera settings apply in every scene. Size, crop and filters are per layer. | Middle-dot aside; "input". |
| Main.qml:2200 | Not on any scene yet — drag it onto the canvas. / Audio source — always mixed in. | Not in any scene. Drag it onto the canvas. / Audio source. Plays in every scene. | Em-dashes; matches the Sources row text. |
| SourceSection.qml:59 | Shared input | Shared source | Term. |
| SourceSection.qml:67 | Changes this input everywhere: a, b. Size, crop and filters are per scene. | Changes apply in: a, b. Size, crop and filters are per layer. | Term. |
| SourceSection.qml:101 | Changes here apply in every scene that uses X. / Changes here apply wherever X is used. | Changes apply to X in every scene. | One string, shorter. |
| SourceSection.qml:104 | Show this input in the Sources panel | Select it in Sources | Term, shorter. |
| app.cpp:1107, 2458 | Audio sources aren't placed on the canvas | Audio sources can't go on the canvas | Plainer. |
| app.cpp:2539 | Game Capture is Windows-only | Game capture is Windows-only | Sentence case. |

### Add source dialog (Main.qml)

| Where | Old | New | Why |
|---|---|---|---|
| 2314 | Each input is added once and shared by every scene. | Add each source once and use it in any scene. | Term. |
| 2271 | Name  ·  in use | Name (in use) | Middle-dot aside. |
| 2376 | Another app is using this camera. Close it there to use it here. | Another app is using this camera. Close that app first. | "it there… it here". |
| 2381 **T** | Already in your show, so this is its feed. A camera opens in one place at a time. | Already in your show. A camera opens in one place at a time, so this is that feed. | Clearer order; keeps "Already in your show" for the test. |
| 2388 | Enter a URL or pick a file to see the page | Enter a URL or choose a file | "Choose a file" is used everywhere else; the preview is implied. |
| 2435 | List the open windows again | Refresh window list | Shorter. |

### Layers and canvas

| Where | Old | New | Why |
|---|---|---|---|
| Main.qml:1778 help | Add files: drag in from File Explorer or use + Asset. | Add files: drag in from File Explorer or click Add. | There's no "+ Asset" button; it's "Add". |
| Main.qml:1779 help | Shortcuts: Ctrl+C / Ctrl+V copy and paste (Ctrl+Shift+V pastes linked), Ctrl+D duplicate, Ctrl+G group, Delete remove. | Keys: Ctrl+C copy, Ctrl+V paste, Ctrl+Shift+V paste linked, Ctrl+D duplicate, Ctrl+G group, Delete remove. | Parenthetical removed; same pattern throughout. |
| Main.qml:1835–1837 | Drag up or down on the canvas to set the size, then type. Esc or a click elsewhere finishes. / Used for the next one you draw. Select a shape to change its own. | Drag up or down to set the size, then type. Esc or click away to finish. / Applies to the next shape. Select a shape to change it. | Wordy. |
| Main.qml:1923 | Layer Fit | Layer fit | Sentence case. |
| Main.qml:1926 | Restore the source's own proportions | Use the source's proportions | Shorter. |
| Main.qml:1945 | W and H keep their proportions. Click to change them separately. / W and H change separately. Click to keep proportions. | Unlock proportions / Lock proportions | Tooltip names the action. |
| Main.qml:1981 | Finish cropping (Enter). Esc puts it back. / Drag the edges on the canvas to crop, or inside to move the crop. Double-click a layer to start. | Finish cropping (Enter). Esc cancels. / Drag an edge on the canvas to crop, or inside to move it. Shortcut: double-click a layer. | Shorter; keeps all three facts. |
| Main.qml:1987 | Hide exact crop values / Exact crop values | Hide crop values / Show crop values | Paired verbs. |
| GraphicTools.qml:22 | Laser pointer, not added to the scene | Laser pointer (not added to the scene) | Comma aside. |
| CanvasArea.qml:34 | Live · editing what's on air is locked. Hold Alt, or unlock to edit. / Live · live lock is off, edits go on air. | Live: on-air layers are locked. Hold Alt or click Unlock to edit. / Live: edits go on air. | Middle dot; comma chain. |
| CanvasArea.qml:56 help | Groups: click selects the group. Each double-click goes one level deeper. | Groups: click selects the group. Double-click to go one level deeper. | Plainer. |
| CanvasArea.qml:70–72 | Pixel-perfect: each canvas pixel lands exactly on screen pixels, so text is crisp. Click to fit the panel exactly instead. / Pixel-perfect is on, but no exact ratio fits well at this panel size, so it fits the panel. Make the panel a little larger to snap. / Fits the panel exactly (may soften text). Click for pixel-perfect scaling. | Pixel-perfect: sharpest text. Click to fit the panel. / Pixel-perfect can't fit this panel size. Make the panel larger. / Fit to panel. Text may look soft. Click for pixel-perfect. | 3 long tooltips. |
| app.cpp:1342 | To group layers, pick ones at the same level (all inside one group, or all outside) | To group layers, pick layers at the same level | Parenthetical restates "same level". |
| app.cpp:2381, 2403 | Live lock is on. Hold Alt or unlock to change layers on air. | On-air layers are locked. Hold Alt or click Unlock to edit. | Says what's locked. |
| app.cpp:2392 | Live lock is on. Hold Alt or unlock to change sources on air. | On-air sources are locked. Hold Alt or click Unlock to edit. | Same. |
| app.cpp:3724 / 3735 / 3812 | Live lock is on. Hold Alt to add graphics. / …to resize. / …to move layers. | On-air layers are locked. Hold Alt to add graphics. / …to resize. / …to move. | Same. |

### Inspector

| Where | Old | New | Why |
|---|---|---|---|
| Main.qml:1780 help | Numbers: math like 1920/2 works. Up/Down nudge, Shift for 10. | Numbers: type math like 1920/2. Up and Down nudge, Shift for 10. | Plainer. |
| Main.qml:1780 help | Source: select one in Sources to edit its input. | Source: select it in Sources to edit it in every scene. | Term. |
| BrowserOptions.qml:101–102 **T** (has "Follows layer", unaffected) | Resizing the layer resizes the page, like a browser window. Click to set a fixed size. / The page keeps this size and the layer scales it. Click to have the page follow the layer. | The page resizes with the layer. Click to set a fixed size. / The page keeps this size. Click to follow the layer. | Cut simile and restatement. |
| BrowserOptions.qml:56 | Reload the page, skipping the cache | Reload, ignoring the cache | Shorter. |
| BrowserOptions.qml:196 | Back to the default: a transparent page | Reset to default (transparent page) | Colon aside. |

### Audio mixer and audio settings

| Where | Old | New | Why |
|---|---|---|---|
| Main.qml:1698 **T** (13 matches) | Audio Mixer (panel title) | Audio mixer | Only title-case panel name. Will check it isn't a saved layout key. |
| MixerView.qml:56 | Not monitored. Click to hear it in your headphones (and keep it in the output). / Monitor only: you hear it, viewers don't. Click to turn monitoring off. / Monitored: you hear it, and it's in the output. Click to turn monitoring off. | Not monitored / Monitor only: you hear it, viewers don't / Monitored: you and viewers hear it | State only; the icon toggles. |
| MixerView.qml:112 | 1 input / N inputs | 1 source / N sources | Term. |
| MixerView.qml:124 | Strips. Click for rows. / Rows. Click for strips. | Show as rows / Show as strips | Action tooltip. |
| MixerView.qml:177, 273 **T** | Monitoring, tracks and effects (in the Inspector) | Audio settings | Parenthetical; the click opens them. |
| MixerView.qml:287 | No audio inputs match “x” / No audio sources. Add a Mic or Desktop audio from Sources. | No audio sources match “x” / No audio sources. Add a mic or desktop audio in Sources. | Term, case. |
| Main.qml:1781 help | Tracks: track 1 goes to the stream; recordings can use tracks 1-6. | Tracks: track 1 is the stream. Recordings can use tracks 1–6. | Semicolon, en dash. |
| Main.qml:1781 help | Monitor: hear it in headphones. Monitor only keeps it out of output. | Monitor: hear it yourself. Monitor only keeps it off the stream and recording. | "output" is internal. |
| AudioSettings.qml:68 | Audio is delayed N ms to line up with slower video. | Audio is delayed N ms to match slower video. | Shorter. |
| AudioSettings.qml:69 | Mono: both channels carry the same mix (useful for a mic that only comes in on one side). | Mono: the same mix on both sides. Fixes a mic heard on one side only. | Parenthetical. |
| app.cpp:3471 (warn) | Echo risk: monitoring desktop audio plays it back into itself. | Monitoring desktop audio causes an echo. | Says what's wrong plainly. |
| app.cpp:3473 (warn) | Feedback risk: you hear your own mic. Use headphones, or speakers will feed back into it. | You'll hear your own mic. Use headphones to avoid feedback. | Shorter, same advice. |
| app.cpp:3475 (warn) | Monitor only: you hear it, the stream and recording don't. | Monitor only: not in the stream or recording. | Shorter. |
| app.cpp:3477 (warn) | Not monitored: viewers hear this page's sounds (alerts), you don't. Click the headphones to hear it. | Viewers hear this page but you don't. Turn on monitoring to hear it. | Parenthetical; "Click the headphones". |
| app.cpp:3479 (warn) | Track 1 is off, so it isn't in the stream. | Track 1 is off, so it's not in the stream. | Minor; keep or skip. |
| SettingsPage.qml:350 | Always mixed in, in every scene. | Plays in every scene. | Matches Sources. |
| SettingsPage.qml:360 | Levels, monitoring and tracks are in the Audio tab. | Levels, monitoring and tracks are in the Audio mixer. | There's no Audio tab. |

### Output controls, status

| Where | Old | New | Why |
|---|---|---|---|
| OutputControls.qml:71–72 | Replay buffer is on (last N s). Click to turn it off. / Keep the last N s so you can save a highlight | Replay buffer on (last N s). Click to turn off. / Keep the last N s to save as a clip | Shorter. |
| OutputControls.qml:88–89 | Other apps see the program scene as "OBS Virtual Camera". Click to stop. / Send the program scene to Zoom, Meet, Discord and others as "OBS Virtual Camera" | On as "OBS Virtual Camera". Click to stop. / Send the on-air scene to other apps as "OBS Virtual Camera" | Term; cut the list. |
| OutputControls.qml:90 | Set up the virtual camera (Windows asks for permission once) | Set up the virtual camera. Windows asks for permission once. | Parenthetical. |
| OutputControls.qml:99 | Recording · N MB. Click to stop. / Record the program scene | Stop recording (N MB) / Record the on-air scene | Action tooltip; term. |
| OutputControls.qml:148 | Click again to end stream | Click again to stop | Shorter. |
| OutputControls.qml:165 | N dropped frames · click twice to end / Start streaming with your saved settings | N dropped frames. Click twice to stop. / Start streaming | Middle dot; restatement. |
| app.cpp:2507 | Resolution and FPS changes apply after you stop streaming and recording | Resolution and frame rate changes apply after you stop streaming and recording | "FPS" vs the "Frame rate" label. |
| engine_extras.cpp:138 | Replay buffer on · keeps the last %1 s | Replay buffer on (last %1 s) | Matches the top-bar tooltip. |

### Assets

| Where | Old | New | Why |
|---|---|---|---|
| Main.qml:1100–1101 | X is missing (moved or deleted outside LOBS). / N files are missing (moved or deleted outside LOBS). | X was moved or deleted outside LOBS. / N files were moved or deleted outside LOBS. | Parenthetical. |
| Main.qml:1103 | Search assets folders by file name | Search assets by file name | Shorter. |
| Main.qml:1106 | Point X at a file | Choose a new file for X | Odd phrasing. |
| Main.qml:1350 | This folder is empty. Drag files here, or paste with Ctrl+V. | Empty folder. Drop files here or paste with Ctrl+V. | Shorter. |
| Main.qml:1410 | In use: remove its layers first | In use. Remove its layers first | Colon aside. |
| Main.qml:1778 help | Project files: this project's assets folder. | Files: this project's assets folder. | Restates. |
| app.cpp:2748 | Couldn't rename X (is it open in another app?) | Couldn't rename X. Is it open in another app? | Parenthetical. |
| app.cpp:2781 | Cut X. Paste it into another folder. | Cut X | Matches the layer toast. |
| app.cpp:2842 | Nothing to paste. Copy files here or in File Explorer first. | Nothing to paste | Restates. |
| app.cpp:2882 | N items in use by layers, so kept. Remove those layers first. | Kept N items that layers use. Remove those layers first. | Clearer about what happened. |

### Captions

| Where | Old | New | Why |
|---|---|---|---|
| CaptionsPanel.qml:41 | Captions run on this PC, so your audio never leaves it. They need a one-time 142 MB download. | Captions run on this PC, so your audio stays private. Needs a one-time 142 MB download. | Shorter. |
| CaptionsPanel.qml:117 | Listening. Text shows up a few seconds after speech. | Listening. Text appears a few seconds after speech. | Plainer. |
| CaptionsPanel.qml:130 | CC (x) for Stream / · sent once you go live / · not sent to the stream | CC (x), sent to the stream / , sent when you go live / , not sent to the stream | Stray capital; middle dots. |
| Main.qml:1784 help | Language: pick it, or let LOBS detect it. / Text shows up a few seconds after speech. | Language: pick one or detect it automatically. / Text appears a few seconds after speech. | Plainer. |

### Settings

| Where | Old | New | Why |
|---|---|---|---|
| SettingsPage.qml:28 | New versions of LOBS, with the OBS engine they were tested with. | New versions of LOBS and its OBS engine. | Shorter. |
| SettingsPage.qml:187 | Auto picks the closest ingest. | Auto picks the closest server. | Jargon. |
| SettingsPage.qml:239 | While streaming or recording, layers on air don't change by accident. Hold Alt, or click Unlock in the banner, to edit. | Locks on-air layers while you stream or record. Hold Alt or click Unlock to edit. | Comma chain. |
| SettingsPage.qml:271, SetupWizard.qml:445 | High, medium files / Indistinguishable, large files (Settings) vs High (moderate files) / Indistinguishable (very large files) / Same as stream (small files) (wizard) | High (medium files) / Indistinguishable (large files) / Same as stream (small files) in both | Consistency. |
| SettingsPage.qml:282 | Turn it on from the top bar. Save replay writes to your recordings folder. | Turn it on in the top bar. Saved replays go to your recordings folder. | Plainer. |
| SettingsPage.qml:386 | <b>Global hotkeys are blocked right now.</b> macOS's Secure Keyboard Entry is on, so no app gets keys typed elsewhere. Close password fields and password managers, or turn off Secure Keyboard Entry in Terminal's menu. | <b>Global hotkeys are blocked.</b> Secure Keyboard Entry is on. Close password fields and password managers, or turn it off in Terminal's menu. | Shorter. |
| SettingsPage.qml:457, 461, 466 | Checking for updates... / Installing... / Update... | … | Ellipsis. |
| SettingsPage.qml:486 | Comes with LOBS and updates with it, so every release runs the engine it was tested with. | Ships with LOBS. Each release uses the engine it was tested with. | Comma chain. |
| SettingsPage.qml:508 | Checks LOBS's GitHub releases daily. Updates install only after the download matches its SHA-256 and is signed by LOBS's publisher. If LOBS fails to start twice on a new engine, it goes back to the previous one. | Checks GitHub daily. Updates install only if the checksum and signature match. If a new engine fails to start twice, LOBS goes back to the previous one. | Shorter; same guarantees. |

### Setup wizard

| Where | Old | New | Why |
|---|---|---|---|
| SetupWizard.qml:172 | A few questions and a quick test to set up streaming and recording. You can change it all later in Settings. | Set up streaming and recording in a few steps. You can change everything later in Settings. | Plainer. |
| SetupWizard.qml:175 | Just record | Record only | "Just". |
| SetupWizard.qml:280 | Opens your dashboard in your browser | Opens your dashboard | Restates. |
| SetupWizard.qml:323 | LOBS can't test X without going live, so tell it your upload speed. A speed test site shows it as "Upload". | LOBS can't test X without going live. Enter your upload speed from any speed test. | Shorter. |
| SetupWizard.qml:362 | Add your stream key first (previous step). | Add your stream key in the previous step. | Parenthetical. |
| SetupWizard.qml:384 | How recordings are sized and how smooth they are. | Resolution and frame rate for recordings. | Plainer. |
| SetupWizard.qml:389 | Your upload is too slow for a stable stream. A wired connection helps, or record instead. | Your upload is too slow for a stable stream. Try a wired connection, or record only. | Actionable. |
| SetupWizard.qml:466 | Twitch panels next to your scenes. | Add Twitch panels next to your scenes. | Fragment. |

### Transitions, Twitch, docks

| Where | Old | New | Why |
|---|---|---|---|
| TransitionsDialog.qml:133 | Show plays this transition when it switches scenes | Used when Show switches scenes | Shorter. |
| TwitchPanel.qml:32 | Open in your browser | Open in browser | Shorter. |
| DockTabBar.qml:158 | Drag to move · double-click to float | Drag to move. Double-click to float. | Middle dot. |

### Stats help

| Where | Old | New | Why |
|---|---|---|---|
| Main.qml:1783 | Orange: more than 1%, worth a look. | Orange: over 1%. | Chatty. |

### Updater and engine messages (C++)

| Where | Old | New | Why |
|---|---|---|---|
| app_updater.cpp:51 | the download isn't validly signed (0x%1) | the download isn't signed correctly (0x%1) | Odd word. |
| engine.cpp:74 | libobs failed to start | The OBS engine failed to start | Internal name. |
| engine.cpp:98, 103 | Video init failed (%1) / Audio init failed | Couldn't start video (%1) / Couldn't start audio | Jargon. |
| engine_manager.cpp:417 | OBS %1 didn't pass LOBS's compatibility check, so LOBS stays on %2 until a LOBS update supports it. | OBS %1 failed the compatibility check. LOBS stays on %2 until an update supports it. | Comma chain. |
| engine_manager.cpp:420 | OBS %1 didn't start on this PC earlier, so it isn't offered again. | OBS %1 failed to start on this PC before, so it's no longer offered. | Plainer. |
| engine_manager.cpp:506 | OBS %1 isn't compatible with this LOBS yet (%2), so LOBS stays on %3. | OBS %1 isn't compatible with this version of LOBS (%2). Staying on %3. | Plainer. |
| engine_manager.cpp:510 | it no longer has %1 libobs function%2 LOBS uses: %3 | it's missing %1 function%2 LOBS uses: %3 | Internal name. |
| engine_manager.cpp:512 | Testing OBS %1 with LOBS... | Testing OBS %1… | Ellipsis; restates. |
| engine_output.cpp:23 | Windows Security → Controlled folder access can also block it: allow LOBS there, or choose another folder. | Controlled folder access in Windows Security may also block it. Allow LOBS there or choose another folder. | Colon aside. |

## Kept on purpose

- `"Label  Ctrl X"` and `"Label    Ctrl X"` shortcut spacing in menus (chrome.keys and tests depend on it). "Del" in the shortcut column is the standard Windows abbreviation.
- "Pick from screen (Esc cancels)", "Name (optional)", "Starting the preview…", "No sound output found", "Follows layer", "In the mixer (stream and recording)", "Monitor off": fine as is, and tests match them.
- "Show" means both the document and the scene-switch button. Changing either is a UI decision, not a copy fix.
- Import-summary fragments in app.cpp:2320–2322 are matched by `s.contains()` against the importer's output; not touched.
- Crash-report issue body (crash_report.cpp) reads fine.

## Notes: text that's long because the UI is unclear

- **Crop button tooltip** (Main.qml:1981) has to explain how crop works because nothing on the canvas shows it. On-canvas handles or a hint would let this be "Crop".
- **Pixel-perfect zoom** (CanvasArea.qml:70–72) uses a percentage button as a mode toggle; the tooltip explains the mode. A labeled toggle would need no explanation.
- **Live banner** (CanvasArea.qml:34) and four live-lock toasts all explain the same lock. A lock icon on locked layers would carry most of it.
- **Monitoring states** (MixerView.qml:56 plus four `warn` lines) explain three states with one icon. A visible state label in the strip would cut the tooltips.

## Appendix: full inventory

Every string literal from QML and C++ sources (excluding self-test code in main.cpp and test_runner.cpp), as `file:line: text`. Concatenated strings appear as fragments.

```
native/qml/AssetMouse.qml:41: Move to 
native/qml/AudioSettings.qml:14: Audio
native/qml/AudioSettings.qml:15: Global
native/qml/AudioSettings.qml:15: Scene
native/qml/AudioSettings.qml:19: Unpin
native/qml/AudioSettings.qml:19: Pin to the front of the mixer
native/qml/AudioSettings.qml:25: Volume
native/qml/AudioSettings.qml:33: Monitor off
native/qml/AudioSettings.qml:33: Monitor and output
native/qml/AudioSettings.qml:33: Monitor only (not in output)
native/qml/AudioSettings.qml:39: Balance
native/qml/AudioSettings.qml:53: Sync offset (ms)
native/qml/AudioSettings.qml:58: Mono
native/qml/AudioSettings.qml:68: Audio is delayed 
native/qml/AudioSettings.qml:68:  ms to line up with slower video.
native/qml/AudioSettings.qml:68: Audio plays 
native/qml/AudioSettings.qml:68:  ms early.
native/qml/AudioSettings.qml:69: Mono: both channels carry the same mix (useful for a mic that only comes in on one side).
native/qml/AudioSettings.qml:74: Tracks
native/qml/AudioSettings.qml:84: Track 
native/qml/AudioSettings.qml:84:  (stream)
native/qml/AudioSettings.qml:100: Effects
native/qml/AudioSettings.qml:103: Effect
native/qml/AudioSettings.qml:104: Add an audio effect
native/qml/AudioSettings.qml:139: Earlier in the chain
native/qml/AudioSettings.qml:140: Later in the chain
native/qml/AudioSettings.qml:141: Remove
native/qml/AudioSettings.qml:161: Imported from OBS. No controls in LOBS yet.
native/qml/AudioSettings.qml:170: Applies to this source in every scene.
native/qml/BrowserOptions.qml:27:  fps
native/qml/BrowserOptions.qml:28: shuts down when hidden
native/qml/BrowserOptions.qml:29: reloads on live
native/qml/BrowserOptions.qml:30: no access
native/qml/BrowserOptions.qml:30: reads your info
native/qml/BrowserOptions.qml:30: basic control
native/qml/BrowserOptions.qml:30: advanced control
native/qml/BrowserOptions.qml:30: full control
native/qml/BrowserOptions.qml:40: File
native/qml/BrowserOptions.qml:56: Reload the page, skipping the cache
native/qml/BrowserOptions.qml:63: Choose an .html file
native/qml/BrowserOptions.qml:67: Browse…
native/qml/BrowserOptions.qml:68: Reload the page
native/qml/BrowserOptions.qml:72: Web pages (*.html *.htm)
native/qml/BrowserOptions.qml:72: All files (*)
native/qml/BrowserOptions.qml:79: Size
native/qml/BrowserOptions.qml:83: Canvas
native/qml/BrowserOptions.qml:92: Canvas
native/qml/BrowserOptions.qml:99: Follows layer
native/qml/BrowserOptions.qml:99: Fixed
native/qml/BrowserOptions.qml:101: Resizing the layer resizes the page, like a browser window. Click to set a fixed size.
native/qml/BrowserOptions.qml:102: The page keeps this size and the layer scales it. Click to have the page follow the layer.
native/qml/BrowserOptions.qml:115: Advanced
native/qml/BrowserOptions.qml:120: Advanced
native/qml/BrowserOptions.qml:123: Defaults
native/qml/BrowserOptions.qml:133: Sound
native/qml/BrowserOptions.qml:136: In the mixer (stream and recording)
native/qml/BrowserOptions.qml:136: Your speakers only
native/qml/BrowserOptions.qml:140: Frame rate
native/qml/BrowserOptions.qml:145: Match the canvas
native/qml/BrowserOptions.qml:145: 30 fps
native/qml/BrowserOptions.qml:145: 60 fps
native/qml/BrowserOptions.qml:145: Custom…
native/qml/BrowserOptions.qml:152:  fps
native/qml/BrowserOptions.qml:157: When hidden
native/qml/BrowserOptions.qml:160: Keep running
native/qml/BrowserOptions.qml:160: Shut down (reloads when shown)
native/qml/BrowserOptions.qml:164: When live
native/qml/BrowserOptions.qml:167: Do nothing
native/qml/BrowserOptions.qml:167: Reload the page
native/qml/BrowserOptions.qml:171: Page access
native/qml/BrowserOptions.qml:175: None
native/qml/BrowserOptions.qml:176: Read LOBS status
native/qml/BrowserOptions.qml:177: Read your info
native/qml/BrowserOptions.qml:178: Basic control of LOBS
native/qml/BrowserOptions.qml:179: Advanced control of LOBS
native/qml/BrowserOptions.qml:180: Full control of LOBS
native/qml/BrowserOptions.qml:190: Custom (
native/qml/BrowserOptions.qml:190:  line)
native/qml/BrowserOptions.qml:190:  lines)
native/qml/BrowserOptions.qml:190: Default
native/qml/BrowserOptions.qml:193: Edit the page's CSS
native/qml/BrowserOptions.qml:196: Reset
native/qml/BrowserOptions.qml:196: Back to the default: a transparent page
native/qml/BrowserOptions.qml:214: Default: a transparent page with no margins or scrollbars
native/qml/BrowserOptions.qml:227: Refresh cache
native/qml/CanvasArea.qml:34: Live
native/qml/CanvasArea.qml:34: Recording
native/qml/CanvasArea.qml:34:  · editing what's on air is locked. Hold Alt, or unlock to edit.
native/qml/CanvasArea.qml:34:  · live lock is off, edits go on air.
native/qml/CanvasArea.qml:36: Unlock
native/qml/CanvasArea.qml:36: Lock again
native/qml/CanvasArea.qml:47: No scene
native/qml/CanvasArea.qml:48: On air
native/qml/CanvasArea.qml:48: Showing
native/qml/CanvasArea.qml:51: Canvas
native/qml/CanvasArea.qml:53: Move: drag a layer. Snaps to edges and layers.
native/qml/CanvasArea.qml:54: Resize: drag a corner. Shift: free resize.
native/qml/CanvasArea.qml:55: Ctrl: drag without snapping.
native/qml/CanvasArea.qml:56: Groups: click selects the group. Each double-click goes one level deeper.
native/qml/CanvasArea.qml:57: Nudge: arrow keys move 1 px, Shift+arrows 10 px.
native/qml/CanvasArea.qml:58: Live lock: hold Alt to move layers while live.
native/qml/CanvasArea.qml:59: Show: switch to this scene.
native/qml/CanvasArea.qml:70: Pixel-perfect: each canvas pixel lands exactly on screen pixels, so text is crisp. Click to fit the panel exactly instead.
native/qml/CanvasArea.qml:71: Pixel-perfect is on, but no exact ratio fits well at this panel size, so it fits the panel. Make the panel a little larger to snap.
native/qml/CanvasArea.qml:72: Fits the panel exactly (may soften text). Click for pixel-perfect scaling.
native/qml/CanvasArea.qml:79: Transition
native/qml/CanvasArea.qml:81: How Show switches scenes
native/qml/CanvasArea.qml:98: Edit transitions…
native/qml/CanvasArea.qml:101: Show
native/qml/CanvasArea.qml:101: Switch to this scene
native/qml/CanvasArea.qml:157: Open projector
native/qml/CanvasArea.qml:166: New window
native/qml/CanvasArea.qml:170: Open scene projector
native/qml/CanvasArea.qml:179: New window
native/qml/CaptionsPanel.qml:39: Downloading the speech model… 
native/qml/CaptionsPanel.qml:41: Captions run on this PC, so your audio never leaves it. They need a one-time 142 MB download.
native/qml/CaptionsPanel.qml:45: Try again
native/qml/CaptionsPanel.qml:45: Download speech model
native/qml/CaptionsPanel.qml:58: Audio source…
native/qml/CaptionsPanel.qml:64: Stop
native/qml/CaptionsPanel.qml:64: Start
native/qml/CaptionsPanel.qml:82: Send to stream
native/qml/CaptionsPanel.qml:117: Listening. Text shows up a few seconds after speech.
native/qml/CaptionsPanel.qml:117: Pick a mic or desktop audio, then Start.
native/qml/CaptionsPanel.qml:129: Captions are off
native/qml/CaptionsPanel.qml:130: CC (
native/qml/CaptionsPanel.qml:130:  for Stream
native/qml/CaptionsPanel.qml:130:  · sent once you go live
native/qml/CaptionsPanel.qml:130:  · not sent to the stream
native/qml/CaptionsPanel.qml:132: Clear the transcript
native/qml/FColorField.qml:11: Color 
native/qml/FColorField.qml:27: Color
native/qml/FColorField.qml:211: Pick from screen (Esc cancels)
native/qml/FColorField.qml:225: Swatches
native/qml/FColorField.qml:244: Already a swatch
native/qml/FColorField.qml:244: Save 
native/qml/FColorField.qml:244:  as a swatch. Right-click a swatch to remove it.
native/qml/FColorField.qml:250: Recent
native/qml/FCopyToMenu.qml:6: Copy to
native/qml/FCopyToMenu.qml:20: Linked copy
native/qml/FCopyToMenu.qml:21: Independent copy
native/qml/FFillSlider.qml:61: Slider
native/qml/FFontPicker.qml:14: Choose font…
native/qml/FFontPicker.qml:79: Search fonts
native/qml/FFontPicker.qml:93: Scroll
native/qml/FFontPicker.qml:99: Recently used
native/qml/FFontPicker.qml:99: All fonts
native/qml/FFontPicker.qml:132: No fonts match
native/qml/FHelp.qml:10: Key: text
native/qml/FHelp.qml:67: <font color='
native/qml/FHelp.qml:67: </b></font> 
native/qml/FNumField.qml:9: Number
native/qml/FNumField.qml:62: Number
native/qml/FScrollView.qml:9: Scroll area
native/qml/FScrollView.qml:23: Scroll
native/qml/FSearchField.qml:10: Search
native/qml/FSearchField.qml:20: Clear
native/qml/FSection.qml:37: Reset 
native/qml/FSection.qml:46: Linked · 
native/qml/FSection.qml:46:  scenes
native/qml/FSection.qml:48: Shared with other scenes
native/qml/FSection.qml:55: Shared with 
native/qml/FSection.qml:60: Unlink in this scene
native/qml/FSection.qml:69: Link 
native/qml/FSection.qml:69:  with this source in another scene
native/qml/FSection.qml:79: Link with 
native/qml/FSection.qml:82: Keep this scene's values
native/qml/FSection.qml:83: Use 
native/qml/FSection.qml:83: 's values
native/qml/FSelect.qml:9: Choose…
native/qml/FSwitch.qml:15: Switch
native/qml/FluidList.qml:9: List
native/qml/GraphicTools.qml:16: Select and move layers
native/qml/GraphicTools.qml:17: Text: click or drag a box
native/qml/GraphicTools.qml:18: Rectangle: drag (Shift for a square)
native/qml/GraphicTools.qml:19: Ellipse: drag (Shift for a circle)
native/qml/GraphicTools.qml:20: Arrow (Shift snaps to 45°)
native/qml/GraphicTools.qml:21: Pencil
native/qml/GraphicTools.qml:22: Laser pointer, not added to the scene
native/qml/Main.qml:36: Ctrl+,
native/qml/Main.qml:41: Ctrl+Y
native/qml/Main.qml:42: Ctrl+D
native/qml/Main.qml:45: Escape
native/qml/Main.qml:46: Return
native/qml/Main.qml:46: Enter
native/qml/Main.qml:55: Ctrl+Shift+V
native/qml/Main.qml:56: Ctrl+G
native/qml/Main.qml:57: Ctrl+Shift+G
native/qml/Main.qml:59: Ctrl+]
native/qml/Main.qml:60: Ctrl+[
native/qml/Main.qml:61: Ctrl+Shift+]
native/qml/Main.qml:61: Ctrl+}
native/qml/Main.qml:62: Ctrl+Shift+[
native/qml/Main.qml:62: Ctrl+{
native/qml/Main.qml:63: Ctrl+S
native/qml/Main.qml:64: Ctrl+O
native/qml/Main.qml:66: Delete
native/qml/Main.qml:66: Backspace
native/qml/Main.qml:66: Delete
native/qml/Main.qml:81: File
native/qml/Main.qml:82: New Project…
native/qml/Main.qml:82: Ctrl+N
native/qml/Main.qml:83: Open…
native/qml/Main.qml:83: Ctrl+O
native/qml/Main.qml:86: Open Recent
native/qml/Main.qml:99: Clear Menu
native/qml/Main.qml:103: Import from OBS
native/qml/Main.qml:111:  scenes
native/qml/Main.qml:117: No OBS scene collections found
native/qml/Main.qml:119: Choose a File…
native/qml/Main.qml:122: Save
native/qml/Main.qml:122: Ctrl+S
native/qml/Main.qml:123: Save As…
native/qml/Main.qml:123: Ctrl+Shift+S
native/qml/Main.qml:125: About LOBS
native/qml/Main.qml:126: Settings…
native/qml/Main.qml:126: Ctrl+,
native/qml/Main.qml:129: Edit
native/qml/Main.qml:130: Undo 
native/qml/Main.qml:130: Undo
native/qml/Main.qml:131: Redo
native/qml/Main.qml:134: Copy
native/qml/Main.qml:135: Paste
native/qml/Main.qml:136: Paste Linked
native/qml/Main.qml:136: Ctrl+Shift+V
native/qml/Main.qml:137: Duplicate
native/qml/Main.qml:137: Ctrl+D
native/qml/Main.qml:138: Delete
native/qml/Main.qml:140: Group
native/qml/Main.qml:140: Ctrl+G
native/qml/Main.qml:141: Ungroup
native/qml/Main.qml:141: Ctrl+Shift+G
native/qml/Main.qml:143: Bring to Front
native/qml/Main.qml:143: Ctrl+Shift+]
native/qml/Main.qml:144: Bring Forward
native/qml/Main.qml:144: Ctrl+]
native/qml/Main.qml:145: Send Backward
native/qml/Main.qml:145: Ctrl+[
native/qml/Main.qml:146: Send to Back
native/qml/Main.qml:146: Ctrl+Shift+[
native/qml/Main.qml:150: Window
native/qml/Main.qml:151: Minimize
native/qml/Main.qml:151: Ctrl+M
native/qml/Main.qml:152: Zoom
native/qml/Main.qml:154: Exit Full Screen
native/qml/Main.qml:154: Enter Full Screen
native/qml/Main.qml:155: Meta+Ctrl+F
native/qml/Main.qml:172: Show All Panels
native/qml/Main.qml:173: Reset Layout
native/qml/Main.qml:176: Help
native/qml/Main.qml:177: LOBS on GitHub
native/qml/Main.qml:178: Report a Problem…
native/qml/Main.qml:287: LOBS mixer preview
native/qml/Main.qml:315: Settings
native/qml/Main.qml:325: Settings  Ctrl ,
native/qml/Main.qml:350: Start a new project in its own folder
native/qml/Main.qml:351: Open
native/qml/Main.qml:351: Stop streaming and recording to open another show
native/qml/Main.qml:351: Open a show (Ctrl+O)
native/qml/Main.qml:355: Import
native/qml/Main.qml:358: Stop streaming and recording to import a collection
native/qml/Main.qml:358: Import scenes and sources from OBS
native/qml/Main.qml:369: OBS: 
native/qml/Main.qml:369:  scenes
native/qml/Main.qml:375: No OBS scene collections found
native/qml/Main.qml:377: Choose a file...
native/qml/Main.qml:380: Save
native/qml/Main.qml:380: Save (Ctrl+S)
native/qml/Main.qml:384: LOBS 
native/qml/Main.qml:385: Install LOBS 
native/qml/Main.qml:386: LOBS 
native/qml/Main.qml:386:  available
native/qml/Main.qml:388: A new version of LOBS
native/qml/Main.qml:394: Window
native/qml/Main.qml:413: Show all panels
native/qml/Main.qml:414: Reset layout
native/qml/Main.qml:418: Undo 
native/qml/Main.qml:418:   Ctrl Z
native/qml/Main.qml:419: Redo  Ctrl Shift Z
native/qml/Main.qml:428: Minimize
native/qml/Main.qml:429: Restore
native/qml/Main.qml:429: Maximize
native/qml/Main.qml:430: Close
native/qml/Main.qml:471: Video
native/qml/Main.qml:479: Scenes
native/qml/Main.qml:491: List
native/qml/Main.qml:492: Grid with live thumbnails
native/qml/Main.qml:494: Scene
native/qml/Main.qml:494: Add a scene
native/qml/Main.qml:501: Show this scene
native/qml/Main.qml:502: Rename…
native/qml/Main.qml:504: Show in 
native/qml/Main.qml:505: Duplicate with linked layers
native/qml/Main.qml:506: Duplicate
native/qml/Main.qml:508: Delete scene
native/qml/Main.qml:526: Scene
native/qml/Main.qml:545: On air
native/qml/Main.qml:545: Showing
native/qml/Main.qml:553: Scenes
native/qml/Main.qml:598: On air
native/qml/Main.qml:598: Showing
native/qml/Main.qml:602: Scene
native/qml/Main.qml:622: Sources
native/qml/Main.qml:636: Video
native/qml/Main.qml:636: Audio
native/qml/Main.qml:640: Search sources
native/qml/Main.qml:643: Source
native/qml/Main.qml:643: Add a source
native/qml/Main.qml:651: Search by name or device
native/qml/Main.qml:774: Muted. Click to unmute.
native/qml/Main.qml:774: Mute
native/qml/Main.qml:809: Remove
native/qml/Main.qml:809: In scene
native/qml/Main.qml:809: Add to scene
native/qml/Main.qml:818: Remove from 
native/qml/Main.qml:818: Add to 
native/qml/Main.qml:825: More
native/qml/Main.qml:833: Source
native/qml/Main.qml:868: Remove from 
native/qml/Main.qml:868: Add to 
native/qml/Main.qml:871: Settings…
native/qml/Main.qml:872: Rename…
native/qml/Main.qml:876: Plays in every scene
native/qml/Main.qml:877: Used in 
native/qml/Main.qml:877: Not used in any scene yet
native/qml/Main.qml:881: Delete source
native/qml/Main.qml:890: No sources match /u201c
native/qml/Main.qml:890: No audio inputs yet.
native/qml/Main.qml:890: No video inputs yet.
native/qml/Main.qml:890: Add each camera, capture or mic once,/nthen drag it onto any scene.
native/qml/Main.qml:900: Assets
native/qml/Main.qml:939: New folder
native/qml/Main.qml:959: 1920×1080 · 2.1 MB
native/qml/Main.qml:959: 0:12 · 1280×720 · 8 MB
native/qml/Main.qml:986: Add files to assets
native/qml/Main.qml:990: Find the missing file
native/qml/Main.qml:1008: Name
native/qml/Main.qml:1008: Date
native/qml/Main.qml:1008: Size
native/qml/Main.qml:1009: Sort
native/qml/Main.qml:1013: Name
native/qml/Main.qml:1014: Date modified
native/qml/Main.qml:1015: Size
native/qml/Main.qml:1017: Ascending
native/qml/Main.qml:1017: Descending
native/qml/Main.qml:1020: Grid
native/qml/Main.qml:1021: List
native/qml/Main.qml:1026: Search all folders  Ctrl F
native/qml/Main.qml:1031: More
native/qml/Main.qml:1034: Add files
native/qml/Main.qml:1043: Up one folder  Backspace
native/qml/Main.qml:1071: Show this folder in File Explorer
native/qml/Main.qml:1079: Search all folders
native/qml/Main.qml:1100:  is missing (moved or deleted outside LOBS).
native/qml/Main.qml:1101:  files are missing (moved or deleted outside LOBS).
native/qml/Main.qml:1103: Find it
native/qml/Main.qml:1103: Search assets folders by file name
native/qml/Main.qml:1105: Choose file…
native/qml/Main.qml:1106: Point 
native/qml/Main.qml:1106:  at a file
native/qml/Main.qml:1134: Drop files here, or click to add them
native/qml/Main.qml:1181: Assets
native/qml/Main.qml:1197: Assets
native/qml/Main.qml:1208: Scroll
native/qml/Main.qml:1348: Nothing in assets matches /u201c
native/qml/Main.qml:1349: audio files
native/qml/Main.qml:1349:  here.
native/qml/Main.qml:1350: This folder is empty. Drag files here, or paste with Ctrl+V.
native/qml/Main.qml:1351: Drop images, video or audio here.
native/qml/Main.qml:1366: Drop to copy into 
native/qml/Main.qml:1373: Open
native/qml/Main.qml:1374: Place on 
native/qml/Main.qml:1375: Rename…  F2
native/qml/Main.qml:1404: Copy 
native/qml/Main.qml:1404:  items
native/qml/Main.qml:1404: Copy
native/qml/Main.qml:1404:   Ctrl C
native/qml/Main.qml:1405: Cut 
native/qml/Main.qml:1405:  items
native/qml/Main.qml:1405:   Ctrl X
native/qml/Main.qml:1406: Paste here  Ctrl V
native/qml/Main.qml:1408: Show in File Explorer
native/qml/Main.qml:1410: In use: remove its layers first
native/qml/Main.qml:1410: Move to Recycle Bin  Del
native/qml/Main.qml:1417: New folder  Ctrl N
native/qml/Main.qml:1418: Paste  Ctrl V
native/qml/Main.qml:1419: Add files…
native/qml/Main.qml:1421: Show folder in File Explorer
native/qml/Main.qml:1431: Layers
native/qml/Main.qml:1443: Group  Ctrl G
native/qml/Main.qml:1444: Copy to another scene
native/qml/Main.qml:1445: Delete  Del
native/qml/Main.qml:1455: Linked copy
native/qml/Main.qml:1456: Independent copy
native/qml/Main.qml:1467: Rename…
native/qml/Main.qml:1468: Copy    Ctrl C
native/qml/Main.qml:1469: Cut    Ctrl X
native/qml/Main.qml:1470: Paste    Ctrl V
native/qml/Main.qml:1471: Paste linked    Ctrl Shift V
native/qml/Main.qml:1473: Duplicate
native/qml/Main.qml:1475: Group selection
native/qml/Main.qml:1476: Ungroup
native/qml/Main.qml:1478: Bring to front    Ctrl Shift ]
native/qml/Main.qml:1479: Bring forward    Ctrl ]
native/qml/Main.qml:1480: Send backward    Ctrl [
native/qml/Main.qml:1481: Send to back    Ctrl Shift [
native/qml/Main.qml:1483: Delete
native/qml/Main.qml:1559: New group
native/qml/Main.qml:1559: Move into group
native/qml/Main.qml:1576: Layer
native/qml/Main.qml:1615: Show the group's layers
native/qml/Main.qml:1615: Hide the group's layers
native/qml/Main.qml:1632: Linked with another scene
native/qml/Main.qml:1638: Move up
native/qml/Main.qml:1639: Move down
native/qml/Main.qml:1640: Unlock
native/qml/Main.qml:1640: Lock
native/qml/Main.qml:1642: Unlock
native/qml/Main.qml:1643: Hide
native/qml/Main.qml:1643: Show
native/qml/Main.qml:1650: Empty scene./nDrag a source from Sources onto the canvas.
native/qml/Main.qml:1660: Inspector
native/qml/Main.qml:1667: History
native/qml/Main.qml:1675: Chat
native/qml/Main.qml:1686: Stream info
native/qml/Main.qml:1698: Audio Mixer
native/qml/Main.qml:1705: Stats
native/qml/Main.qml:1723: Captions
native/qml/Main.qml:1776: Click: edit a scene without showing it.
native/qml/Main.qml:1776: Show or double-click: switch to the scene.
native/qml/Main.qml:1776: Grid: shows live thumbnails of every scene.
native/qml/Main.qml:1776: Right-click: rename, duplicate with linked or independent layers, or delete.
native/qml/Main.qml:1777: Inputs: added once, shared by every scene.
native/qml/Main.qml:1777: Place it: drag onto the canvas or double-click.
native/qml/Main.qml:1777: Per scene: size, crop and filters.
native/qml/Main.qml:1777: Files are in Assets. Add text and color from Layers.
native/qml/Main.qml:1778: Project files: this project's assets folder.
native/qml/Main.qml:1778: Add files: drag in from File Explorer or use + Asset.
native/qml/Main.qml:1778: Place it: drag onto the canvas or double-click.
native/qml/Main.qml:1778: Right-click: show in File Explorer or move to Recycle Bin.
native/qml/Main.qml:1779: Order: layers higher in the list draw on top.
native/qml/Main.qml:1779: Linked: settings shared with another scene.
native/qml/Main.qml:1779: Shortcuts: Ctrl+C / Ctrl+V copy and paste (Ctrl+Shift+V pastes linked), Ctrl+D duplicate, Ctrl+G group, Delete remove.
native/qml/Main.qml:1779: Copy to: copy layers to another scene.
native/qml/Main.qml:1780: This scene: layer changes apply here only.
native/qml/Main.qml:1780: Linked: shared with another scene. Unlink to separate.
native/qml/Main.qml:1780: Numbers: math like 1920/2 works. Up/Down nudge, Shift for 10.
native/qml/Main.qml:1780: Source: select one in Sources to edit its input.
native/qml/Main.qml:1781: Global: Mic and Desktop Audio play in every scene.
native/qml/Main.qml:1781: Scene audio: plays only in scenes that contain it.
native/qml/Main.qml:1781: Tracks: track 1 goes to the stream; recordings can use tracks 1-6.
native/qml/Main.qml:1781: Monitor: hear it in headphones. Monitor only keeps it out of output.
native/qml/Main.qml:1781: Fader: set the level. Meter shows peak dB.
native/qml/Main.qml:1782: Undo: Ctrl+Z; redo: Ctrl+Shift+Z or Ctrl+Y.
native/qml/Main.qml:1782: Jump: click a step to go back to it.
native/qml/Main.qml:1783: Frames missed: the PC couldn't render in time.
native/qml/Main.qml:1783: Frames skipped: the encoder fell behind.
native/qml/Main.qml:1783: Dropped frames: the network couldn't keep up.
native/qml/Main.qml:1783: Orange: more than 1%, worth a look.
native/qml/Main.qml:1784: Speech to text from one audio source, on this PC.
native/qml/Main.qml:1784: Send to stream: viewers turn on CC to see them.
native/qml/Main.qml:1784: Language: pick it, or let LOBS detect it.
native/qml/Main.qml:1784: Text shows up a few seconds after speech.
native/qml/Main.qml:1827: Text
native/qml/Main.qml:1827: Rectangle
native/qml/Main.qml:1827: Ellipse
native/qml/Main.qml:1827: Arrow
native/qml/Main.qml:1827: Pencil
native/qml/Main.qml:1827: Laser pointer
native/qml/Main.qml:1835: A fading trail. Nothing is added to the scene.
native/qml/Main.qml:1836: Drag up or down on the canvas to set the size, then type. Esc or a click elsewhere finishes.
native/qml/Main.qml:1837: Used for the next one you draw. Select a shape to change its own.
native/qml/Main.qml:1843: Bold
native/qml/Main.qml:1844: Italic
native/qml/Main.qml:1849: Color
native/qml/Main.qml:1849: Fill
native/qml/Main.qml:1851: None
native/qml/Main.qml:1851: No fill (outline only)
native/qml/Main.qml:1856: Color
native/qml/Main.qml:1856: Stroke
native/qml/Main.qml:1862: Trail length
native/qml/Main.qml:1871: Thickness
native/qml/Main.qml:1871: Stroke width
native/qml/Main.qml:1885: Select a layer or source to edit it.
native/qml/Main.qml:1898:  layers
native/qml/Main.qml:1911: Transform
native/qml/Main.qml:1923: Layer Fit
native/qml/Main.qml:1924: Fit inside the canvas, keeping proportions
native/qml/Main.qml:1925: Fill
native/qml/Main.qml:1925: Cover the whole canvas, keeping proportions
native/qml/Main.qml:1926: Original
native/qml/Main.qml:1926: Restore the source's own proportions
native/qml/Main.qml:1931: Position
native/qml/Main.qml:1934: Center on the canvas
native/qml/Main.qml:1938: Size
native/qml/Main.qml:1945: W and H keep their proportions. Click to change them separately.
native/qml/Main.qml:1945: W and H change separately. Click to keep proportions.
native/qml/Main.qml:1952: Rotate
native/qml/Main.qml:1954: Mirror left to right
native/qml/Main.qml:1955: Mirror top to bottom
native/qml/Main.qml:1971: Crop & corners
native/qml/Main.qml:1971: Corners
native/qml/Main.qml:1975: Crop
native/qml/Main.qml:1979: Done
native/qml/Main.qml:1979: Crop
native/qml/Main.qml:1981: Finish cropping (Enter). Esc puts it back.
native/qml/Main.qml:1981: Drag the edges on the canvas to crop, or inside to move the crop. Double-click a layer to start.
native/qml/Main.qml:1987: Hide exact crop values
native/qml/Main.qml:1987: Exact crop values
native/qml/Main.qml:1996: Aspect
native/qml/Main.qml:1997: Free
native/qml/Main.qml:2008: Remove the crop
native/qml/Main.qml:2014: Corners
native/qml/Main.qml:2020: Square
native/qml/Main.qml:2020: Round
native/qml/Main.qml:2029: Filters
native/qml/Main.qml:2065: Remove filter
native/qml/Main.qml:2130: Choose 
native/qml/Main.qml:2146: Add filter…
native/qml/Main.qml:2156: All files (*)
native/qml/Main.qml:2165: Visibility
native/qml/Main.qml:2166: Visible
native/qml/Main.qml:2169: Opacity
native/qml/Main.qml:2186:  · input settings apply to every scene that uses it. Size, crop and filters are set per layer.
native/qml/Main.qml:2195: Used in
native/qml/Main.qml:2200: Not on any scene yet — drag it onto the canvas.
native/qml/Main.qml:2200: Audio source — always mixed in.
native/qml/Main.qml:2220: History step
native/qml/Main.qml:2236: Add a source
native/qml/Main.qml:2271:   ·  in use
native/qml/Main.qml:2314: Each input is added once and shared by every scene.
native/qml/Main.qml:2318: Camera
native/qml/Main.qml:2318: Display
native/qml/Main.qml:2318: Window
native/qml/Main.qml:2318: Game
native/qml/Main.qml:2319: Browser
native/qml/Main.qml:2319: Sound
native/qml/Main.qml:2350: sound output
native/qml/Main.qml:2350:  found
native/qml/Main.qml:2376: Another app is using this camera. Close it there to use it here.
native/qml/Main.qml:2381: Already in your show, so this is its feed. A camera opens in one place at a time.
native/qml/Main.qml:2388: Enter a URL or pick a file to see the page
native/qml/Main.qml:2388: Starting the preview…
native/qml/Main.qml:2388: Nothing to preview
native/qml/Main.qml:2393: Reload the page
native/qml/Main.qml:2420: Say something
native/qml/Main.qml:2420: Play something
native/qml/Main.qml:2421: Mono
native/qml/Main.qml:2434: Choose the window to capture
native/qml/Main.qml:2435: List the open windows again
native/qml/Main.qml:2449: Allow Screen Recording to list your windows.
native/qml/Main.qml:2449: No windows to capture.
native/qml/Main.qml:2486: Name (optional)
native/qml/Main.qml:2491: Text
native/qml/Main.qml:2491: File path
native/qml/Main.qml:2494: Also place on 
native/qml/Main.qml:2499: Cancel
native/qml/Main.qml:2500: Add source
native/qml/Main.qml:2511: Rename
native/qml/Main.qml:2527: Cancel
native/qml/Main.qml:2528: Rename
native/qml/Main.qml:2535: Install LOBS 
native/qml/Main.qml:2535: LOBS 
native/qml/Main.qml:2535:  is available
native/qml/Main.qml:2545: Downloading... 
native/qml/Main.qml:2546: LOBS closes, installs the update and opens again in about a minute. Your shows and settings stay as they are.
native/qml/Main.qml:2547: You have 
native/qml/Main.qml:2547: . The update downloads in the background while you keep working.
native/qml/Main.qml:2560: Stop streaming, recording, the replay buffer and the virtual camera first. LOBS won't close while they're running.
native/qml/Main.qml:2564: What's new
native/qml/Main.qml:2566: Later
native/qml/Main.qml:2568: Restart and install
native/qml/Main.qml:2568: Download
native/qml/Main.qml:2580: LOBS closed unexpectedly
native/qml/Main.qml:2592: Sorry about that. LOBS saved a crash report on this PC. Reporting it on GitHub helps get it fixed. The issue asks for the log, which is safe to post. Keep the .dmp file private: it can hold what LOBS had in memory, like your stream key.
native/qml/Main.qml:2596: Show files
native/qml/Main.qml:2598: Not now
native/qml/Main.qml:2599: Report on GitHub
native/qml/Main.qml:2606: Save this show first?
native/qml/Main.qml:2615: This show has never been saved. If you don't save it, its changes are lost.
native/qml/Main.qml:2620: Cancel
native/qml/Main.qml:2621: Don't save
native/qml/Main.qml:2622: Save…
native/qml/Main.qml:2630: Delete 
native/qml/Main.qml:2640: It's used in 
native/qml/Main.qml:2640:  scenes (
native/qml/Main.qml:2641: ). Deleting it removes it from all of them. You can undo this.
native/qml/Main.qml:2646: Cancel
native/qml/Main.qml:2647: Delete from all scenes
native/qml/MediaControls.qml:30: Pause
native/qml/MediaControls.qml:30: Play
native/qml/MediaControls.qml:31: Play from the start
native/qml/MediaControls.qml:32: Stop
native/qml/MixerView.qml:56: Not monitored. Click to hear it in your headphones (and keep it in the output).
native/qml/MixerView.qml:56: Monitor only: you hear it, viewers don't. Click to turn monitoring off.
native/qml/MixerView.qml:56: Monitored: you hear it, and it's in the output. Click to turn monitoring off.
native/qml/MixerView.qml:82: Mono
native/qml/MixerView.qml:106: Search audio
native/qml/MixerView.qml:112: 1 input
native/qml/MixerView.qml:112:  inputs
native/qml/MixerView.qml:117: Search
native/qml/MixerView.qml:124: Strips. Click for rows.
native/qml/MixerView.qml:124: Rows. Click for strips.
native/qml/MixerView.qml:175: Muted. Click to unmute.
native/qml/MixerView.qml:175: Mute
native/qml/MixerView.qml:177: Monitoring, tracks and effects (in the Inspector)
native/qml/MixerView.qml:204: Audio mixer
native/qml/MixerView.qml:271: Muted. Click to unmute.
native/qml/MixerView.qml:271: Mute
native/qml/MixerView.qml:273: Monitoring, tracks and effects (in the Inspector)
native/qml/MixerView.qml:287: No audio inputs match /u201c
native/qml/MixerView.qml:287: No audio sources. Add a Mic or Desktop audio from Sources.
native/qml/NewProjectDialog.qml:11: New project
native/qml/NewProjectDialog.qml:19: Untitled project
native/qml/NewProjectDialog.qml:43: Your current project is saved before the new one opens.
native/qml/NewProjectDialog.qml:44: Your current show isn't saved as a project. Save it first, or it will be replaced.
native/qml/NewProjectDialog.qml:46: Save as…
native/qml/NewProjectDialog.qml:51: Name
native/qml/NewProjectDialog.qml:56: Location
native/qml/NewProjectDialog.qml:60: Browse…
native/qml/NewProjectDialog.qml:66: Creates 
native/qml/NewProjectDialog.qml:66:  with the show file and an assets folder.
native/qml/NewProjectDialog.qml:72: Cancel
native/qml/NewProjectDialog.qml:74: Create project
native/qml/NewProjectDialog.qml:82: Where to keep the project
native/qml/OutputControls.qml:69: Replay
native/qml/OutputControls.qml:71: Replay buffer is on (last 
native/qml/OutputControls.qml:71:  s). Click to turn it off.
native/qml/OutputControls.qml:72: Keep the last 
native/qml/OutputControls.qml:72:  s so you can save a highlight
native/qml/OutputControls.qml:79: Save
native/qml/OutputControls.qml:80: Save the last 
native/qml/OutputControls.qml:80:  s to your recordings folder
native/qml/OutputControls.qml:86: Virtual cam
native/qml/OutputControls.qml:88: Other apps see the program scene as /"OBS Virtual Camera/". Click to stop.
native/qml/OutputControls.qml:89: Send the program scene to Zoom, Meet, Discord and others as /"OBS Virtual Camera/"
native/qml/OutputControls.qml:90: Set up the virtual camera (Windows asks for permission once)
native/qml/OutputControls.qml:96: Record
native/qml/OutputControls.qml:99: Recording · 
native/qml/OutputControls.qml:99:  MB. Click to stop.
native/qml/OutputControls.qml:99: Record the program scene
native/qml/OutputControls.qml:109: Stop streaming
native/qml/OutputControls.qml:109: Go live
native/qml/OutputControls.qml:147: Go live
native/qml/OutputControls.qml:148: Click again to end stream
native/qml/OutputControls.qml:149: LIVE  
native/qml/OutputControls.qml:156:  kbps
native/qml/OutputControls.qml:165:  dropped frames · click twice to end
native/qml/OutputControls.qml:165: Start streaming with your saved settings
native/qml/PermissionsBar.qml:37: Allow…
native/qml/PermissionsBar.qml:37: Reopen LOBS
native/qml/PermissionsBar.qml:37: Open System Settings
native/qml/PermissionsBar.qml:43: Reopen LOBS
native/qml/PermissionsBar.qml:49: Open System Settings
native/qml/PermissionsBar.qml:57: Hide until LOBS restarts
native/qml/SettingsPage.qml:23: Stream
native/qml/SettingsPage.qml:23: Where LOBS sends your live video.
native/qml/SettingsPage.qml:24: Recording
native/qml/SettingsPage.qml:24: Where recordings go and how they're encoded.
native/qml/SettingsPage.qml:25: Video
native/qml/SettingsPage.qml:25: Output size, frame rate and encoder.
native/qml/SettingsPage.qml:26: Audio
native/qml/SettingsPage.qml:26: Devices for your global mic and desktop audio.
native/qml/SettingsPage.qml:27: Hotkeys
native/qml/SettingsPage.qml:27: Work even while a game has focus.
native/qml/SettingsPage.qml:28: Updates
native/qml/SettingsPage.qml:28: New versions of LOBS, with the OBS engine they were tested with.
native/qml/SettingsPage.qml:29: About
native/qml/SettingsPage.qml:29: Versions, licenses and where LOBS keeps its files.
native/qml/SettingsPage.qml:104: Close settings  Esc
native/qml/SettingsPage.qml:105: Settings
native/qml/SettingsPage.qml:156: You're live. Stream and video changes apply next time you go live.
native/qml/SettingsPage.qml:167: Setup assistant
native/qml/SettingsPage.qml:168: Picks a server, bitrate and video settings.
native/qml/SettingsPage.qml:169: Run setup
native/qml/SettingsPage.qml:172: Destination
native/qml/SettingsPage.qml:176: Service
native/qml/SettingsPage.qml:177: Pick Custom for any RTMP or SRT server.
native/qml/SettingsPage.qml:179: Custom server
native/qml/SettingsPage.qml:179: Custom
native/qml/SettingsPage.qml:185: Custom
native/qml/SettingsPage.qml:186: Server
native/qml/SettingsPage.qml:187: Auto picks the closest ingest.
native/qml/SettingsPage.qml:189: Custom
native/qml/SettingsPage.qml:191: Auto
native/qml/SettingsPage.qml:196: Custom
native/qml/SettingsPage.qml:197: Server URL
native/qml/SettingsPage.qml:198: rtmp://, rtmps:// or srt://
native/qml/SettingsPage.qml:202: Stream key
native/qml/SettingsPage.qml:206: Key saved
native/qml/SettingsPage.qml:206: No key yet
native/qml/SettingsPage.qml:207: Stored encrypted.
native/qml/SettingsPage.qml:207: Find it in your platform's dashboard.
native/qml/SettingsPage.qml:211: Replace key…
native/qml/SettingsPage.qml:211: Paste stream key
native/qml/SettingsPage.qml:212: Save
native/qml/SettingsPage.qml:213: Remove saved key
native/qml/SettingsPage.qml:217: Quality
native/qml/SettingsPage.qml:221: Video bitrate
native/qml/SettingsPage.qml:222: 6000 kbps suits 1080p60 on Twitch. Services may cap it.
native/qml/SettingsPage.qml:226: Audio bitrate
native/qml/SettingsPage.qml:228:  kbps
native/qml/SettingsPage.qml:234: While live
native/qml/SettingsPage.qml:238: Live lock
native/qml/SettingsPage.qml:239: While streaming or recording, layers on air don't change by accident. Hold Alt, or click Unlock in the banner, to edit.
native/qml/SettingsPage.qml:250: Files
native/qml/SettingsPage.qml:254: Folder
native/qml/SettingsPage.qml:255: New recordings are named by date and time.
native/qml/SettingsPage.qml:259: Format
native/qml/SettingsPage.qml:260: MKV survives a crash; MP4 plays everywhere.
native/qml/SettingsPage.qml:264: Quality
native/qml/SettingsPage.qml:268: Recording quality
native/qml/SettingsPage.qml:269: Same as stream reuses the stream encoder.
native/qml/SettingsPage.qml:271: Same as stream
native/qml/SettingsPage.qml:271: High, medium files
native/qml/SettingsPage.qml:271: Indistinguishable, large files
native/qml/SettingsPage.qml:277: Replay buffer
native/qml/SettingsPage.qml:281: Keep the last
native/qml/SettingsPage.qml:282: Turn it on from the top bar. Save replay writes to your recordings folder.
native/qml/SettingsPage.qml:285: 1 min
native/qml/SettingsPage.qml:285: 2 min
native/qml/SettingsPage.qml:298: Output
native/qml/SettingsPage.qml:302: Resolution
native/qml/SettingsPage.qml:303: Your canvas stays 1920 × 1080 and is scaled to this.
native/qml/SettingsPage.qml:311: Frame rate
native/qml/SettingsPage.qml:312: 30 fps
native/qml/SettingsPage.qml:312: 60 fps
native/qml/SettingsPage.qml:315: Encoder
native/qml/SettingsPage.qml:319: Video encoder
native/qml/SettingsPage.qml:320: Hardware encoders use the GPU.
native/qml/SettingsPage.qml:323: Automatic · 
native/qml/SettingsPage.qml:329: Preset
native/qml/SettingsPage.qml:330: Quality looks best; Speed uses the least GPU.
native/qml/SettingsPage.qml:331: Quality
native/qml/SettingsPage.qml:331: Balanced
native/qml/SettingsPage.qml:331: Speed
native/qml/SettingsPage.qml:341: Global sources
native/qml/SettingsPage.qml:350: Always mixed in, in every scene.
native/qml/SettingsPage.qml:354: Default device
native/qml/SettingsPage.qml:360: Levels, monitoring and tracks are in the Audio tab.
native/qml/SettingsPage.qml:386: <b>Global hotkeys are blocked right now.</b> macOS's Secure Keyboard Entry is on, so no app gets keys typed elsewhere. Close password fields and password managers, or turn off Secure Keyboard Entry in Terminal's menu. Logging out clears it.
native/qml/SettingsPage.qml:420: Press keys
native/qml/SettingsPage.qml:420: Not set
native/qml/SettingsPage.qml:427: Click, then press keys. Backspace clears, Esc cancels.
native/qml/SettingsPage.qml:456: LOBS 
native/qml/SettingsPage.qml:457: Checking for updates...
native/qml/SettingsPage.qml:458: LOBS 
native/qml/SettingsPage.qml:458:  is available
native/qml/SettingsPage.qml:459: Downloading LOBS 
native/qml/SettingsPage.qml:460: LOBS 
native/qml/SettingsPage.qml:460:  is ready to install
native/qml/SettingsPage.qml:461: Installing...
native/qml/SettingsPage.qml:462: Up to date
native/qml/SettingsPage.qml:462:  · checked 
native/qml/SettingsPage.qml:463: Last checked 
native/qml/SettingsPage.qml:463: Not checked yet
native/qml/SettingsPage.qml:466: Update...
native/qml/SettingsPage.qml:466: Check now
native/qml/SettingsPage.qml:481: OBS engine
native/qml/SettingsPage.qml:485: OBS 
native/qml/SettingsPage.qml:486: Comes with LOBS and updates with it, so every release runs the engine it was tested with.
native/qml/SettingsPage.qml:499: Previous engine
native/qml/SettingsPage.qml:500: OBS 
native/qml/SettingsPage.qml:500:  is kept as a fallback.
native/qml/SettingsPage.qml:501: Restart LOBS
native/qml/SettingsPage.qml:501: Go back to 
native/qml/SettingsPage.qml:508: Checks LOBS's GitHub releases daily. Updates install only after the download matches its SHA-256 and is signed by LOBS's publisher. If LOBS fails to start twice on a new engine, it goes back to the previous one.
native/qml/SettingsPage.qml:519: Version
native/qml/SettingsPage.qml:519:  · Qt 6.11, C++20
native/qml/SettingsPage.qml:520: Engine
native/qml/SettingsPage.qml:520: OBS 
native/qml/SettingsPage.qml:520:  (official release build)
native/qml/SettingsPage.qml:521: Encoder in use
native/qml/SettingsPage.qml:523: Files
native/qml/SettingsPage.qml:525: Shows and settings
native/qml/SettingsPage.qml:526: Recordings
native/qml/SettingsPage.qml:528: Open source
native/qml/SettingsPage.qml:530: OBS Studio (libobs)
native/qml/SettingsPage.qml:530: GPL-2.0. LOBS is GPL-2.0-or-later.
native/qml/SettingsPage.qml:531: Inter typeface
native/qml/SettingsPage.qml:531: SIL Open Font License 1.1, by Rasmus Andersson
native/qml/SettingsPage.qml:532: Lucide icons
native/qml/SettingsPage.qml:532: ISC License
native/qml/SettingsPage.qml:533: Fluid Functionalism
native/qml/SettingsPage.qml:533: Design system the UI follows, ported to QML
native/qml/SetupWizard.qml:20: Twitch
native/qml/SetupWizard.qml:43: Twitch
native/qml/SetupWizard.qml:58: Welcome to LOBS
native/qml/SetupWizard.qml:58: Camera, mic and screen
native/qml/SetupWizard.qml:58: Your channel
native/qml/SetupWizard.qml:58: Closest server
native/qml/SetupWizard.qml:58: Test your connection
native/qml/SetupWizard.qml:59: Video and bitrate
native/qml/SetupWizard.qml:59: Recording
native/qml/SetupWizard.qml:59: Twitch panels
native/qml/SetupWizard.qml:59: You're set
native/qml/SetupWizard.qml:96: Twitch
native/qml/SetupWizard.qml:172: A few questions and a quick test to set up streaming and recording. You can change it all later in Settings.
native/qml/SetupWizard.qml:174: Stream and record
native/qml/SetupWizard.qml:174: Twitch, YouTube or another service.
native/qml/SetupWizard.qml:175: Just record
native/qml/SetupWizard.qml:175: No streaming.
native/qml/SetupWizard.qml:179: Where do you stream?
native/qml/SetupWizard.qml:183: Twitch
native/qml/SetupWizard.qml:183: Twitch
native/qml/SetupWizard.qml:183: YouTube - RTMPS
native/qml/SetupWizard.qml:183: YouTube
native/qml/SetupWizard.qml:183: Custom
native/qml/SetupWizard.qml:183: Other (RTMP)
native/qml/SetupWizard.qml:197: Allow what you'll use. You can change this later in System Settings.
native/qml/SetupWizard.qml:220: Off. Turn it on in System Settings
native/qml/SetupWizard.qml:220: , then reopen LOBS.
native/qml/SetupWizard.qml:227: Allowed
native/qml/SetupWizard.qml:231: Allow…
native/qml/SetupWizard.qml:231: Open System Settings
native/qml/SetupWizard.qml:237: Reopen LOBS
native/qml/SetupWizard.qml:252: Twitch username
native/qml/SetupWizard.qml:263: As in twitch.tv/yourname.
native/qml/SetupWizard.qml:266: Custom
native/qml/SetupWizard.qml:268: Server URL
native/qml/SetupWizard.qml:273: Stream key
native/qml/SetupWizard.qml:276: Saved. Paste a new one to replace it
native/qml/SetupWizard.qml:276: Paste your stream key
native/qml/SetupWizard.qml:279: Get my key
native/qml/SetupWizard.qml:280: Opens your dashboard in your browser
native/qml/SetupWizard.qml:284: Find it in your 
native/qml/SetupWizard.qml:284: Custom
native/qml/SetupWizard.qml:284: server settings
native/qml/SetupWizard.qml:284:  - RTMPS
native/qml/SetupWizard.qml:284:  dashboard
native/qml/SetupWizard.qml:284: . Stored encrypted.
native/qml/SetupWizard.qml:293: Sorted by response time.
native/qml/SetupWizard.qml:294: Checking…
native/qml/SetupWizard.qml:294: Check again
native/qml/SetupWizard.qml:308: Fastest
native/qml/SetupWizard.qml:309: no answer
native/qml/SetupWizard.qml:313: Checking servers…
native/qml/SetupWizard.qml:322: A 10 second test stream. Nothing goes live.
native/qml/SetupWizard.qml:323: LOBS can't test 
native/qml/SetupWizard.qml:323:  - RTMPS
native/qml/SetupWizard.qml:323:  without going live, so tell it your upload speed. A speed test site shows it as /"Upload/".
native/qml/SetupWizard.qml:336:  Mbps
native/qml/SetupWizard.qml:336: Test failed
native/qml/SetupWizard.qml:337:  Mbps
native/qml/SetupWizard.qml:342: upload measured · 
native/qml/SetupWizard.qml:342: % frames dropped
native/qml/SetupWizard.qml:343: measuring… 
native/qml/SetupWizard.qml:343: Not tested yet
native/qml/SetupWizard.qml:357: Stop
native/qml/SetupWizard.qml:357: Test again
native/qml/SetupWizard.qml:357: Start test
native/qml/SetupWizard.qml:362: Add your stream key first (previous step).
native/qml/SetupWizard.qml:369: Upload speed
native/qml/SetupWizard.qml:372:  Mbps
native/qml/SetupWizard.qml:375: If you skip it, LOBS assumes 
native/qml/SetupWizard.qml:375:  Mbps.
native/qml/SetupWizard.qml:383: Recommended for 
native/qml/SetupWizard.qml:383:  Mbps upload
native/qml/SetupWizard.qml:383: , encoding with 
native/qml/SetupWizard.qml:384: How recordings are sized and how smooth they are.
native/qml/SetupWizard.qml:389: Your upload is too slow for a stable stream. A wired connection helps, or record instead.
native/qml/SetupWizard.qml:393: No hardware encoder found. 720p is lighter on your CPU.
native/qml/SetupWizard.qml:397: Resolution
native/qml/SetupWizard.qml:407: Frame rate
native/qml/SetupWizard.qml:410: 60 fps (games, motion)
native/qml/SetupWizard.qml:410: 30 fps (talking, slides)
native/qml/SetupWizard.qml:418: Stream bitrate
native/qml/SetupWizard.qml:419: Twitch
native/qml/SetupWizard.qml:421:  Mbps
native/qml/SetupWizard.qml:426: Close to your measured upload.
native/qml/SetupWizard.qml:436: Save to
native/qml/SetupWizard.qml:438: Browse…
native/qml/SetupWizard.qml:442: Quality
native/qml/SetupWizard.qml:445: High (moderate files)
native/qml/SetupWizard.qml:445: Indistinguishable (very large files)
native/qml/SetupWizard.qml:445: Same as stream (small files)
native/qml/SetupWizard.qml:452: Format
native/qml/SetupWizard.qml:455: MP4 (plays everywhere)
native/qml/SetupWizard.qml:455: MKV (survives crashes)
native/qml/SetupWizard.qml:466: Twitch panels next to your scenes.
native/qml/SetupWizard.qml:467: Chat
native/qml/SetupWizard.qml:467: Read and reply to chat.
native/qml/SetupWizard.qml:468: Stream info
native/qml/SetupWizard.qml:468: Title, category and tags.
native/qml/SetupWizard.qml:469: Needs your channel name from the Your channel step.
native/qml/SetupWizard.qml:475: Finish to save.
native/qml/SetupWizard.qml:478: Platform
native/qml/SetupWizard.qml:479:  - RTMPS
native/qml/SetupWizard.qml:479:  · key saved
native/qml/SetupWizard.qml:479:  · no stream key yet
native/qml/SetupWizard.qml:480: Server
native/qml/SetupWizard.qml:481: Automatic
native/qml/SetupWizard.qml:482: Video
native/qml/SetupWizard.qml:483:  at 
native/qml/SetupWizard.qml:483:  fps
native/qml/SetupWizard.qml:484: Bitrate
native/qml/SetupWizard.qml:485:  Mbps
native/qml/SetupWizard.qml:486: Recording
native/qml/SetupWizard.qml:487:  quality
native/qml/SetupWizard.qml:488: Panels
native/qml/SetupWizard.qml:489: Chat
native/qml/SetupWizard.qml:489: Stream info
native/qml/SetupWizard.qml:489: None
native/qml/SetupWizard.qml:496: Skip setup
native/qml/SetupWizard.qml:499: Back
native/qml/SetupWizard.qml:501: Finish
native/qml/SetupWizard.qml:501: Skip test
native/qml/SetupWizard.qml:501: Continue
native/qml/SetupWizard.qml:503: Custom
native/qml/SourceFields.qml:18: Any fullscreen game
native/qml/SourceFields.qml:18: A specific window
native/qml/SourceFields.qml:18: Foreground window (hotkey)
native/qml/SourceFields.qml:25: Choose window…
native/qml/SourceFields.qml:25: Choose device…
native/qml/SourceFields.qml:50: Muted. Click to unmute.
native/qml/SourceFields.qml:50: Mute
native/qml/SourceFields.qml:56: Plays on your speakers, not in the stream
native/qml/SourceFields.qml:78: Where the page's sound goes
native/qml/SourceFields.qml:82: In the mixer (stream and recording)
native/qml/SourceFields.qml:83: Your speakers only
native/qml/SourceFields.qml:98: Text
native/qml/SourceFields.qml:114: Bold
native/qml/SourceFields.qml:121: Italic
native/qml/SourceFields.qml:130: Color
native/qml/SourceFields.qml:149: Fill
native/qml/SourceFields.qml:151: None
native/qml/SourceFields.qml:151: No fill (outline only)
native/qml/SourceFields.qml:157: Color
native/qml/SourceFields.qml:157: Stroke
native/qml/SourceFields.qml:161: None
native/qml/SourceFields.qml:161: No outline
native/qml/SourceFields.qml:168: Thickness
native/qml/SourceFields.qml:168: Stroke width
native/qml/SourceSection.qml:44: Source · 
native/qml/SourceSection.qml:44: Content
native/qml/SourceSection.qml:59: Used in 
native/qml/SourceSection.qml:59:  scenes
native/qml/SourceSection.qml:59: Shared input
native/qml/SourceSection.qml:67: Changes this input everywhere
native/qml/SourceSection.qml:67: . Size, crop and filters are per scene.
native/qml/SourceSection.qml:101: Changes here apply in every scene that uses 
native/qml/SourceSection.qml:101: Changes here apply wherever 
native/qml/SourceSection.qml:101:  is used.
native/qml/SourceSection.qml:104: Open in Sources
native/qml/SourceSection.qml:104: Show this input in the Sources panel
native/qml/StatsView.qml:14: Stats
native/qml/StatsView.qml:15: Frame rate
native/qml/StatsView.qml:15:  fps
native/qml/StatsView.qml:16: Render time
native/qml/StatsView.qml:17: Frames missed (render lag)
native/qml/StatsView.qml:18: Frames skipped (encoder lag)
native/qml/StatsView.qml:20: Memory
native/qml/StatsView.qml:21: Free space for recordings
native/qml/StatsView.qml:23: Stream
native/qml/StatsView.qml:23:  kbps
native/qml/StatsView.qml:24: Dropped frames (network)
native/qml/StatsView.qml:25: Recording
native/qml/StatsView.qml:26: Replay buffer
native/qml/StatsView.qml:26: On · last 
native/qml/StatsView.qml:27: Virtual camera
native/qml/StatusBar.qml:25: Stream and recording stats
native/qml/StatusBar.qml:36: LIVE 
native/qml/StatusBar.qml:36:  kbps · 
native/qml/StatusBar.qml:36:  dropped (
native/qml/StatusBar.qml:38: REC 
native/qml/StatusBar.qml:39: Replay on
native/qml/StatusBar.qml:40: Virtual camera
native/qml/StatusBar.qml:41: CPU 
native/qml/StatusBar.qml:43:  fps · 
native/qml/StatusBar.qml:50: Lagging 
native/qml/StatusBar.qml:50: % · skipped 
native/qml/StatusBar.qml:52:  GB free
native/qml/TransitionsDialog.qml:10: Transitions
native/qml/TransitionsDialog.qml:16: Fade
native/qml/TransitionsDialog.qml:16: Swipe
native/qml/TransitionsDialog.qml:16: Slide
native/qml/TransitionsDialog.qml:16: Fade to color
native/qml/TransitionsDialog.qml:16: Luma wipe
native/qml/TransitionsDialog.qml:16: Stinger
native/qml/TransitionsDialog.qml:56: In use
native/qml/TransitionsDialog.qml:65: Add transition
native/qml/TransitionsDialog.qml:116: Preview
native/qml/TransitionsDialog.qml:130: In use
native/qml/TransitionsDialog.qml:130: Use for Show
native/qml/TransitionsDialog.qml:133: Show plays this transition when it switches scenes
native/qml/TransitionsDialog.qml:139: Delete this transition
native/qml/TransitionsDialog.qml:154: Switches instantly, with no animation.
native/qml/TransitionsDialog.qml:159: Duration
native/qml/TransitionsDialog.qml:169: Direction
native/qml/TransitionsDialog.qml:183: Swipe in (the new scene moves over the old one)
native/qml/TransitionsDialog.qml:191: Color
native/qml/TransitionsDialog.qml:197: Switch point
native/qml/TransitionsDialog.qml:207: Pattern
native/qml/TransitionsDialog.qml:218: Softness
native/qml/TransitionsDialog.qml:225: Invert
native/qml/TransitionsDialog.qml:233: Video
native/qml/TransitionsDialog.qml:234: A video with transparency (.webm, .mov)
native/qml/TransitionsDialog.qml:235: Browse
native/qml/TransitionsDialog.qml:240: Switch scenes at
native/qml/TransitionsDialog.qml:249: Audio
native/qml/TransitionsDialog.qml:252: Viewers hear it
native/qml/TransitionsDialog.qml:252: Only you hear it
native/qml/TransitionsDialog.qml:252: Silent
native/qml/TransitionsDialog.qml:260: Scene audio
native/qml/TransitionsDialog.qml:263: Fade out, then in
native/qml/TransitionsDialog.qml:263: Crossfade
native/qml/TransitionsDialog.qml:271: Plays a video over the switch. The scene changes at the time above.
native/qml/TransitionsDialog.qml:279: Stinger video
native/qml/TransitionsDialog.qml:280: Videos (*.webm *.mov *.mp4 *.mkv *.avi)
native/qml/TransitionsDialog.qml:280: All files (*)
native/qml/TransitionsDialog.qml:285: Double-click a transition to use it for Show.
native/qml/TransitionsDialog.qml:286: Done
native/qml/TwitchPanel.qml:13: Change channel
native/qml/TwitchPanel.qml:30: Zoom (
native/qml/TwitchPanel.qml:30: Ctrl
native/qml/TwitchPanel.qml:30: Ctrl
native/qml/TwitchPanel.qml:30:  -). Click for 100%.
native/qml/TwitchPanel.qml:31: Reload
native/qml/TwitchPanel.qml:32: Open in your browser
native/qml/TwitchPanel.qml:33: Change channel
native/qml/TwitchPanel.qml:54: Loading Twitch…
native/qml/TwitchPanel.qml:61: Twitch chat
native/qml/TwitchPanel.qml:61: Stream info
native/qml/TwitchPanel.qml:62: As in twitch.tv/yourname.
native/qml/TwitchPanel.qml:67: Open
native/qml/dock/DockSeparator.qml:9: Resize panels
native/qml/dock/DockSeparator.qml:17: Resize panels
native/qml/dock/DockTabBar.qml:14: Panel tabs
native/qml/dock/DockTabBar.qml:51: Panel tabs
native/qml/dock/DockTabBar.qml:158: Drag to move · double-click to float
native/qml/dock/DockTabBar.qml:177: Close panel
native/qml/dock/DockTitleBar.qml:24: Dock back
native/qml/dock/DockTitleBar.qml:25: Maximize
native/qml/dock/DockTitleBar.qml:26: Close
native/qml/dock/PanelDragController.qml:161: Moving 
native/qml/dock/PanelDragController.qml:189: Add as tab
native/src/app.cpp:69: Scene
native/src/app.cpp:70: Background
native/src/app.cpp:73: Desktop Audio
native/src/app.cpp:93: Virtual
native/src/app.cpp:170: Replay saved: 
native/src/app.cpp:355: Couldn't autosave. Check that there's space on the drive.
native/src/app.cpp:402: Cameras
native/src/app.cpp:403: Screen capture
native/src/app.cpp:404: Audio
native/src/app.cpp:405: Media
native/src/app.cpp:422: Cameras
native/src/app.cpp:422: Screen capture
native/src/app.cpp:422: Audio
native/src/app.cpp:422: Media
native/src/app.cpp:549:  (this scene)
native/src/app.cpp:641: On program: 
native/src/app.cpp:656: Background
native/src/app.cpp:663: Background
native/src/app.cpp:675: Add scene
native/src/app.cpp:676: Scene
native/src/app.cpp:700: Save swatch
native/src/app.cpp:700: Remove swatch
native/src/app.cpp:700: Change swatches
native/src/app.cpp:727: Fade
native/src/app.cpp:727: Swipe
native/src/app.cpp:727: Slide
native/src/app.cpp:728: Fade to color
native/src/app.cpp:728: Luma wipe
native/src/app.cpp:728: Stinger
native/src/app.cpp:749: Add transition
native/src/app.cpp:756: A show needs at least one transition
native/src/app.cpp:757: Delete transition
native/src/app.cpp:770: Use transition
native/src/app.cpp:775: Edit transition
native/src/app.cpp:834: Program
native/src/app.cpp:835: LOBS projector · 
native/src/app.cpp:881: Facecam
native/src/app.cpp:921: Facecam
native/src/app.cpp:942: Facecam
native/src/app.cpp:954: Add background
native/src/app.cpp:960: Rename scene
native/src/app.cpp:967: A show needs at least one scene
native/src/app.cpp:970: Delete scene
native/src/app.cpp:976: Duplicate scene (linked)
native/src/app.cpp:976: Duplicate scene
native/src/app.cpp:977:  copy
native/src/app.cpp:991: Text
native/src/app.cpp:1010: Add 
native/src/app.cpp:1077: A scene can't show itself, even through another scene
native/src/app.cpp:1091: A scene can't show itself, even through another scene
native/src/app.cpp:1095: Add scene 
native/src/app.cpp:1095:  as a source
native/src/app.cpp:1107: Audio sources aren't placed on the canvas
native/src/app.cpp:1111: Place 
native/src/app.cpp:1123: Rename source
native/src/app.cpp:1137: Delete source
native/src/app.cpp:1171: Edit 
native/src/app.cpp:1223: Show layer
native/src/app.cpp:1223: Hide layer
native/src/app.cpp:1229: Lock layer
native/src/app.cpp:1229: Unlock layer
native/src/app.cpp:1242: Move layer up
native/src/app.cpp:1242: Move layer down
native/src/app.cpp:1250: Bring to front
native/src/app.cpp:1250: Send to back
native/src/app.cpp:1250: Bring forward
native/src/app.cpp:1250: Send backward
native/src/app.cpp:1282: Group layers
native/src/app.cpp:1282: Move into group
native/src/app.cpp:1282: Move layer
native/src/app.cpp:1310: Rename layer
native/src/app.cpp:1322: The background can't be deleted. Hide it instead.
native/src/app.cpp:1323: Delete %1 layers
native/src/app.cpp:1323: Delete layer
native/src/app.cpp:1342: To group layers, pick ones at the same level (all inside one group, or all outside)
native/src/app.cpp:1344: Group layers
native/src/app.cpp:1355: Ungroup
native/src/app.cpp:1366: Duplicate layer
native/src/app.cpp:1393: Copied 
native/src/app.cpp:1393: Copied %1 layers
native/src/app.cpp:1404: Cut 
native/src/app.cpp:1404: Cut %1 layers
native/src/app.cpp:1413: Paste linked
native/src/app.cpp:1413: Paste
native/src/app.cpp:1462: Copy to %1 (%2)
native/src/app.cpp:1464: Copied to %1 as %2
native/src/app.cpp:1479: Move group
native/src/app.cpp:1493: Flip horizontally
native/src/app.cpp:1493: Unflip horizontally
native/src/app.cpp:1494: Flip vertically
native/src/app.cpp:1494: Unflip vertically
native/src/app.cpp:1494: Set 
native/src/app.cpp:1532: Move group
native/src/app.cpp:1541: Set 
native/src/app.cpp:1571: Resize
native/src/app.cpp:1608: This layer's size isn't known yet
native/src/app.cpp:1620: Crop to aspect
native/src/app.cpp:1620: Remove crop
native/src/app.cpp:1662: Cancel crop
native/src/app.cpp:1677: Round corners
native/src/app.cpp:1709: Crop
native/src/app.cpp:1726: Crop
native/src/app.cpp:1740: Opacity
native/src/app.cpp:1747: Show layer
native/src/app.cpp:1747: Hide layer
native/src/app.cpp:1778: Fit to canvas
native/src/app.cpp:1778: Fill canvas
native/src/app.cpp:1778: Center
native/src/app.cpp:1778: Fix aspect
native/src/app.cpp:1791: Add 
native/src/app.cpp:1805: Remove filter
native/src/app.cpp:1819: Toggle filter
native/src/app.cpp:1836: Edit filter
native/src/app.cpp:1856: Link %1 with %2
native/src/app.cpp:1857: %1 linked with %2
native/src/app.cpp:1874: Unlink and reset 
native/src/app.cpp:1874: Reset 
native/src/app.cpp:1884: Unlinked 
native/src/app.cpp:1884:  and reset it here only
native/src/app.cpp:1894: Unlink 
native/src/app.cpp:1920: Undo: 
native/src/app.cpp:1929: Redo: 
native/src/app.cpp:1955: Stop streaming and recording to start a new show
native/src/app.cpp:1956: New document
native/src/app.cpp:1967: Stop streaming and recording to load the sample show
native/src/app.cpp:1968: Load sample show
native/src/app.cpp:2053: Add your stream key first
native/src/app.cpp:2054: Twitch
native/src/app.cpp:2055: The bandwidth test only works with Twitch
native/src/app.cpp:2064: Twitch
native/src/app.cpp:2064: YouTube
native/src/app.cpp:2087: Twitch
native/src/app.cpp:2089: YouTube
native/src/app.cpp:2091: Kick
native/src/app.cpp:2103: Couldn't save 
native/src/app.cpp:2109: /LOBS Projects
native/src/app.cpp:2124: Give the project a name.
native/src/app.cpp:2127: Names can't contain < > : /" / // | ? * or end with a period.
native/src/app.cpp:2131: /" is reserved by Windows. Choose another name.
native/src/app.cpp:2134: Choose where to keep it.
native/src/app.cpp:2137: That name is too long for this folder. Use a shorter name or folder.
native/src/app.cpp:2139: There's already a folder named /"
native/src/app.cpp:2139: /" there.
native/src/app.cpp:2148: Stop streaming and recording to start a new project
native/src/app.cpp:2156: Scene
native/src/app.cpp:2160: Couldn't create 
native/src/app.cpp:2162: New project
native/src/app.cpp:2172: Created 
native/src/app.cpp:2206: Couldn't save 
native/src/app.cpp:2214: Saved 
native/src/app.cpp:2223: Stop streaming and recording to open another show
native/src/app.cpp:2226: Couldn't open 
native/src/app.cpp:2231: Not a LOBS document
native/src/app.cpp:2237: Repaired %1 broken reference%2 in this show
native/src/app.cpp:2238: Open 
native/src/app.cpp:2269: MMM d, yyyy
native/src/app.cpp:2278: Couldn't open 
native/src/app.cpp:2281: Not an OBS scene collection
native/src/app.cpp:2294: %1: device not connected, using this PC's default
native/src/app.cpp:2300: Stop streaming and recording to import a collection
native/src/app.cpp:2305: Import 
native/src/app.cpp:2305:  from OBS
native/src/app.cpp:2320:  filter has no LOBS controls
native/src/app.cpp:2321: no LOBS equivalent
native/src/app.cpp:2322: device not connected
native/src/app.cpp:2326: controls for some filters on %1 source%2 (they still run)
native/src/app.cpp:2327: %1 source%2 with no LOBS equivalent
native/src/app.cpp:2328: %1 device%2 not connected, set to this PC's default
native/src/app.cpp:2329: %1 other item%2
native/src/app.cpp:2331: %1 scene%2 made with the old LOBS plugin (%3)
native/src/app.cpp:2333: Imported %1 scene%2 and %3 sources from OBS /"%4/".
native/src/app.cpp:2334:  Not carried over: 
native/src/app.cpp:2381: Live lock is on. Hold Alt or unlock to change layers on air.
native/src/app.cpp:2392: Live lock is on. Hold Alt or unlock to change sources on air.
native/src/app.cpp:2403: Live lock is on. Hold Alt or unlock to change layers on air.
native/src/app.cpp:2416: Nudge
native/src/app.cpp:2458: Audio sources aren't placed on the canvas
native/src/app.cpp:2464: Place 
native/src/app.cpp:2507: Resolution and FPS changes apply after you stop streaming and recording
native/src/app.cpp:2538: Not on this OS
native/src/app.cpp:2539: Game Capture is Windows-only
native/src/app.cpp:2539: Not available here
native/src/app.cpp:2542: Needs access
native/src/app.cpp:2543: Allow 
native/src/app.cpp:2543:  for LOBS
native/src/app.cpp:2554: Windows default device
native/src/app.cpp:2554: No device chosen
native/src/app.cpp:2556: System default device
native/src/app.cpp:2556: No device chosen
native/src/app.cpp:2559: Not connected
native/src/app.cpp:2564: A specific game window
native/src/app.cpp:2564: Any fullscreen game
native/src/app.cpp:2568: No URL yet
native/src/app.cpp:2568: Local page
native/src/app.cpp:2578: Waiting for a game
native/src/app.cpp:2578: Window not found
native/src/app.cpp:2578: No signal
native/src/app.cpp:2599: Remove 
native/src/app.cpp:2599:  from 
native/src/app.cpp:2680: Assets
native/src/app.cpp:2724: New folder
native/src/app.cpp:2726: Couldn't create a folder here
native/src/app.cpp:2736: Names can't be empty or contain // / : * ? /" < > |
native/src/app.cpp:2746: Something named /"
native/src/app.cpp:2746: /" is already here
native/src/app.cpp:2748: Couldn't rename 
native/src/app.cpp:2748:  (is it open in another app?)
native/src/app.cpp:2764: Preferred DropEffect
native/src/app.cpp:2773: Copied 
native/src/app.cpp:2773: Copied %1 items
native/src/app.cpp:2781: Cut 
native/src/app.cpp:2781: . Paste it into another folder.
native/src/app.cpp:2781: Cut %1 items
native/src/app.cpp:2819: A folder can't go inside itself
native/src/app.cpp:2834: Moved 1 item
native/src/app.cpp:2834: Moved %1 items
native/src/app.cpp:2842: Nothing to paste. Copy files here or in File Explorer first.
native/src/app.cpp:2847: Preferred DropEffect
native/src/app.cpp:2864: Pasted 1 item
native/src/app.cpp:2864: Pasted %1 items
native/src/app.cpp:2882: %1 item%2 in use by layers, so kept. Remove those layers first.
native/src/app.cpp:2882: s are
native/src/app.cpp:2884: Moved 1 item to the Recycle Bin
native/src/app.cpp:2884: Moved %1 items to the Recycle Bin
native/src/app.cpp:2898: 1 item
native/src/app.cpp:2898: %1 items
native/src/app.cpp:2914: %1 MB
native/src/app.cpp:2914: %1 KB
native/src/app.cpp:3031: Found %1 missing file%2 in assets
native/src/app.cpp:3031: ; %1 still missing
native/src/app.cpp:3033: Not found in assets. Use Choose file…
native/src/app.cpp:3046: Couldn't copy that file into assets
native/src/app.cpp:3048: Relinked 
native/src/app.cpp:3090: Added %1 file%2 to assets
native/src/app.cpp:3090:  (%1 couldn't be copied)
native/src/app.cpp:3120:  is no longer in the assets folder
native/src/app.cpp:3133: Moved 
native/src/app.cpp:3133:  to the Recycle Bin
native/src/app.cpp:3135: Couldn't move 
native/src/app.cpp:3135:  to the Recycle Bin
native/src/app.cpp:3189: Place 
native/src/app.cpp:3209: Text
native/src/app.cpp:3209: Text
native/src/app.cpp:3211: Color
native/src/app.cpp:3284: Turn on the replay buffer first
native/src/app.cpp:3306: Output
native/src/app.cpp:3306: Start or stop streaming
native/src/app.cpp:3307: Output
native/src/app.cpp:3307: Start or stop recording
native/src/app.cpp:3308: Output
native/src/app.cpp:3308: Turn the replay buffer on or off
native/src/app.cpp:3309: Output
native/src/app.cpp:3309: Save replay
native/src/app.cpp:3310: Output
native/src/app.cpp:3310: Start or stop the virtual camera
native/src/app.cpp:3312: Scenes
native/src/app.cpp:3312: Show 
native/src/app.cpp:3317: Audio
native/src/app.cpp:3317: Mute or unmute 
native/src/app.cpp:3318: Audio
native/src/app.cpp:3318: Push to talk: 
native/src/app.cpp:3434: Reorder mixer
native/src/app.cpp:3442: Pin 
native/src/app.cpp:3442: Unpin 
native/src/app.cpp:3471: Echo risk: monitoring desktop audio plays it back into itself.
native/src/app.cpp:3473: Feedback risk: you hear your own mic. Use headphones, or speakers will feed back into it.
native/src/app.cpp:3475: Monitor only: you hear it, the stream and recording don't.
native/src/app.cpp:3477: Not monitored: viewers hear this page's sounds (alerts), you don't. Click the headphones to hear it.
native/src/app.cpp:3479: Track 1 is off, so it isn't in the stream.
native/src/app.cpp:3512: Add 
native/src/app.cpp:3519: Remove audio effect
native/src/app.cpp:3529: Reorder audio effects
native/src/app.cpp:3536: Enable audio effect
native/src/app.cpp:3536: Bypass audio effect
native/src/app.cpp:3543: Edit audio effect
native/src/app.cpp:3565: Volume 
native/src/app.cpp:3565: Balance 
native/src/app.cpp:3567: Mute 
native/src/app.cpp:3567: Unmute 
native/src/app.cpp:3567: Audio 
native/src/app.cpp:3724: Live lock is on. Hold Alt to add graphics.
native/src/app.cpp:3735: Live lock is on. Hold Alt to resize.
native/src/app.cpp:3739: Layer is locked
native/src/app.cpp:3746: Layer is locked
native/src/app.cpp:3755: Layer is locked
native/src/app.cpp:3809: Layer is locked
native/src/app.cpp:3812: Live lock is on. Hold Alt to move layers.
native/src/app.cpp:4080: Move %1 layers
native/src/app.cpp:4080: Move 
native/src/app.cpp:4082: Resize 
native/src/app.cpp:4084: Crop 
native/src/app.cpp:4144: Text
native/src/app.cpp:4152: Rectangle
native/src/app.cpp:4152: Ellipse
native/src/app.cpp:4157: Arrow
native/src/app.cpp:4157: Drawing
native/src/app.cpp:4175: Add 
native/src/app.cpp:4373: Needs 
native/src/app.h:307: New folder
native/src/app.h:520: Needs Screen Recording
native/src/app_updater.cpp:29: Logan Affleck
native/src/app_updater.cpp:51: the download isn't validly signed (0x%1)
native/src/app_updater.cpp:61: the download is signed by /"
native/src/app_updater.cpp:61: /", not LOBS's publisher
native/src/app_updater.cpp:69: updates aren't available on macOS yet
native/src/app_updater.cpp:76: Updated to LOBS 
native/src/app_updater.cpp:96: MMM d, h:mm ap
native/src/app_updater.cpp:125: Accept
native/src/app_updater.cpp:130: Couldn't reach GitHub to check for updates.
native/src/app_updater.cpp:164: Couldn't save the download to 
native/src/app_updater.cpp:201: The download didn't finish. Try again.
native/src/app_updater.cpp:205: The download was damaged (checksum mismatch). Try again.
native/src/app_updater.cpp:208: Not installed: 
native/src/app_updater.cpp:219: Not installed: 
native/src/app_updater.cpp:225: Couldn't start the installer.
native/src/browser_view.cpp:86: html { zoom: %1; }
native/src/captions.cpp:89: Couldn't download the speech model: 
native/src/captions.cpp:89: The speech model download was damaged. Try again.
native/src/captions.cpp:118: Detect automatically
native/src/captions.cpp:131: Download the speech model first
native/src/captions.cpp:135: Couldn't load the speech model
native/src/captions.cpp:144: That source has no audio
native/src/captions.cpp:150: Listening
native/src/crash_report.cpp:37: Crash in LOBS 
native/src/crash_report.cpp:38: **What were you doing when it crashed?**/n/n/n
native/src/crash_report.cpp:39: **Log:** drag 
native/src/crash_report.cpp:39: .log from the crash folder that just opened into this box. 
native/src/crash_report.cpp:40: (Don't post the 
native/src/crash_report.cpp:40:  here: it can hold your stream key. Share it privately if asked.)/n/n
native/src/crash_report.cpp:41: LOBS 
native/src/engine.cpp:74: libobs failed to start
native/src/engine.cpp:98: Video init failed (%1)
native/src/engine.cpp:103: Audio init failed
native/src/engine.cpp:150: Engine running
native/src/engine.cpp:241: Bold Italic
native/src/engine.cpp:241: Bold
native/src/engine.cpp:241: Italic
native/src/engine.cpp:241: Regular
native/src/engine.cpp:1323: Solid
native/src/engine.cpp:1372: Draw
native/src/engine.cpp:1404: Solid
native/src/engine.cpp:1418: Draw
native/src/engine.cpp:1454: Solid
native/src/engine.cpp:1482: Solid
native/src/engine.cpp:1788: %1 fps · %2 ms
native/src/engine.h:75: Needs Screen Recording
native/src/engine.h:82: : message
native/src/engine.h:261: Starting engine…
native/src/engine.h:282: Fade
native/src/engine_extras.cpp:110: LOBS replay video
native/src/engine_extras.cpp:112: LOBS replay audio
native/src/engine_extras.cpp:116: Couldn't create encoders for the replay buffer
native/src/engine_extras.cpp:119: Replay %CCYY-%MM-%DD %hh-%mm-%ss
native/src/engine_extras.cpp:122: LOBS replay buffer
native/src/engine_extras.cpp:125: The replay buffer isn't available in this engine
native/src/engine_extras.cpp:134: Replay buffer failed to start
native/src/engine_extras.cpp:138: Replay buffer on · keeps the last %1 s
native/src/engine_extras.cpp:148: Replay buffer off
native/src/engine_extras.cpp:171: Replay saved: 
native/src/engine_extras.cpp:216: This engine doesn't include the virtual camera driver
native/src/engine_extras.cpp:226: /c regsvr32 /s /"%1/" && regsvr32 /s /"%2/"
native/src/engine_extras.cpp:236: Virtual camera setup was cancelled
native/src/engine_extras.cpp:242: Windows didn't register the virtual camera
native/src/engine_extras.cpp:251: The virtual camera isn't available on macOS yet
native/src/engine_extras.cpp:264: LOBS virtual camera
native/src/engine_extras.cpp:266: The virtual camera isn't available in this engine
native/src/engine_extras.cpp:270: Virtual camera failed to start
native/src/engine_extras.cpp:274: Virtual camera on
native/src/engine_extras.cpp:284: Virtual camera off
native/src/engine_manager.cpp:318: SystemRoot
native/src/engine_manager.cpp:326: couldn't unpack the download: 
native/src/engine_manager.cpp:336: couldn't move the engine into place
native/src/engine_manager.cpp:347: OBS engine %1 didn't start, so LOBS went back to %2.
native/src/engine_manager.cpp:360: MMM d, h:mm ap
native/src/engine_manager.cpp:368: OBS engine %1 didn't start, so LOBS went back to %2.
native/src/engine_manager.cpp:387: Accept
native/src/engine_manager.cpp:392: Couldn't reach GitHub to check for OBS updates.
native/src/engine_manager.cpp:415: OBS engine %1 is installed. Restart LOBS to use it.
native/src/engine_manager.cpp:417: OBS %1 didn't pass LOBS's compatibility check, so LOBS stays on %2 until a LOBS update supports it.
native/src/engine_manager.cpp:420: OBS %1 didn't start on this PC earlier, so it isn't offered again.
native/src/engine_manager.cpp:433: No checksum for this download. Not installed.
native/src/engine_manager.cpp:438: Couldn't write the download to 
native/src/engine_manager.cpp:470: The download didn't finish. Try again.
native/src/engine_manager.cpp:474: Checksum mismatch. Download discarded.
native/src/engine_manager.cpp:491: Install failed: 
native/src/engine_manager.cpp:506: OBS %1 isn't compatible with this LOBS yet (%2), so LOBS stays on %3.
native/src/engine_manager.cpp:510: it no longer has %1 libobs function%2 LOBS uses: %3
native/src/engine_manager.cpp:512: Testing OBS %1 with LOBS...
native/src/engine_manager.cpp:533: a test run on it crashed or hung
native/src/engine_manager.cpp:533: the test run failed: 
native/src/engine_manager.cpp:539: OBS engine %1 passed LOBS's checks. Restart LOBS to use it.
native/src/engine_manager.cpp:561: LOBS will use OBS engine %1 after a restart.
native/src/engine_output.cpp:22: If Windows 10 Ransomware Protection is enabled it can also cause this error//.[^/n]*
native/src/engine_output.cpp:23: Windows Security %1 Controlled folder access can also block it: allow LOBS there, or choose another folder.
native/src/engine_output.cpp:36: NVIDIA NVENC H.264
native/src/engine_output.cpp:37: AMD AMF H.264
native/src/engine_output.cpp:38: Intel Quick Sync H.264
native/src/engine_output.cpp:39: Apple VideoToolbox H.264
native/src/engine_output.cpp:40: Apple VideoToolbox H.264
native/src/engine_output.cpp:41: x264 (software)
native/src/engine_output.cpp:173: Video reset failed (%1)
native/src/engine_output.cpp:189: Bad server address
native/src/engine_output.cpp:190: Couldn't connect to the server
native/src/engine_output.cpp:191: Server rejected the stream key
native/src/engine_output.cpp:192: Disconnected from the server
native/src/engine_output.cpp:193: Encoder error
native/src/engine_output.cpp:194: Stream stopped (%1)
native/src/engine_output.cpp:212: Custom
native/src/engine_output.cpp:213: Add your stream key in Settings first
native/src/engine_output.cpp:225: Custom
native/src/engine_output.cpp:227: LOBS service
native/src/engine_output.cpp:231: LOBS service
native/src/engine_output.cpp:237: LOBS stream video
native/src/engine_output.cpp:239: Couldn't create the video encoder
native/src/engine_output.cpp:243: LOBS stream audio
native/src/engine_output.cpp:259: LOBS stream
native/src/engine_output.cpp:271: Couldn't start the stream
native/src/engine_output.cpp:280: Live on 
native/src/engine_output.cpp:280: Custom
native/src/engine_output.cpp:292: Stream ended after 
native/src/engine_output.cpp:347: /LOBS 
native/src/engine_output.cpp:347: yyyy-MM-dd HH-mm-ss
native/src/engine_output.cpp:363: LOBS record video
native/src/engine_output.cpp:364: LOBS record video
native/src/engine_output.cpp:366: LOBS record audio
native/src/engine_output.cpp:373: LOBS recording
native/src/engine_output.cpp:379: Recording failed to start
native/src/engine_output.cpp:386: Recording → 
native/src/engine_output.cpp:397: Recording saved: 
native/src/engine_output.cpp:471: LOBS bandwidth test
native/src/engine_output.cpp:475: LOBS bandwidth test video
native/src/engine_output.cpp:478: LOBS bandwidth test audio
native/src/engine_output.cpp:482: Couldn't set up the test encoder
native/src/engine_output.cpp:487: LOBS bandwidth test
native/src/engine_output.cpp:494: Couldn't connect to the server
native/src/engine_output.cpp:533: The server didn't accept the test stream. Check the stream key.
native/src/permissions.cpp:38: not asked yet
native/src/permissions.cpp:60: Screen Recording
native/src/permissions.cpp:60: Input Monitoring
native/src/permissions.cpp:60: Camera
native/src/permissions.cpp:60: Microphone
native/src/permissions.cpp:92: mic sources
native/src/permissions.cpp:92: camera sources
native/src/permissions.cpp:92: screen, window and desktop audio sources
native/src/permissions.cpp:93: global hotkeys
native/src/permissions.cpp:102: is on and takes effect when LOBS reopens.
native/src/permissions.cpp:110:  won't work
native/src/permissions.cpp:110:  stay empty
native/src/permissions.cpp:113: takes effect when LOBS reopens.
native/src/permissions.cpp:117: is needed for 
native/src/permissions.cpp:118: is off, so 
native/src/permissions.cpp:118: . Turn it on in System Settings
native/src/permissions.cpp:118: , then reopen LOBS.
native/src/permissions.cpp:129: Mics and live captions
native/src/permissions.cpp:129: Cameras
native/src/permissions.cpp:129: Screens, windows and desktop audio
native/src/permissions.cpp:181: not asked yet
native/src/permissions.h:30: Microphone
native/src/permissions.h:30: Screen Recording
native/src/platform_mac.mm:37: stream key
native/src/platform_mac.mm:47: LOBS stream key
native/src/platform_mac.mm:77: Asked
native/src/platform_mac.mm:89: Asked
native/src/platform_mac.mm:103: Privacy_ScreenCapture
native/src/platform_mac.mm:104: Privacy_ListenEvent
native/src/platform_mac.mm:105: Privacy_Camera
native/src/platform_mac.mm:106: Privacy_Microphone
native/src/platform_mac.mm:115: Name: WxH @ x,y
native/src/platform_mac.mm:189: while kill -0 %1 2>/dev/null; do sleep 0.2; done; open -n /"$0/"
native/src/platform_win.cpp:56: LOBS stream key
native/src/settings.h:11: Twitch
native/src/settings.h:11: Custom
native/src/settings.h:40: Ctrl+Shift+F1
native/src/window_chrome_mac.mm:131: AppleActionOnDoubleClick
native/src/window_chrome_mac.mm:132: Minimize
native/src/window_chrome_mac.mm:134: None
native/src/window_chrome_mac.mm:144: (Ctrl/s*[+ ]/s*Shift/s*[+ ]/s*)
native/src/window_chrome_mac.mm:144: (Shift/s*[+ ]/s*Ctrl/s*[+ ]/s*)
native/src/window_chrome_mac.mm:145: (Ctrl/s*[+ ]/s*(?=/S))
native/src/window_chrome_mac.mm:145: (Alt/s*[+ ]/s*(?=/S))
native/src/window_chrome_mac.mm:148: Ctrl

```
