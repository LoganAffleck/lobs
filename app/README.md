# LOBS v0.1

A scene editor that drives OBS Studio (28+) over its built-in WebSocket server. See `../PLAN.md` for the design.

## Run

```bash
npm install
npm run dev
```

Open http://localhost:5317. The document autosaves to the browser; **Save** and **Open** handle `.lobs.json` files.

## Connect OBS

In OBS, open **Tools → WebSocket Server Settings**, enable the server, then click **Connect OBS** in LOBS. On connect and after every edit, LOBS pushes the document into OBS:

- Each source → one OBS input, kept in a hidden `[LOBS] Pool` scene.
- Each layer → a private scene `[LOBS] ins_…` that wraps the shared input. Crop goes on the inner item; filters and opacity go on the private scene. That gives every layer its own filters while the device is captured only once.
- Each LOBS scene → an OBS scene with the same name. LOBS replaces that scene's contents. Scenes LOBS didn't create are left alone.

## Shortcuts

| Keys | Action |
|---|---|
| Ctrl+Z / Ctrl+Shift+Z | Undo / redo |
| Ctrl+C / Ctrl+V | Copy / paste layers (asks Linked or Independent when pasting into another scene) |
| Ctrl+D | Duplicate layer |
| Ctrl+G / Ctrl+Shift+G | Group / ungroup |
| Arrows (+Shift) | Nudge 1px (10px) |
| Alt + drag | Move layers while live (live lock) |
| Shift + resize | Free aspect · Ctrl + drag: no snapping |

## OBS plugin (recommended)

`../obs-plugin` builds `lobs.dll`, which adds a **LOBS Instance** source. With it installed, each layer is a single source instead of a helper scene, so your Scenes dock stays clean.

```bash
cmd /c ..\obs-plugin\build.bat
```

Copy `obs-plugin/build/lobs.dll` to `C:\ProgramData\obs-studio\plugins\lobs\bin\64bit\` and restart OBS. LOBS detects it automatically and migrates existing layers on the next sync.
