---
title: Importing from OBS
order: 11
description: Bring an OBS Studio scene collection into LOBS, and what does and doesn't come over.
---

Import reads an OBS Studio scene collection and turns it into a LOBS show. It only reads the OBS file. Nothing in OBS changes.

![The Import menu listing OBS scene collections](shot:import)

## How to import

- Click **Import** in the top bar.
- The menu lists every scene collection in `%APPDATA%\obs-studio\basic\scenes`, newest first, as "OBS: &lt;name&gt; · N scenes". If there are none, it reads "No OBS scene collections found".
- **Choose a file...** opens a collection from anywhere.

Opening an OBS collection with **Open** imports it too.

The import replaces your current show. If that show matters, [save it as a project](/docs/projects-and-assets/#save) first. Ctrl+Z undoes the import.

When it's done, a toast reads "Imported N scenes and M sources from OBS "&lt;name&gt;"", followed by a list of anything not carried over. The full list goes to `lobs.log` in the app folder.

## What comes over

- The canvas resolution.
- Scenes, in OBS's order. The scene that was current in OBS becomes the one you edit and the one on program.
- Sources, matched by type:

| OBS source | LOBS source | Settings carried over |
|---|---|---|
| Video Capture Device | Camera | device |
| Display Capture | Display Capture | display |
| Window Capture | Window Capture | window |
| Game Capture | Game Capture | mode, window |
| Image | Image | file |
| Media Source | Media File | file, loop |
| Browser | Browser | URL or local file, width and height (default 800×600), custom CSS |
| Color Source | Color | color |
| Text (GDI+ or FreeType 2) | Text | text, font, size, color, bold, italic |
| Audio Input Capture | Microphone | device |
| Audio Output Capture, Application Audio Capture | Desktop Audio | device |

- Each layer's position, scale, alignment, bounding box, rotation, flip, crop, visibility and lock. Sizes settle once each source reports its real size, which can take up to 10 seconds.
- Audio settings: volume, mute, mono, balance, sync offset, monitoring and tracks.
- Global audio devices (Desktop Audio 1 and 2, Mic/Aux 1 to 4). Audio inputs that OBS had inside scenes become global sources, so they play in every scene and aren't placed on the canvas.
- Video filters: **Color Correction**, **Chroma Key** and **Sharpen** become LOBS filters. **Apply LUT** becomes **LUT**. A relative Crop/Pad filter is merged into the layer's crop.
- If a source with filters appears in several scenes, its filters are [linked](/docs/inspector/#linking-sections) across those scenes.
- Audio filters: noise suppression, noise gate, gain, EQ, compressor, expander, limiter and NVIDIA effects get LOBS controls.

## What doesn't come over, or changes

- **Groups** are flattened into separate layers.
- **Nested scenes** (a scene used inside another scene) are skipped. LOBS doesn't support them yet.
- **Sources with no LOBS equivalent** are skipped.
- **Crop/Pad filters with an absolute size** are skipped.
- **Other video filters** are kept but have no controls in LOBS.
- **Other audio filters** are kept but show "Imported from OBS. No controls in LOBS yet."
- **Cameras and displays** that aren't on this PC fall back to this PC's default device.
