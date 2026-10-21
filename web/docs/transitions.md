---
title: Transitions
order: 6
description: Choose and edit the transition that plays when you switch scenes.
---

A transition plays when the program switches scenes. Transitions are saved in the show, and one of them is in use at a time. A new show has **Fade** (300 ms, in use) and **Cut**.

The in-use transition plays when you click **Show**, double-click a scene, or press a scene's [hotkey](/docs/settings/#hotkeys).

## Switching transitions

The canvas header has a button that shows the transition in use and its length, for example **Fade · 300 ms**. Click it to switch to another transition or to open **Edit transitions…**.

![The transition menu in the canvas header](shot:transition-menu)

## The Transitions dialog

![The Transitions dialog with its live preview](shot:transitions)

The list on the left marks the transition in use with **In use**.

- Click a transition to preview and edit it.
- Double-click a transition to use it for **Show**.
- **+ Add transition** adds **Fade**, **Cut**, **Swipe**, **Slide**, **Fade to color**, **Luma wipe** or **Stinger**. A second one of the same type gets a number, such as "Swipe 2".

On the right:

- A live preview from a blue A card to a green B card. It replays after each change, and **Preview** replays it on demand.
- **Name**.
- **Use for Show** puts this transition in use ("Show plays this transition when it switches scenes"). It reads **In use** when it already is.
- Delete. A show needs at least one transition, so the last one can't be deleted.

Click **Done** to close.

## Settings by type

New transitions start at 500 ms, except Cut (instant) and Stinger (1000 ms). **Duration** is a slider from 100 ms to 3 s in 50 ms steps, on every type except Cut and Stinger.

| Type | Settings |
|---|---|
| **Fade** | **Duration** |
| **Cut** | None. It switches instantly, with no animation. |
| **Swipe** | **Duration**; **Direction** left, right, up or down (default left); **Swipe in**, which moves the new scene over the old one (default off) |
| **Slide** | **Duration**; **Direction** left, right, up or down (default left) |
| **Fade to color** | **Duration**; **Color** (default black); **Switch point** 0 to 100% (default 50%) |
| **Luma wipe** | **Duration**; **Pattern**, the wipe image (default a horizontal linear wipe); **Softness** 0 to 1 (default 0.03); **Invert** |
| **Stinger** | **Video**; **Switch scenes at** 0 to 5000 ms (default 500); **Audio**; **Scene audio** |

### Stinger

A stinger plays a video over the switch. The scene changes at the time set in **Switch scenes at**.

- **Video**: use a video with transparency (.webm or .mov). **Browse** also accepts .mp4, .mkv and .avi.
- **Audio**: **Viewers hear it**, **Only you hear it** or **Silent** (default).
- **Scene audio**: **Fade out, then in** or **Crossfade**.
