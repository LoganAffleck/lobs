---
title: Inspector
order: 4
description: Set a layer's position, size, crop, corners, filters and visibility, and link those settings across scenes.
---

The Inspector edits whatever you select: a layer, a group, several layers, or a source from the Sources panel. With nothing selected it reads "Select a layer or source to edit it." When a drawing tool is active and nothing is selected, it shows that tool's defaults (see [Graphic tools](/docs/graphic-tools/#tool-defaults)).

![The Inspector with a layer selected](shot:panel-inspector)

A layer's settings come in sections: **Transform**, **Crop & corners**, **Filters** and **Visibility**. Each applies to this layer in this scene only, unless you [link it](#linking-sections).

## Source section

At the top of a layer, a collapsible **Source · &lt;name&gt;** section holds the shared input's settings (device, URL, game mode and so on). It's closed by default and shows a one-line status or problem while closed.

- The badge reads **Used in N scenes** or **Shared input**.
- Changes here apply to the input everywhere. Size, crop and filters stay per scene.
- **Open in Sources** jumps to the source in the Sources panel.

Text, color, image, media and shape layers have their own content, so their fields (text, font, fill and so on) appear inline instead.

## Transform

| Field | What it sets |
|---|---|
| **Position** (**X**, **Y**) | Where the layer's top-left corner sits, in canvas pixels. |
| **Size** (**W**, **H**) | In canvas pixels. Minimum 1. |
| **Rotate** | -360 to 360 degrees. |

- The lock between **W** and **H** keeps proportions. It's on by default, and LOBS remembers your choice.
- The button at the end of **Position** centers the layer on the canvas.
- Under **Size**: **Fit** fits the layer inside the canvas, keeping proportions, and **Fill** covers the canvas. Both also center the layer and reset rotation. **Original** restores the source's own proportions.
- The two buttons beside **Rotate** flip the layer: **Mirror left to right** and **Mirror top to bottom**.

For a group, only **X** and **Y** apply. They move the whole group.

### Number fields

- Type math: `+`, `-`, `*`, `/` and parentheses work. For example, type `1920/2` in **X**.
- Up and Down change the value by 1. Hold Shift for 10.
- Drag sideways to scrub. Shift scrubs 10 times faster, Ctrl 10 times slower. A plain click (no drag) puts the cursor in the field for typing.
- One scrub is one undo step.

## Crop & corners

Crop is available once the source has reported its size. Text, shape and color layers can't be cropped, so for them this section is titled **Corners**.

![Cropping a layer on the canvas](shot:crop)

- **Crop** turns on crop mode. Drag the edges or corners on the canvas. Click **Done**, or press Enter or Esc, to finish. On a mirrored layer, the edge you drag is the one that's cropped.
- The small button beside it names the current crop (**None**, **16:9**, ..., **Custom**) and opens the presets and exact values:
  - Presets: **None**, **16:9**, **4:3**, **1:1** (Square) and **9:16** (Vertical). A preset crops around the center and keeps the layer's scale.
  - **L**, **T**, **R** and **B** set the crop from each edge of the source, in source pixels.
- **Corners** runs from 0 to 50% of the layer's short side. It reads **Square** at 0 and **Round** at 50 (a circle or a pill).

Rectangles round their own corners, so the stroke follows the curve.

## Filters

Click **Add filter…** to add a video filter to this layer. These are OBS's own filters, with OBS's settings and defaults. Each filter card has an on/off switch and **Remove filter**.

| Filter | Parameters (range, default) |
|---|---|
| **Color Correction** | Gamma (-3 to 3); Contrast (-4 to 4); Brightness (-1 to 1); Saturation (-1 to 5); Hue shift (-180° to 180°), all default 0; Opacity (default 100%); Color multiply (default white); Color add (default black) |
| **Chroma Key** | Key color (Green, Blue, Magenta or Custom color, default Green); Custom color (shown for Custom color, default #00ff00); Similarity (1 to 1000, default 400); Smoothness (1 to 1000, default 80); Spill reduction (1 to 1000, default 100); Opacity (default 100%); Contrast (-4 to 4), Brightness and Gamma (-1 to 1), default 0 |
| **Color Key** | Key color (Green, Blue, Red, Magenta or Custom color, default Green); Custom color; Similarity (1 to 1000, default 80); Smoothness (1 to 1000, default 50); Opacity, Contrast, Brightness and Gamma as in Chroma Key |
| **Luma Key** | Luma max (default 1); Luma max smooth (default 0); Luma min (default 0); Luma min smooth (default 0), all 0 to 1 |
| **Image Mask/Blend** | Type (alpha mask from the color or alpha channel, or blend by multiply, add or subtract; default color channel); Image (file); Color (default white); Opacity (default 100%); Stretch image (default off) |
| **Sharpen** | Sharpness (0 to 1, default 0.08) |
| **Scroll** | Horizontal and Vertical speed (-500 to 500, default 0); Limit width and Limit height (off), with Width and Height in px (default 100) when on; Loop (default on) |
| **Scaling/Aspect Ratio** | Scale filtering (Point, Bilinear, Bicubic, Lanczos or Area, default Bicubic); Resolution (None, Canvas size, an aspect ratio or a size, default None); Undistort center (Bicubic and Lanczos only, default off) |
| **Apply LUT** | LUT file (.cube or .png); Amount (0 to 1, default 1) |
| **Render Delay** | Delay (0 to 500 ms, default 0) |

Filters imported from OBS that LOBS doesn't support are kept, but have no controls. See [Importing from OBS](/docs/importing-from-obs/).

## Visibility

- **Visible** shows or hides the layer.
- **Opacity** runs from 0 to 100%.

Both work on several selected layers at once.

## Section header controls

Each section header (Transform, Crop, Filters, Visibility) can show:

- **Reset** (the rotate icon), when the values differ from the defaults. Its tooltip reads "Reset &lt;section&gt;".
- The **link** button, when the same source is placed somewhere else and this section isn't linked.
- A **Linked · &lt;Scene&gt;** chip (or **Linked · N scenes**), when it is linked.

## Linking sections

Linking makes two or more layers of the same source share one section. Edit the section on either layer and both change. For example, link **Filters** so a camera's color correction matches in every scene, while each scene keeps its own size and position.

![The link menu](shot:link-menu)

To link:

- Click the link button on a section.
- Pick **Link with &lt;Scene&gt;**. Other placements in the same scene also appear, marked **(this scene)**.
- Choose **Keep this scene's values** or **Use &lt;Scene&gt;'s values**.

The toast reads "&lt;Section&gt; linked with &lt;Scene&gt;". If the other layer is already linked with others, this layer joins that group. If this layer was in a different group, the two groups merge.

To unlink, click the **Linked** chip. The menu lists every **Shared with &lt;Scene&gt;** and **Unlink in this scene**. Unlinking keeps the current values as this layer's own. When only one layer is left in a link group, it goes back to its own values.

On the canvas, if you select several layers that share a linked transform, the move applies once.

## Several layers

With more than one layer selected, the Inspector title reads **N layers**. Changes to **Visible** and **Opacity** apply to all of them.

## Source mode

Select a source in the Sources panel to edit its input. The caption reads "&lt;Kind&gt; · input settings apply to every scene that uses it. Size, crop and filters are set per layer."

Below the input fields, **Used in** has one button per placement. Click one to jump to that scene and select the layer. A source that isn't placed anywhere says so and suggests dragging it onto the canvas. An audio source notes that it is always mixed in.
