---
title: Layers
order: 3
description: Order, group, lock and copy the layers that make up a scene.
---

The Layers panel lists everything placed in the scene you're editing. The layer at the top of the list draws on top. The panel header shows the scene's name and has **Group** (Ctrl+G), **Copy to another scene** and **Delete** (Delete).

![The Layers panel](shot:layers)

## Layer rows

Each row has a color dot, an icon and the name. The name is dimmed when the layer is hidden.

- A link icon means one or more sections are linked with another scene ("Linked with another scene").
- **N fx** shows how many filters the layer has.
- On hover or when selected: **Move up**, **Move down** and **Lock** / **Unlock**.
- The eye toggles **Hide** / **Show**.

Click a row to select it. Shift+click or Ctrl+click adds to the selection. Double-click to rename.

## Changing the order

- Drag a row. A line shows where it will land, above or below the target.
- Use **Move up** and **Move down** on the row.
- Use the keys or the right-click menu:

| Keys | Action |
|---|---|
| Ctrl+] | Bring forward |
| Ctrl+[ | Send backward |
| Ctrl+Shift+] | Bring to front |
| Ctrl+Shift+[ | Send to back |

## Groups

- Select layers and press Ctrl+G (or click **Group** in the header) to group them. The group is named **Group**, numbered if the name is taken.
- Ctrl+Shift+G ungroups.
- Drop a layer on a group to move it in (**Move into group**).
- Hold a dragged layer over another layer for 300 ms to make a **New group**.
- A group left empty by a move is deleted.

On the canvas, a click selects the top-level group. Each double-click steps one level deeper.

In the [Inspector](/docs/inspector/#transform), a group has only X and Y. They move the whole group.

## Lock

A locked layer can't be moved, resized or cropped on the canvas. Trying shows "Layer is locked". Click **Unlock** on the row to free it.

## Background layer

Every new scene has a **Background** layer, a black color fill.

- It stays at the bottom. You can't drag it or move other layers below it.
- It can't be deleted ("The background can't be deleted. Hide it instead."). Use the eye to hide it.

## Duplicate

Ctrl+D (or **Duplicate** in the right-click menu) makes an independent copy in the same scene, offset 20 px right and down.

## Copy to another scene

Click **Copy to another scene** in the header, pick a scene, then pick:

- **Linked copy**: the copy's Transform, Crop, Filters and Visibility stay linked to the original. Change one and the other follows.
- **Independent copy**: the copy starts with the same values and changes on its own from then on.

The toast reads "Copied to &lt;scene&gt; as linked" or "…as independent". You can link or unlink sections later in the [Inspector](/docs/inspector/#linking-sections).

![The copy menu with Linked copy and Independent copy](shot:copy-menu)

## Right-click menu

| Item | Keys |
|---|---|
| **Rename…** | |
| **Duplicate** | Ctrl+D |
| **Group selection** | Ctrl+G |
| **Ungroup** | Ctrl+Shift+G |
| **Bring to front** | Ctrl+Shift+] |
| **Bring forward** | Ctrl+] |
| **Send backward** | Ctrl+[ |
| **Send to back** | Ctrl+Shift+[ |
| **Delete** | Delete |

An empty scene shows "Empty scene. Drag a source from Sources onto the canvas."
