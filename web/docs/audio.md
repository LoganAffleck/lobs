---
title: Audio
order: 7
description: Set levels, where each sound goes, monitoring and effects for every source with sound.
---

The Audio mixer lists every source with sound: mics, desktop audio, media files and browser sources. It sits as a tab next to Sources by default.

![The Audio mixer](shot:mixer)

## Global and scene audio

- **Global** sources (Mic and Desktop Audio) play in every scene, all the time. Their devices are set in [Settings > Audio](/docs/settings/#audio).
- **Scene** sources (media files and browser sources) are heard only while a scene that contains them is running.

## The mixer

The toolbar shows **N sources**, a **Search audio** field and a layout button that cycles **Auto**, **Rows** and **Strips**. **Auto** uses vertical strips when the panel is wide and short, and compact rows otherwise. LOBS remembers your choice.

Each source has a color dot, icon, name, badges and a dB readout. The badges are:

| Badge | Meaning |
|---|---|
| **FX N** | N effects in the chain. |
| **Mono** | Mono is on. |
| **L n%** / **R n%** | Balance is off center. |
| **+N ms** | Sync offset. |
| Amber warning icon | A setting worth checking. See [Warnings](#warnings). |

- Click a source's name to select it and show its settings in the Inspector. Drag the name to reorder; a drag starts once the mouse moves a few pixels.
- The mute button mutes the source.
- The headphones button turns monitoring on (you hear it, and so does the output) or off.
- **…** shows its [audio settings](#audio-settings) in the Inspector.

### Fader and meter

- The fader runs from -60 to 0 dB on a log scale.
- The stereo peak meter is green from -60 to -18 dB, amber from -18 to -6 dB and red from -6 to 0 dB. It dims while the source is muted.
- Drag the fader, or use the mouse wheel (2% per notch). Double-click resets it to 0 dB.
- Levels update 20 times a second. One fader drag is one undo step.

## Audio settings

Select an audio source (in Sources or the mixer) and the Inspector shows its settings.

![A mic in the Inspector](shot:audio-inspector)

- **Mute, meter and level.** The live meter is also the volume control: drag its handle. The level reads in dB, or **Muted**.
- **Active in.** **Recording** and **Stream**, each on or off. Turn off **Recording** to keep a sound, such as alerts or chat sounds, out of your recordings while viewers still hear it. Both are off while monitoring is **Monitor only**.
- **Monitor.** **Off**, **Monitor and output** (you hear it, and so do the stream and recording), or **Monitor only** (you hear it, nobody else does).

### Advanced

Closed by default. When something in it isn't at its default, the closed row says so (for example **Mono**).

| Setting | What it does |
|---|---|
| **Balance** | Left to right, filling from the middle. Snaps to **Center** near the middle. |
| **Sync offset** | Shifts the source's timing, from -950 to 20000 ms. |
| **Mono** | Mixes the source down to mono. Fixes a mic heard on one side only. |
| **Always muted in "scene"** | Mic and desktop audio only. Keeps the source muted while the scene you're editing is on air; other scenes still hear it. |
| **Pinned to the front of the mixer** | Keeps the source first in the mixer. |
| **Extra tracks** | Tracks 2 to 5, for multitrack recordings and plugins. Track 1 is the stream; track 6 is the recording's own mix. |

### Warnings

| Warning | When |
|---|---|
| "Monitoring desktop audio causes an echo." | Desktop audio is set to **Monitor and output**. |
| "You'll hear your own mic. Use headphones to avoid feedback." | A mic is set to **Monitor and output**. |
| "Monitor only: not in the stream or recording." | Monitoring is **Monitor only**. |
| "Viewers hear this page but you don't. Turn on monitoring to hear it." | A browser source isn't monitored. |

### Effects

Click **+ Effect** to add one to the source's chain. Effects run top to bottom. Each effect is one row: its name and a **…** menu (**Move up**, **Move down**, **Remove**), then its control and its on/off switch. Drag the grip to move an effect; the others slide aside to show where it lands. An effect with several settings shows a summary; click it to open its settings, and again to close them. Effects apply to the source in every scene.

Each setting gets the control that suits it:

- **Levels** (thresholds, suppression): a bar to drag, and the value to type.
- **Gains** (gain, output gain, EQ bands): fill from 0 dB, snap to it near the middle, and double-click to reset.
- **Ratios**: preset chips, and the value to type.
- **Times** (attack, hold, release): a number field. Type, scroll, drag sideways, or use **−** and **+**.

| Effect | Settings (range, default) |
|---|---|
| **Noise suppression** | Suppression -60 to 0 dB (-30) |
| **Noise gate** | Close and open thresholds -96 to 0 dB (-32, -26), on one bar over the live level; Attack 0 to 500 ms (25); Hold 0 to 2000 ms (200); Release 0 to 2000 ms (150) |
| **Gain** | -30 to 30 dB (0) |
| **3-band EQ** | Low, Mid, High: -20 to 20 dB (0), as three faders |
| **Compressor** | Threshold -60 to 0 dB (-18); Ratio 1 to 32:1 (10; presets 2, 4, 10, 20); Attack 1 to 500 ms (6); Release 1 to 1000 ms (60); Output gain -32 to 32 dB (0) |
| **Expander** | Threshold -60 to 0 dB (-40); Ratio 1 to 20:1 (2; presets 2, 3, 4, 6); Attack 1 to 100 ms (10); Release 1 to 1000 ms (50); Output gain -32 to 32 dB (0) |
| **Limiter** | Ceiling -60 to 0 dB (-6); Release 1 to 1000 ms (60) |
| **NVIDIA noise removal** | Strength 0 to 100% (100%) |

Audio filters imported from OBS that LOBS doesn't know are kept, but show "Imported from OBS. No controls in LOBS yet."

## Push to talk

Bind **Push to talk: &lt;source&gt;** in [Settings > Hotkeys](/docs/settings/#hotkeys). The source then stays muted except while you hold the key.

## Signal flow

Each source's sound passes through, in order:

1. Its effects, top to bottom.
2. Volume, mute, push to talk, and **Always muted** in the scene on air.
3. Mono, balance and sync offset.
4. Monitoring.
5. Where it's active: the stream (track 1), the recording (track 6) and any extra tracks.
