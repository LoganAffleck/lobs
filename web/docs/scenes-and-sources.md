---
title: Scenes and sources
order: 2
description: How sources, scenes and layers relate, and how to use the Scenes and Sources panels.
---

A source is an input such as a camera, a screen capture or a browser page. You add it once, and every scene can use it. Each scene places it as a layer, and each layer has its own size, crop, filters and visibility.

![The editor with Scenes and Sources on the left](shot:editor)

## The core idea

- **Sources** are shared. Changing a camera's device or a browser source's URL changes it in every scene.
- **Layers** belong to one scene. Moving, cropping or filtering a layer changes only that scene.
- When you want two scenes to match, you can link a layer's sections across scenes. See [Linking sections](/docs/inspector/#linking-sections).

The Sources panel lists inputs only. Image, video and audio files live in [Assets](/docs/projects-and-assets/#assets-panel). Text and shapes are drawn with the [graphic tools](/docs/graphic-tools/) and appear as layers.

## Editing vs. showing

The scene you edit and the scene on program (the one viewers see) can differ.

- Click a scene to edit it. The program doesn't change.
- Double-click a scene to edit it and show it.
- Click **Show** in the canvas header to put the edited scene on program with the current [transition](/docs/transitions/). The toast reads "On program: &lt;scene&gt;". **Show** is hidden when the edited scene is already on program.

## Scenes panel

- **List** and **Grid with live thumbnails** switch the view. Grid thumbnails refresh every second. While the grid is shown, every scene keeps running so its thumbnail stays live.
- **+ Scene** adds a scene named **Scene** (numbered if the name is taken) with a black Background layer.
- Each row shows its layer count. The program scene is marked **Showing**, or **On air** in red while you stream or record.
- In the grid, the program scene has a red border and the edited scene a blue one.

![Scenes as a grid of live thumbnails](shot:scenes-grid)

Right-click a scene for:

| Item | What it does |
|---|---|
| **Show this scene** | Puts it on program. |
| **Rename…** | Renames it. |
| **Duplicate with linked layers** | Copies every layer and links each copy's Transform, Crop, Filters and Visibility to the original. |
| **Duplicate** | Copies every layer, including the background, as independent layers. |
| **Delete scene** | Deletes it right away, with no confirmation. Ctrl+Z brings it back. |

Duplicates are named "&lt;name&gt; copy". In a linked duplicate, layers inside groups are linked but the groups themselves aren't.

## Sources panel

Sources are grouped into **Cameras**, **Screen capture**, **Audio** and **Media** (browser sources).

- The **All**, **Video** and **Audio** tabs filter the list. In **All**, video sources come first, then audio.
- Search matches the name, the device and the kind.
- **+ Source** opens **Add a source**.

Each video row has a live thumbnail. Each audio row has a small stereo meter and a mute button.

### Placing a source

- Drag a video source onto the canvas to place it where you drop it. A source that fills the canvas drops full-canvas.
- Double-click a source to place it in the edited scene.
- The dot on a video row shows whether it's in the edited scene. A green **In scene** dot reads **Remove** on hover. Otherwise the row reads **Add to scene** on hover.
- Removing a source from a scene deletes every layer of that source in the edited scene.

Single-click a source to open its input settings in the [Inspector](/docs/inspector/#source-mode).

Audio sources aren't placed on the canvas. They play in every scene (see [Global and scene audio](#global-and-scene-audio)).

### Source menu

Click **…** or right-click a source:

- **Add to &lt;scene&gt;** or **Remove from &lt;scene&gt;**.
- **Settings…** and **Rename…**.
- A line telling you where it's used: **Plays in every scene** (audio), **Used in A, B**, or **Not used in any scene yet**.
- **Delete source** deletes the source and all its layers in every scene.

## Add a source

![The Add a source dialog](shot:add-source)

- Pick a kind: **Camera**, **Display**, **Window**, **Game**, **Browser**, **Mic** or **Desktop**.
- **Name (optional)** defaults to the kind's name.
- For **Browser**, enter the page address. LOBS adds `https://` if you leave it out.
- **Also place on &lt;scene&gt;** is on by default. It's hidden for **Mic** and **Desktop**.
- Click **Add source**.

Cameras, displays, mics and desktop audio start on your PC's first real device. Browser sources start with monitoring on, so you hear alerts as well as your viewers.

## Source kinds

| Kind | Default size | Has audio | How you add it |
|---|---|---|---|
| Camera | 1280×720 | no | **Add a source** |
| Display Capture | canvas | no | **Add a source** |
| Window Capture | canvas | no | **Add a source** |
| Game Capture | canvas | no | **Add a source** |
| Browser | 1920×1080 | yes | **Add a source** |
| Microphone | not placed | yes | **Add a source** |
| Desktop Audio | not placed | yes | **Add a source** |
| Image | the file's own size, fitted to the canvas | no | [Assets](/docs/projects-and-assets/#assets-panel) |
| Media File | 1280×720 | yes | [Assets](/docs/projects-and-assets/#assets-panel) |
| Text | sized as you draw | no | [Text tool](/docs/graphic-tools/#text) |
| Shape | 400×240 (click without dragging) | no | [Shape tools](/docs/graphic-tools/) |
| Color | canvas | no | The Background layer, or [imported from OBS](/docs/importing-from-obs/) |

Image, media, text, color and shape sources are deleted automatically once no layer uses them.

### Input settings by kind

| Kind | Settings |
|---|---|
| Camera, Display, Window, Mic, Desktop | **Choose device…** or **Choose window…**. The list refreshes when you open it. |
| Game | Mode: **Any fullscreen game**, **A specific window** (with **Choose window…**) or **Foreground window (hotkey)**. |
| Browser | URL, **Reload the page**, and **Page size** W × H (defaults to the canvas size). The page's audio goes to the [mixer](/docs/audio/). |
| Text | The text, a font picker, **B** (on by default), **I** and **Color** (white by default). |
| Color | One color (default #1d1b2e). |
| Shape | Rectangle and ellipse: **Fill** and **Stroke**, each can be **None**. Arrow and pencil: **Color**. **Stroke width** or **Thickness**, 0 to 48 px. |
| Image, Media | The file path. Media placed from Assets loops by default. |

Color fields open a picker with **Pick from screen** (Esc cancels), **Swatches** saved in the show (right-click a swatch to remove it) and **Recent**.

## Status and problem badges

The second line of each source row shows its status: the device name, **Windows default device**, **No device chosen**, **Any fullscreen game**, **A specific game window**, the browser page's host, **No URL yet** or **Local page**. Once the size is known, it's added (for example " · 1920×1080").

Problems show as amber badges, with an amber icon and border on the row. LOBS checks every second and re-lists devices every 30 seconds.

| Badge | Meaning |
|---|---|
| **Not connected** | The device is missing. |
| **No signal** | The input is running but sends no picture. |
| **Waiting for a game** | Game Capture is running and hasn't found a game yet. |
| **Window not found** | Window Capture is running and can't find its window. |

## Global and scene audio

- **Global** audio: Mic and Desktop Audio. Each plays in every scene, all the time. Pick their devices in [Settings > Audio](/docs/settings/#audio).
- **Scene** audio: media files and browser sources. You hear them only while a scene that contains them is running.

The [Audio Mixer](/docs/audio/) marks each source **Global** or **Scene**.
