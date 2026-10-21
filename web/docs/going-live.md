---
title: Going live
order: 8
description: Stream, record, save replays and run the virtual camera, and read the status bar while you do.
---

The four output buttons sit at the top right: **Go live**, **Record**, **Replay** and **Virtual cam**. Each sends the program scene, the one marked **Showing**, not the scene you're editing.

![The editor while live](shot:editor-live)

## Go live

Click **Go live** to stream with the settings in [Settings > Stream](/docs/settings/#stream). Without a stream key you get "Add your stream key in Settings first" (a **Custom server** doesn't need one).

- While live, the button shows **LIVE** with the elapsed time and the bitrate in kbps. It turns amber and reads **reconnecting** when the connection is poor or dropped.
- If the connection drops, LOBS retries 20 times, 2 seconds apart.
- To end the stream, click the button twice within 3 seconds. The first click shows "Click again to end stream". The tooltip shows the dropped frame count.
- Your service may lower the bitrate to its own limit.

If the stream can't start or stops, LOBS shows one of these:

| Message | What to check |
|---|---|
| "Bad server address" | The server or Server URL in Settings > Stream. |
| "Couldn't connect to the server" | Your connection, or pick another server. |
| "Server rejected the stream key" | The stream key. Paste it again from your service. |
| "Disconnected from the server" | The connection dropped and didn't come back. |
| "Encoder error" | The encoder in [Settings > Video](/docs/settings/#video). Try another one. |

## Record

Click **Record** to start. The button becomes a red timer, and its tooltip shows the file size in MB. Click again to stop.

- Files go to the recordings folder (default `Videos\LOBS`) and are named by date and time, for example `LOBS 2026-10-20 18-30-05.mp4`.
- The format and quality come from [Settings > Recording](/docs/settings/#recording).
- Audio is AAC at 192 kbps. Recordings contain [audio track 1](/docs/audio/#monitoring-tracks-and-effects) only.

## Replay

**Replay** turns the replay buffer on or off. It keeps the last 30 seconds by default (change it in [Settings > Recording](/docs/settings/#recording)), at recording quality, up to 2048 MB.

While it runs, a **Save** button appears. It writes the buffer to the recordings folder as `Replay <date> <time>.<format>`, and the toast reads "Replay saved: &lt;file&gt;". Saving with the buffer off gives "Turn on the replay buffer first".

## Virtual cam

**Virtual cam** sends the program scene to other apps as a camera named **OBS Virtual Camera**. Pick it in your video call app.

The first time, LOBS sets up the camera driver. The tooltip reads "Set up the virtual camera (Windows asks for permission once)". Accept the Windows prompt to finish.

## Live lock

While you stream or record, the canvas locks so you don't move a layer by accident. A red banner reads "Live · layers are locked. Hold Alt to move one." (or "Recording · …").

While it's locked:

- Moving, resizing and cropping on the canvas need Alt held.
- Arrow-key nudges are blocked ("Unlock to nudge while live").
- Drawing on the program scene needs Alt held. The laser still works.

Click **Unlock** in the banner to turn it off, and **Lock again** to turn it back on. Starting a stream locks it again.

## Status bar

![Stats, opened from the status bar](shot:stats)

The bar along the bottom shows:

| Item | Shows |
|---|---|
| Engine status | Messages from the OBS engine. |
| **LIVE** | Time, kbps, and dropped frames (count and percentage). |
| **REC** | Time and file size in MB. |
| **Replay on** | The replay buffer is running. |
| **Virtual camera** | The virtual camera is running. |
| **CPU** | CPU use. Amber above 80%. |
| fps | Frames per second against the target, and render time in ms. Amber when more than 2 fps below target. |
| **Lagging x% · skipped y%** | Appears when frames missed (render lag) or skipped (encoder lag) pass 1%. |
| **N GB free** | Appears when the recordings drive has less than 10 GB free. |

The **Lagging** figure counts every frame since LOBS started, so a hiccup at startup keeps showing after it's over.

Click the status bar to open **Stats**: **Frame rate**, **Render time**, **Frames missed (render lag)**, **Frames skipped (encoder lag)**, **CPU**, **Memory**, **Free space for recordings**, **Stream**, **Dropped frames (network)**, **Recording**, **Replay buffer** and **Virtual camera**.

## Projectors

To show the program or a scene on another screen, right-click the canvas. See [Projectors](/docs/workspace/#projectors).
