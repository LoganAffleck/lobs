---
title: Settings
order: 9
description: Every setting for streaming, recording, video, audio devices, hotkeys, the OBS engine and About.
---

Settings is a full page. Open it with the gear in the top bar or Ctrl+, and close it with Esc. Every change saves as you make it. The stream key is the exception: it has its own **Save** button.

While you're live, the Stream and Video sections read "You're live. Stream and video changes apply next time you go live." Resolution and frame rate changes made while streaming or recording apply once both have stopped.

## Stream

"Where LOBS sends your live video."

![Settings > Stream](shot:settings-stream)

- **Setup assistant**: **Run setup** opens the [setup assistant](/docs/getting-started/#setup-assistant) again.
- **Service**: every streaming service OBS knows, plus **Custom server**. Default **Twitch**. Pick **Custom server** for any RTMP or SRT server.
- **Server**: default **Auto**, which picks the closest ingest server.
- **Server URL** (Custom server only): an address starting with `rtmp://`, `rtmps://` or `srt://`.

### Stream key

- The status reads **Key saved** or **No key yet**. The key is stored encrypted.
- Paste the key into the masked field (**Paste stream key**, or **Replace key…** when one is saved) and click **Save**.
- The trash button is **Remove saved key**.

### Quality

- **Video bitrate**: in kbps, default 6000, minimum 500. 6000 kbps suits 1080p60 on Twitch. Services may cap it.
- **Audio bitrate**: 96, 128, 160, 192, 256 or 320 kbps. Default 160.

## Recording

"Where recordings go and how they're encoded."

- **Folder**: default `Videos\LOBS`. New recordings are named by date and time.
- **Format**: **MP4** (default), **MKV** or **MOV**. MKV survives a crash; MP4 plays everywhere.
- **Recording quality**:

| Option | What it does |
|---|---|
| **Same as stream** | While you're live, the recording reuses the stream's encoder. |
| **High, medium files** (default) | Constant quality at level 20. Intel Quick Sync records at 40 Mbps instead. |
| **Indistinguishable, large files** | Constant quality at level 16. Intel Quick Sync records at 80 Mbps instead. |

- **Keep the last**: the replay buffer length, 15 s, 30 s (default), 1 min or 2 min. Turn the buffer on with **Replay** in the top bar. See [Replay](/docs/going-live/#replay).

## Video

"Output size, frame rate and encoder."

![Settings > Video](shot:settings-video)

- **Resolution**: 1920×1080, 1600×900, 1280×720 or 960×540. The canvas stays 1920×1080 and is scaled to this size (with Lanczos scaling).
- **Frame rate**: 30 or 60 fps. Default 60.
- **Video encoder**: **Automatic** (shown with the encoder it picks) or any encoder on your PC. Hardware encoders use the GPU.
- **Preset**: **Quality** (default), **Balanced** or **Speed**. Quality looks best; Speed uses the least GPU.

Each preset maps to the encoder's own settings:

| Encoder | Quality | Balanced | Speed |
|---|---|---|---|
| x264 (CPU) | faster | veryfast | ultrafast |
| NVIDIA NVENC | p5 | p4 | p2 |
| AMD AMF | quality | balanced | speed |
| Intel Quick Sync | TU2 | TU4 | TU7 |

Keyframes are every 2 seconds.

## Audio

"Devices for your global mic and desktop audio."

One row per [global source](/docs/audio/#global-and-scene-audio), each with a device list (**Default device** follows Windows). Global sources are always mixed in, in every scene. Levels, monitoring and tracks are in the [Audio Mixer](/docs/audio/).

## Hotkeys

"Work even while a game has focus."

![Settings > Hotkeys](shot:settings-hotkeys)

LOBS has no default hotkeys. Every action reads **Not set** until you bind it.

- Click a field. It reads **Press keys**.
- Press the combination.
- Backspace or Delete clears the field. Esc cancels.

A combination can drive only one action. Binding it to a new action removes it from the old one.

| Group | Actions |
|---|---|
| Output | **Start or stop streaming**, **Start or stop recording**, **Turn the replay buffer on or off**, **Save replay**, **Start or stop the virtual camera** |
| Scenes | **Show &lt;scene&gt;** for each scene. It edits that scene and puts it on program with the current transition. |
| Audio | **Mute or unmute &lt;source&gt;** and **Push to talk: &lt;source&gt;** for each source with sound |

Keys you can use: A to Z, 0 to 9, F1 to F24, Space, Tab, Enter, Backspace, Insert, Delete, Home, End, Page Up, Page Down, the arrow keys, Pause, Print Screen, Scroll Lock, Num Lock and punctuation. Modifiers: Shift, Ctrl, Alt and Win.

## OBS engine

"Updates from OBS's official releases."

This section shows the installed engine and its update status, with **Check now**, **Include OBS betas** and **Go back to &lt;version&gt;**. See [OBS engine updates](/docs/obs-engine-updates/).

## About

- **Version**: the LOBS version.
- **Engine**: the OBS version (official release build).
- **Encoder in use**.
- **Shows and settings**: the `%APPDATA%\LOBS\LOBS` folder.
- **Recordings**: the recordings folder.
- Open source credits: OBS Studio (libobs, GPL-2.0), LOBS (GPL-2.0-or-later), Inter (SIL OFL 1.1), Lucide icons (ISC) and Fluid Functionalism.
