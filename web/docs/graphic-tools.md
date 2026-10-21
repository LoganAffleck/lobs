---
title: Graphic tools
order: 5
description: Draw text, shapes, arrows and pencil lines on the canvas, point with the laser, and move layers with the mouse.
---

The toolbar under the canvas draws text and shapes straight into the scene you're editing. Each drawing becomes a layer. The laser pointer is the exception: viewers see it, but it isn't added to the scene.

![The graphic tools under the canvas](shot:tools)

## Tools

Tool keys work while the canvas has focus.

| Tool | Key | How it works |
|---|---|---|
| Select | V | Select and move layers. |
| Text | T | Click, or drag a box. |
| Rectangle | R | Drag. Shift makes a square. |
| Ellipse | O | Drag. Shift makes a circle. |
| Arrow | A | Drag. Shift snaps to 45 degrees. |
| Pencil | P | Draw freehand. |
| Laser | L | Point at things. Not added to the scene. |

- Esc goes back to Select.
- Picking a tool clears the selection.
- After you draw one shape or one text, the tool switches back to Select. Pencil stays active until you change tools.

Click without dragging to get a default size: a 400×240 rectangle, a 300×300 ellipse, a 300 px arrow pointing right, or text 96 px tall.

Shapes stay sharp at any size. While you drag, the preview shows the real shape.

## Tool defaults

With a tool active and nothing selected, the [Inspector](/docs/inspector/) shows the tool's defaults. Changes apply to the next thing you draw, and LOBS remembers them.

| Setting | Default | Used by |
|---|---|---|
| **Fill** | #ef4444 (red). **None** draws an outline only. | Rectangle, ellipse. Also the laser's **Color**. |
| **Stroke** | #ffffff (white) | Rectangle, ellipse. Also the color of arrows, pencil lines and text. |
| **Stroke width** / **Thickness** | 6 px (0 to 48) | Rectangle, ellipse, arrow, pencil |
| Font | Segoe UI, **B** on, **I** off | Text |
| **Trail length** | 0.5 s (0.15 to 2.00) | Laser |

To change a shape after drawing it, select it and edit its fields in the Inspector.

## Text

- Pick Text (T).
- Drag up or down on the canvas to set the line height, or click for 96 px.
- Type. The text appears on the canvas with a blinking caret, and the box widens to fit.
- Enter starts a new line. Backspace deletes. The arrow keys and Delete do nothing while you type.
- Finish with Esc, Ctrl+Enter, or a click somewhere else. Any Ctrl or Alt shortcut also finishes the text first, then runs.

If you type nothing, no layer is made. Text stays crisp when you resize it.

## Laser

![The laser pointer's fading trail](shot:laser)

The laser draws a fading trail on top of everything, and viewers see it. It works while you're live, and it never changes the show or the undo history. Set its color and **Trail length** in the Inspector.

## Live lock

While you stream or record, drawing on the scene that's on program needs Alt held: "Live lock is on. Hold Alt to add graphics." The laser isn't affected. See [Live lock](/docs/going-live/#live-lock).

## Canvas mouse and keys

![Canvas tips from the help card](shot:canvas-help)

| Action | How |
|---|---|
| Move a layer | Drag it. It snaps to the canvas edges and center, and to other layers' edges and centers. |
| Move without snapping | Hold Ctrl while dragging. |
| Resize, keeping proportions | Drag a corner. |
| Resize freely | Hold Shift while dragging a corner. |
| Add to the selection | Shift+click. |
| Step into a group | Double-click. Each double-click goes one level deeper. |
| Nudge | Arrow keys move 1 px, Shift+arrow 10 px. |
| Projectors | Right-click the canvas for **Open projector** and **Open scene projector**. See [Projectors](/docs/workspace/#projectors). |
| Preview scale | Click the **N%** button to switch between pixel-perfect (exact ratios, crisp text) and fit-to-panel. |
