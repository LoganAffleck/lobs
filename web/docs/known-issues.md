---
title: Known issues
order: 15
description: Limits and known problems in LOBS 0.1.1.
---

LOBS 0.1 is an early preview. These are the known limits in 0.1.1.

- **Paths containing "test" break saving.** If the folder LOBS is installed in has "test" anywhere in its path (for example, a Windows account named "testuser" or "contestant"), LOBS runs in a self-test mode with no warning. It starts with a blank show every time, ignores your saved project and doesn't check for engine updates. Install LOBS to a folder without "test" in its path.
- **Autosave isn't crash-safe.** A crash while LOBS is writing the working show could leave it empty. Save your show as a [project](/docs/projects-and-assets/) to keep a separate copy.
- **The Lagging figure overstates lag.** The status bar counts every frame since LOBS started, so a hiccup at startup keeps showing.
- **Recordings and replays carry audio track 1 only.** The mixer offers tracks 1 to 6, but no output uses tracks 2 to 6 yet.
- **The canvas is fixed at 1920×1080.** The output resolution in Settings > Video scales it.
- **Some OBS video filters have no controls yet.** Crop/Pad (use the layer's crop), Video Delay (Async), HDR Tone Mapping, NVIDIA Background Removal and Video Denoise/Upscale, and plugin filters can't be added in LOBS. Imported ones are kept, but have no controls.
- **No nested scenes.** OBS groups are flattened when you import.
- **No way to add a color layer.** Color layers come only from the Background layer or an OBS import. The Background layer can be hidden but not deleted.
- **Linked duplicates don't link groups.** With **Duplicate with linked layers**, layers inside groups are linked, but the groups themselves aren't.
- **The connection test works only with Twitch.** For other services, the setup assistant asks for your upload speed.
