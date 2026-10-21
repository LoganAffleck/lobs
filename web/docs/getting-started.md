---
title: Getting started
order: 1
description: Install LOBS, get past SmartScreen, run the setup assistant and find where your files live.
---

LOBS is a streaming and recording app for Windows, built on OBS Studio's engine. This page covers installing it, the first launch and the setup assistant.

## Install

- Download the installer from [Releases](https://github.com/LoganAffleck/lobs/releases) and run it.
- It installs for your Windows user only and doesn't need admin rights. The default folder is `%LOCALAPPDATA%\Programs\LOBS`.
- OBS Studio doesn't need to be installed. LOBS 0.1.1 ships with OBS engine 32.2.2.
- If the Microsoft Visual C++ 2015-2022 runtime (14.40 or newer) is missing, the installer adds it. Only that step asks for admin permission.
- **Create a desktop shortcut** is optional and off by default. The Start menu gets **LOBS** and **Uninstall LOBS**.
- The last page offers **Start LOBS**.

Installing 0.1.1 over 0.1.0 keeps your shows and settings.

Don't install LOBS under a path that contains the word "test", including a Windows account named something like "testuser". LOBS treats that as a self-test run and doesn't keep your show. See [Known issues](/docs/known-issues/).

### SmartScreen

The installer isn't code-signed yet. If SmartScreen appears, choose **More info**, then **Run anyway**.

### What your PC needs

- 64-bit Windows.
- A graphics card with Direct3D 11. If video can't start, the status bar reads **Video init failed** with an error code.
- A hardware encoder is used when your PC has one: NVIDIA NVENC, AMD AMF or Intel Quick Sync. Without one, LOBS encodes on the CPU with x264.

If LOBS shows "LOBS couldn't find its OBS engine. Reinstall LOBS to restore it.", the engine folder is missing. Run the installer again.

## First launch

A new show starts with:

- One scene named **Scene**, containing a black **Background** layer.
- A **Mic** and a **Desktop Audio** source.

Sources that need a device get the first one on your PC whose name doesn't contain "Virtual". You can change devices in [Settings > Audio](/docs/settings/#audio).

## Setup assistant

The setup assistant opens a moment after launch, on every launch, until you finish or skip it. It picks a server, tests your connection and suggests a bitrate and resolution. To run it again, open **Settings > Stream** and click **Run setup**. Clicking outside the dialog doesn't close it.

![The setup assistant's first step](shot:setup)

The steps depend on what you choose on the first page:

| Choice | Steps |
|---|---|
| Stream and record, Twitch | Welcome, Your channel, Closest server, Test your connection, Video and bitrate, Recording, Twitch panels, You're set |
| Stream and record, YouTube or Other | Welcome, Your channel, Test your connection, Video and bitrate, Recording, You're set |
| Just record | Welcome, Video and bitrate, Recording, You're set |

### Welcome to LOBS

- Choose **Stream and record** or **Just record**.
- Under **Where do you stream?**, pick **Twitch**, **YouTube** or **Other (RTMP)**.
- **Skip setup** appears only on this page. It closes the assistant and doesn't open it again.

### Your channel

- **Twitch username** takes your channel name after `twitch.tv/`. If you paste a full link, LOBS trims it to the name.
- **Server URL** appears for **Other (RTMP)**. **Continue** stays disabled until you enter one.
- **Stream key** is a masked field. **Get my key** opens your service's stream key page in the browser.

Your stream key, service and server URL are saved when you click **Continue** on this page. Everything else is saved when you click **Finish**.

### Closest server (Twitch only)

LOBS times a connection to each Twitch server three times and keeps the best result. A server that doesn't answer within 2.5 seconds shows **no answer**. The list is sorted by response time, and the fastest is marked **Fastest** and selected. Pick a different one if you like, or click **Check again**.

![Server list sorted by response time](shot:setup-server)

### Test your connection

On Twitch, click **Start test** for a 10-second test stream. Nothing goes live. The test needs your stream key ("Add your stream key first").

- The first 2 seconds don't count.
- The result shows your upload speed in Mbps and the percentage of frames dropped.
- If the server doesn't accept the stream within 8 seconds, the test fails with "The server didn't accept the test stream. Check the stream key."
- Click **Stop** to end early, or **Test again** to repeat.
- Until a test has run, **Continue** reads **Skip test**. A skipped test assumes 10 Mbps.

![Connection test result](shot:setup-test)

LOBS can only test Twitch without going live. For YouTube and other services, set **Upload speed** on the slider (1 to 100 Mbps, default 10).

### Video and bitrate

LOBS suggests a bitrate of 75% of your upload speed, rounded down to the nearest 100 kbps. It stays between 1500 kbps and a cap of 6000 (Twitch), 9000 (YouTube) or 8000 (other services). The resolution and frame rate follow the bitrate:

| Bitrate (kbps) | Suggested output |
|---|---|
| under 2500 | 852×480 at 30 fps |
| 2500 to 3499 | 1280×720 at 30 fps |
| 3500 to 4499 | 1280×720 at 60 fps |
| 4500 to 5799 | 1600×900 at 60 fps |
| 5800 and up | 1920×1080 at 60 fps |

Without a hardware encoder, a 1080p suggestion drops to 720p and the page says "No hardware encoder found. 720p is lighter on your CPU."

You can change each suggestion:

- **Resolution**: 1920×1080, 1600×900, 1280×720 or 852×480.
- **Frame rate**: **60 fps (games, motion)** or **30 fps (talking, slides)**.
- **Stream bitrate**: a slider from 1.5 Mbps up to 6 Mbps on Twitch, or 12 Mbps elsewhere. Above 80% of your measured upload, it warns "Close to your measured upload."

### Recording

- **Save to**: the recordings folder. Click **Browse…** to change it.
- **Quality**: **High (moderate files)**, **Indistinguishable (very large files)** or **Same as stream (small files)**.
- **Format**: **MP4 (plays everywhere)**, **MKV (survives crashes)** or **MOV**.

### Twitch panels

Two tiles, both on by default: **Chat** ("Read and reply to chat.") and **Stream info** ("Title, category and tags."). The ones you keep are docked on the right when you finish. See [Twitch panels](/docs/workspace/#twitch-panels).

### You're set

This page shows a summary of your choices. Click **Finish** to save them. Closing the assistant during a connection test stops the test.

## Where files live

| What | Where |
|---|---|
| The app and its OBS engine | `%LOCALAPPDATA%\Programs\LOBS` (unless you picked another folder) |
| Startup log, rewritten on every start | `lobs.log` in the app folder |
| Your working show, autosaved | `%APPDATA%\LOBS\LOBS\current.lobs.json` |
| Stream, video, recording and hotkey settings | `%APPDATA%\LOBS\LOBS\settings.json` |
| Panel layout | `%APPDATA%\LOBS\LOBS\layout.json` |
| Assets, while the show isn't saved as a project | `%APPDATA%\LOBS\LOBS\assets\` |
| Engine update downloads | `%LOCALAPPDATA%\LOBS\LOBS\downloads\` |
| Recordings | `Videos\LOBS` (change it in [Settings > Recording](/docs/settings/#recording)) |
| New projects | `Documents\LOBS Projects` (see [Projects and assets](/docs/projects-and-assets/)) |

Your stream key is encrypted with your Windows login. Only your Windows user on this PC can read it back.

**Settings > About** lists the **Shows and settings** folder and the **Recordings** folder.

## Uninstall

Use **Uninstall LOBS** in the Start menu or Windows Settings. It removes LOBS, its OBS engine (including any engines downloaded by [engine updates](/docs/obs-engine-updates/)) and `lobs.log`. Your shows, settings and recordings are stored outside the app folder, in the places listed above.
