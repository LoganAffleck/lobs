---
title: OBS engine updates
order: 12
description: How LOBS checks for, verifies, tests and rolls back new versions of the OBS engine.
---

LOBS runs on the OBS engine and can update it from OBS's official releases without a new LOBS installer. Every new engine is checked and tested before you use it, and LOBS goes back to the previous one if it fails to start.

![Settings > OBS engine](shot:settings-engine)

## Checking

LOBS checks about 5 seconds after it starts, at most once a day. It also checks when you click **Check now** in **Settings > OBS engine**, or when you turn **Include OBS betas** on or off.

The check asks GitHub for OBS Studio's public list of releases. Nothing about your PC is sent. LOBS takes the newest release with a Windows x64 download. Betas count only with **Include OBS betas** on.

A release isn't offered if:

- It isn't newer than the engine you have.
- An earlier check found it isn't compatible with this version of LOBS.
- It failed to start on this PC before ("…didn't start on this PC earlier, so it isn't offered again.").

While an update is available or in progress, a button in the top bar shows its state: **OBS X available**, **OBS X N%**, **Installing OBS X**, **Testing OBS X** or **Restart for OBS X**. Click it to open **Settings > OBS engine**.

## Installing

Click **Download and install**.

- The download goes to `%LOCALAPPDATA%\LOBS\LOBS\downloads`.
- LOBS checks its SHA-256 checksum against the one GitHub publishes. With no checksum, it stops: "No checksum for this download. Not installed." With a wrong one: "Checksum mismatch. Download discarded."
- LOBS keeps only the engine parts it uses and drops OBS's own interface.
- If the new engine needs a newer Qt than this LOBS has, LOBS warns that browser sources may not load until LOBS updates.

## Verifying

Before a new engine can be used, LOBS tests it:

1. It checks that the engine provides everything this LOBS needs. If not, the engine is deleted, marked incompatible, and not offered again ("isn't compatible with this LOBS yet").
2. It runs a hidden test with the new engine, for up to 2 minutes. The test builds a scene with color, text, a browser source and audio, plus a filter and an audio effect. It checks the rendered colors, mirroring and a fade transition, then records a 2-second clip.

When both pass, the status reads "OBS engine X passed LOBS's checks. Restart LOBS to use it." Click **Restart LOBS**. LOBS keeps the engine you were running and the new one, and deletes older ones.

## Trial and automatic rollback

The first starts on a new engine are a trial. A start counts as good once the window has been up for 5 seconds.

If the new engine hasn't had a good start after two launches, the third launch goes back to the previous engine and marks the new one as failed. LOBS tells you: "OBS engine X didn't start, so LOBS went back to Y." That version isn't offered again on this PC.

## Going back by hand

Under **Previous engine**, click **Go back to &lt;version&gt;**, then restart LOBS.

## Status messages

| Status | Meaning |
|---|---|
| **Checking for updates...** | A check is running. |
| **Up to date · checked …** | You have the newest engine. |
| **OBS X is available** | Click **Download and install**. |
| **Downloading OBS X... N%** | Downloading. |
| **Installing…** | Unpacking and verifying. |
| **Testing OBS X with LOBS...** | The hidden test is running. |
| **Restart LOBS to switch engines** | Click **Restart LOBS**. |
| **Staying on OBS …** | LOBS kept the current engine. |
| **Not checked yet** / **Last checked …** | When the last check ran. |

**What's new in OBS X** > **Open** shows the release notes on GitHub.
