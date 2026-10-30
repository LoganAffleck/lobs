# 0.7.0 hand test

The checks only a person can do, for everything since 0.6.5. The automated tests already cover the
document logic (layouts, following, persistence, click modes); this guide is for what they can't
see: real services, real devices, how things look and feel.

Use the dev copy (`native\dist\LOBS\LOBS.exe`). Tick each box; write anything odd under **Notes**
at the end, with the step number.

About 60–90 minutes. Sections 1–3 matter most for the release; do those first if time is short.

---

## 1. Vertical stream (P3-1)

**Setup:** a show with at least 3 scenes: one with a camera + game/window capture, one with a full
color background + text, and one made before today (if you still have one).

### Turning it on
- [ ] 1.1 With Vertical off, the Video panel header shows **+ Vertical**. Click it: a
      Horizontal/Vertical switch replaces it, and every scene gets a vertical layout.
- [ ] 1.2 Settings > Video > Vertical canvas shows it as on. Turning it off there brings back
      **+ Vertical** in the header.
- [ ] 1.3 Turning Vertical on is refused (or disabled) while live.

### Editing
- [ ] 1.4 Switch to Vertical: the canvas is tall (1080 × 1920), and the header still names the
      scene (not "… (Vertical)").
- [ ] 1.5 The other layout shows live in a corner of the Video panel. Clicking it switches layouts.
- [ ] 1.6 The Show button doesn't appear when the vertical layout of the live scene is open.
- [ ] 1.7 A full-canvas color background fills the whole tall canvas (no black bars).
- [ ] 1.8 A full-screen game or window capture is fitted to the width (not cropped).

### Following (desktop → vertical, one way)
- [ ] 1.9 In Horizontal, move the camera: in Vertical it moves too (scaled).
- [ ] 1.10 In Vertical, move the camera somewhere else: Horizontal doesn't change.
- [ ] 1.11 Back in Horizontal, move the camera again: the Vertical position you set stays.
- [ ] 1.12 Hide a layer in Horizontal: it hides in Vertical too.
- [ ] 1.13 Add a layer in Horizontal: it appears in Vertical. Delete it: it's gone from both.
- [ ] 1.14 Reorder layers in Horizontal: Vertical follows the order.
- [ ] 1.15 Undo after a Vertical-only move: it undoes just that move, and nothing in Horizontal.

### Older scenes
- [ ] 1.16 In a scene from before today, every layer is there **once** in Vertical (no
      duplicate smile). Moving it in Horizontal moves the Vertical one.

### Switching scenes in Vertical
- [ ] 1.17 While in Vertical, click another scene: the switch stays on Vertical and shows that
      scene's vertical layout.
- [ ] 1.18 Make a new scene while in Vertical, then click it: it opens in Vertical with its layers.

### Saving
- [ ] 1.19 Save, close LOBS, reopen: Vertical is still on and every vertical layout, including
      positions you changed there, is as you left it.

### Going live on Twitch (the part tests can't do)
- [ ] 1.20 With Enhanced Broadcasting on, Settings or the stream panel shows Twitch's plan
      with a vertical rendition (e.g. "1080p +4 Vertical").
- [ ] 1.21 Go live. In the Twitch app on a phone, the stream offers the vertical view and it
      shows the vertical layout, not a squashed desktop.
- [ ] 1.22 Switch scenes while live: both the desktop and vertical streams switch together.
- [ ] 1.23 Stats stay steady for 5 minutes (no dropped frames climbing).
- [ ] 1.24 Stop the stream, close LOBS: it exits cleanly (no crash dialog, no stuck process).

## 2. Switching scenes (P3-2b)

- [ ] 2.1 Click a scene: it goes live right away (with the transition), like OBS.
- [ ] 2.2 Double-click a scene: it goes live too.
- [ ] 2.3 Right-click a scene: the menu has **Edit before showing**. Pick it: the scene opens to
      edit, the live picture doesn't change, and **Show** appears in the header.
- [ ] 2.4 Click **Show**: it goes live.
- [ ] 2.5 Right-clicking a scene by itself doesn't change what's open or live.
- [ ] 2.6 Settings > Video > **Clicking a scene shows it** off: a click opens the scene without
      putting it live; Show or a double-click does. **Edit before showing** is gone from the menu.
- [ ] 2.7 The setting is kept after closing and reopening LOBS.
- [ ] 2.8 Run setup with "Bring over my OBS setup" and OBS's Studio Mode **on**: the setting
      ends up off. Repeat with Studio Mode **off**: it ends up on. (OBS's Studio Mode is the
      button under its Controls dock; OBS must be closed when setup reads it.)
- [ ] 2.9 Read the new text (menu item, setting title, both descriptions). Is it clear what
      a click will do?

## 3. Twitch sign-in (P1-7)

- [ ] 3.1 Settings > Stream > Twitch: sign in. The browser goes to Twitch, then back to
      streamlobs.com, then LOBS says you're signed in with your channel name.
- [ ] 3.2 Close and reopen LOBS: still signed in, no new browser trip.
- [ ] 3.3 Sign out, sign in again: it works the second time.
- [ ] 3.4 Cancel on Twitch's page: LOBS says it was cancelled and stays usable.
- [ ] 3.5 Go live with the signed-in account: no stream key typed anywhere.

## 4. Streaming quality and tests (P1-1 to P1-4)

- [ ] 4.1 With the bitrate at 6000 for Twitch, stats show about 5900 for video, steady.
- [ ] 4.2 Run a test stream: it reports a result and nothing appears on your channel.
- [ ] 4.3 Stats panel: frames, bitrate and drops update while live, and make sense.
- [ ] 4.4 Help (or the logs folder) holds the last 10 session logs; older ones are gone.
- [ ] 4.5 Enhanced Broadcasting on with Vertical off: the plan shows only horizontal renditions.

## 5. Setup from OBS (P1-5)

- [ ] 5.1 Setup finds OBS and lists its profiles and scene collections, with "(last used)" right.
- [ ] 5.2 After bringing it over, the scenes, sources, stream settings and plugins work. A
      third-party plugin's source shows in Add Source.
- [ ] 5.3 OBS still opens afterwards with everything as it was.

## 6. Remote control and recovery (P2-2, P2-3)

- [ ] 6.1 OBS Web (or another obs-websocket app) connects with the saved password, lists scenes,
      and switches them.
- [ ] 6.2 Pull the network cable (or turn Wi-Fi off) for 20 s while live: LOBS says it's
      reconnecting, then it's back live without you doing anything.

## 7. Profiles (P2-4)

- [ ] 7.1 Top bar > Profile: make a new one, name it in the popover.
- [ ] 7.2 Change the bitrate in it, switch back: the first profile keeps its bitrate.
- [ ] 7.3 Rename and delete a profile; deleting the one in use is handled sensibly.

## 8. Recording (P2-5)

- [ ] 8.1 Record, pause, resume, stop: one file, no gap or glitch at the pause.
- [ ] 8.2 With MKV chosen, the finished recording also appears as an MP4 that plays.

## 9. Plugin docks (P2-6)

- [ ] 9.1 Downstream Keyer opens from the Window menu and docks under Layers.
- [ ] 9.2 It tabs with other panels, floats, and docks back.
- [ ] 9.3 Its scene list shows real scene names, and a keyed scene shows over the live picture
      in the preview and on stream.
- [ ] 9.4 Open a menu or dialog over it: the menu isn't hidden behind the plugin panel.

## 10. Small things

- [ ] 10.1 Rename a scene and a layer in place (double-click the name, type, Enter / Esc).
- [ ] 10.2 Settings: the window drags from its top edge; the top bar hides its controls while
      Settings is open; the LOBS tooltip doesn't show over it.
- [ ] 10.3 Tabs in Settings show the selected one immediately when clicked.
- [ ] 10.4 Panels opened from the Window menu land under Layers.
- [ ] 10.5 Saved stream key: shown as saved, can be replaced and removed.
- [ ] 10.6 Close LOBS with browser sources and panels open: no crash on exit.

---

## Notes

| Step | What happened | Expected |
|------|---------------|----------|
|      |               |          |
