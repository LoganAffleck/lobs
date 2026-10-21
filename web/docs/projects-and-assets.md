---
title: Projects and assets
order: 10
description: Create, open and save projects, understand autosave, and manage image, video and audio files in the Assets panel.
---

A project is a folder that holds a show file and the media it uses. The top bar has **New**, **Open**, **Import** and **Save**. Import is covered in [Importing from OBS](/docs/importing-from-obs/).

## Autosave and the working show

LOBS keeps one working show in `%APPDATA%\LOBS\LOBS\current.lobs.json`. It saves there 0.4 seconds after every change, and loads it on the next launch.

Autosave never writes your project file. The project file (`<Name>.lobs.json`) changes only when you click **Save**, or when **New** saves the current project before creating another. Save your project before you close LOBS if you want the project file to match what you see.

## New

![The New project dialog](shot:new-project)

**New** opens **New project**:

- **Name**: default "Untitled project".
- **Location**: default `Documents\LOBS Projects`, then whichever folder you used last. Click **Browse…** to change it.
- Click **Create project**.

LOBS creates this folder:

```
<Location>\<Name>\
  <Name>.lobs.json
  assets\
```

The new project starts with one scene and its Background layer. The toast reads "Created &lt;name&gt;".

If your current show is already a project, it's saved first ("Your current project is saved before the new one opens."). If it isn't, the dialog warns "Your current show isn't saved as a project. Save it first, or it will be replaced." and offers **Save as…**.

Names can't be empty, can't contain `< > : " / \ | ? *`, and can't end with a period. The location must not already have a folder with the same name.

## Open

**Open** (Ctrl+O) opens a show file (`.lobs.json`). LOBS copies any outside media the show uses into the project's `assets\` folder and selects the first scene.

If you pick an OBS scene collection, LOBS imports it instead. A file that is neither gives "Not a LOBS document".

## Save

**Save** (Ctrl+S) always asks where to save, so it works as Save As. The file you save becomes the project:

- Its folder's `assets\` becomes the Assets folder.
- Image and video files the show uses from elsewhere are copied into it.
- The toast reads "Saved &lt;file&gt;".

## Assets panel

The Assets panel shows the project's `assets\` folder. Before you save a project, it shows `%APPDATA%\LOBS\LOBS\assets\`. Changes you make in File Explorer show up right away.

![The Assets panel](shot:assets)

### Adding files

- Click **+ Add**, click the drop hint, or drag files in from File Explorer.
- Files are copied into the folder you're viewing. A dropped folder adds the files inside it (one level deep).
- A different file with the same name gets " (2)" added. An identical file is reused.

| Kind | Extensions |
|---|---|
| Image | png, jpg, jpeg, gif, webp, bmp, tga, psd |
| Video | mp4, mov, mkv, webm, avi, flv, ts, m4v |
| Audio | mp3, wav, ogg, flac, m4a, aac, opus |

Other files can be stored in Assets but not placed.

### Browsing

- **Grid** or **List** view. Sort by **Name**, **Date modified** or **Size**, ascending or descending. LOBS remembers both.
- Folders come first. The breadcrumbs and **Up one folder** (Backspace) move between folders.
- **Search all folders** (Ctrl+F) searches every subfolder and shows up to 500 results.
- Tiles show a thumbnail and a duration or extension badge. A green dot means the file is used in a scene. Hover for the scenes it's used in.

### Placing a file

Drag it onto the canvas, double-click it, press Enter, or choose **Place on &lt;scene&gt;**.

- Images keep their own size, scaled down to fit the canvas, centered.
- Videos are placed at 1280×720 and loop.
- Audio files become a tiny layer so the scene plays their sound. Adjust them in the [mixer](/docs/audio/).

Each file is one source. Placing it again adds another layer of the same source.

### Menus and keys

Right-click an item:

| Item | Keys |
|---|---|
| **Open** (folders) | |
| **Place on &lt;scene&gt;** | Enter |
| **Rename…** (the extension stays; layers follow the new name) | F2 |
| Folder color (9 swatches) | |
| **Copy** | Ctrl+C |
| **Cut** | Ctrl+X |
| **Paste here** | Ctrl+V |
| **Show in File Explorer** | |
| **Move to Recycle Bin** | Delete |

Right-click empty space for **New folder** (Ctrl+N), **Paste** (Ctrl+V), **Add files…** and **Show folder in File Explorer**. Ctrl+A selects everything.

Copy, cut and paste use the Windows clipboard, so they work with File Explorer too. A cut pastes once.

A file in use can't be moved to the Recycle Bin ("In use: remove its layers first"). Moving or renaming files inside Assets updates the layers that use them, including in the undo history.

### Missing files

If a file was moved or deleted outside LOBS, a banner reads "&lt;file&gt; is missing (moved or deleted outside LOBS)."

- **Find it** searches Assets for a file with the same name.
- **Choose file…** copies the file you pick into Assets and relinks the layers.

## Project file

A `.lobs.json` file holds the canvas, sources, scenes, layers, links, bins, swatches and transitions. Asset folders may contain a hidden `.lobsfolder` file that stores the folder's color.
