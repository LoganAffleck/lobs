---
title: Audio
order: 7
description: Set levels, monitoring, tracks and effects for every source with sound in the Audio Mixer.
---

The Audio Mixer lists every source with sound: mics, desktop audio, media files and browser sources. It sits as a tab next to Sources by default.

![The Audio Mixer](shot:mixer)

## Global and scene audio

- **Global** sources (Mic and Desktop Audio) play in every scene, all the time. Their devices are set in [Settings > Audio](/docs/settings/#audio).
- **Scene** sources (media files and browser sources) are heard only while a scene that contains them is running.

## The mixer

The toolbar shows **N inputs**, a **Search audio** field and a layout button that cycles **Auto**, **Rows** and **Strips**. **Auto** uses vertical strips when the panel is wide and short, and compact rows otherwise. LOBS remembers your choice.

Each source has a color dot, icon, name, badges and a dB readout. The badges are:

| Badge | Meaning |
|---|---|
| **FX N** | N effects in the chain. |
| **Mono** | Mono is on. |
| **L n%** / **R n%** | Balance is off center. |
| **+N ms** | Sync offset. |
| Amber warning icon | A setting worth checking. See [Warnings](#warnings). |

- The mute button mutes the source.
- The headphones button turns monitoring on (you hear it, and so does the output) or off.
- **…** opens **Monitoring, tracks and effects**.

### Fader and meter

- The fader runs from -60 to 0 dB on a log scale.
- The stereo peak meter is green from -60 to -18 dB, amber from -18 to -6 dB and red from -6 to 0 dB. It dims while the source is muted.
- Drag the fader, or use the mouse wheel (2% per notch). Double-click resets it to 0 dB.
- Levels update 20 times a second. One fader drag is one undo step.

## Monitoring, tracks and effects

The **…** popover shows **Global** or **Scene** and these settings:

| Setting | What it does |
|---|---|
| **Volume** | Same as the fader. |
| Monitoring | **Monitor off**, **Monitor and output**, or **Monitor only (not in output)**. |
| **Balance** | Left to right, with a center detent. Reads **Center**, **L n%** or **R n%**. |
| **Sync offset (ms)** | Shifts the source's timing, from -950 to 20000 ms. |
| **Mono** | Mixes the source down to mono. |
| **Tracks** | Toggles for tracks 1 to 6. Track 1 is the stream. New sources are on track 1 only. |

The stream, recordings and replays currently contain track 1 only. Tracks 2 to 6 can be set, but no output uses them yet.

### Warnings

| Warning | When |
|---|---|
| "Echo risk: monitoring desktop audio plays it back into itself." | Desktop audio is monitored. |
| "Monitor only: you hear it, the stream and recording don't." | Monitoring is set to monitor only. |
| "Not monitored: viewers hear this page's sounds (alerts), you don't. Click the headphones to hear it." | A browser source isn't monitored. |
| "Track 1 is off, so it isn't in the stream." | Track 1 is off. |

### Effects

Click **+ Effect** to add one to the source's chain. Effects run top to bottom. Each card has an on/off switch, **Earlier in the chain**, **Later in the chain** and **Remove**. Effects apply to the source in every scene.

| Effect | Parameters (range, default) |
|---|---|
| **Noise Suppression (RNNoise)** | Suppression -60 to 0 dB (-30) |
| **Noise Gate** | Close below -96 to 0 dB (-32); Open above -96 to 0 dB (-26); Attack 0 to 500 ms (25); Hold 0 to 2000 ms (200); Release 0 to 2000 ms (150) |
| **Gain** | -30 to 30 dB (0) |
| **3-Band EQ** | Low, Mid, High: -20 to 20 dB (0) |
| **Compressor** | Threshold -60 to 0 dB (-18); Ratio 1 to 32:1 (10); Attack 1 to 500 ms (6); Release 1 to 1000 ms (60); Output gain -32 to 32 dB (0) |
| **Expander** | Threshold -60 to 0 dB (-40); Ratio 1 to 20 (2); Attack 1 to 100 ms (10); Release 1 to 1000 ms (50); Output gain -32 to 32 dB (0) |
| **Limiter** | Threshold -60 to 0 dB (-6); Release 1 to 1000 ms (60) |
| **NVIDIA Audio Effects** | Strength 0 to 1 (1) |

Audio filters imported from OBS that LOBS doesn't know are kept, but show "Imported from OBS. No controls in LOBS yet."

## Push to talk

Bind **Push to talk: &lt;source&gt;** in [Settings > Hotkeys](/docs/settings/#hotkeys). The source then stays muted except while you hold the key.

## Signal flow

Each source's sound passes through, in order:

1. Its effects, top to bottom.
2. Volume, mute and push to talk.
3. Mono, balance and sync offset.
4. Monitoring.
5. The tracks it's assigned to.
