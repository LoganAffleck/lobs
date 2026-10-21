---
title: Panels, history and projectors
order: 13
description: Arrange and dock panels, undo with the History panel, use the Twitch panels and open projectors.
---

The LOBS window is a fixed canvas in the center with dockable panels around it. This page covers arranging panels, undo and history, the Twitch panels and projectors.

## Panels

The panels are **Scenes**, **Sources**, **Assets**, **Layers**, **Inspector**, **Audio Mixer**, **History**, **Chat** and **Stream info**.

The default layout:

- Left: Scenes, then Sources (with Audio Mixer as a tab), then Assets.
- Right: Layers, then Inspector (with History as a tab).
- Center: the canvas.

To rearrange:

- Drag a panel's tab grip to move it. A blue preview shows where it will land.
- Drop it on another panel to tab the two together.
- Drag tabs along the strip to reorder them.
- Double-click a tab to float the panel in its own window.
- Hover a tab to show its close button.

The active tab has a help button with tips for that panel.

![A panel's help card](shot:help-card)

LOBS saves the layout every 3 seconds and restores it on start. If a saved layout has no panels open, LOBS restores the default.

### Window menu

The **Window** menu has one item per panel, checked when it's shown. Click one to show or hide that panel. **Show all panels** opens all of them, and **Reset layout** goes back to the default.

![The Window menu](shot:window-menu)

## Undo and history

Every edit is a named step, up to 200 steps. A drag, a scrub or a slider move counts as one step.

- Ctrl+Z undoes. Ctrl+Shift+Z or Ctrl+Y redoes.
- The toast reads "Undo: &lt;step&gt;" or "Redo: &lt;step&gt;".
- The undo button in the top bar shows the next step in its tooltip.

The **History** panel lists every step with its time. Click a step to jump to it. Later steps are dimmed and can still be redone.

![The History panel](shot:history)

These aren't undo steps: selection, the current tool, panel and view changes, the laser, file operations in Assets, and settings.

## Twitch panels

**Chat** and **Stream info** are Twitch pages docked as panels. **Chat** lets you read and reply to chat. **Stream info** edits your title, category and tags. Both start closed and dock on the right as tabs. Turn them on in the [setup assistant](/docs/getting-started/#twitch-panels) or open them from the **Window** menu.

- Sign in to Twitch inside the panel once, and it stays signed in.
- Both panels use the same channel. With none set, the panel asks for one and offers **Open**.
- The toolbar has a zoom badge (Ctrl+plus and Ctrl+minus zoom; click the badge for 100%), **Reload**, **Open in your browser** and **Change channel**. Each panel remembers its zoom.

## Projectors

A projector shows the program or a scene full screen on another display, or in a window.

![Opening a projector from the canvas](shot:projector-menu)

- Right-click the canvas.
- Choose **Open projector** for the program, or **Open scene projector** for the scene you're editing.
- Pick a screen (listed as "&lt;model&gt;: W×H") to open it full screen with the cursor hidden, or **New window** for a 960×540 window.

Esc closes a projector. Double-click a full-screen projector to turn it into a normal window. Projectors close when you close LOBS.
