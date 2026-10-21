# Manual walkthroughs

These are the stories automation can't judge: feel, understanding, and real hardware. Use your
installed LOBS (or `native\dist\LOBS\LOBS.exe`). Each takes 3–10 minutes. Mark ☐ → ✅ or ❌, and
on ❌ write what you saw (a screenshot is best). Do them in order. **M-01 must be first**, before
you read anything else about linking.

**Before you start:** Save your current show (Save As…), then use **New** so you're on an empty
project. Your real show stays safe in its file.

---

## M-01 · Linking, cold (LK-11) · ~10 min · do this first
No help text, no tooltips (don't hover the "?" buttons). Time yourself.

1. Make two scenes, **Chatting** and **Gameplay**. Add your webcam as a source and put it in Chatting.
2. **Task A:** get the same webcam into Gameplay so that **resizing it in either scene resizes it in both**.
   - ☐ Done within 3 minutes · Time: ____ · How did you find the way? ____________
3. **Task B:** keep the size shared, but make the camera **blurry/darker only in Gameplay** (any filter).
   - ☐ Done within 3 minutes · Time: ____ · What did you try first? ____________
4. **Task C:** look at the Gameplay camera. Can you tell, without clicking anything, what's shared with Chatting and what isn't?
   - ☐ Yes · What told you? ____________
5. **Task D:** undo everything back to before Task A with Ctrl+Z. ☐ Both scenes back to how they were
- Rate 1–5: "I always knew whether a change would affect the other scene." ____

## M-02 · First run (SA-01) · ~5 min
Settings → (if there's a reset for the setup assistant use it), or ask me for a clean QA profile.
1. ☐ The setup assistant explains what it will do before it does it
2. Choose **Just record**. ☐ It finishes without asking for a stream key
3. ☐ After finishing, the editor has a usable scene (not a blank black canvas with no hint)
4. Restart LOBS. ☐ The assistant doesn't come back
5. Anything confusing, or any step with no "Back"? ____________

## M-03 · Drag from Sources onto the canvas (MP-02) · 2 min
1. Drag the webcam from **Sources** onto the right half of the canvas.
   - ☐ A drop target/outline shows while dragging · ☐ It lands where you dropped it
2. Start a drag and press **Esc** mid-drag. ☐ Nothing is added
3. Drag and release outside the canvas. ☐ Nothing is added

## M-04 · Canvas feel (CV-01, CV-02, CV-03) · 5 min
1. Resize a layer by a corner handle. ☐ Easy to grab (no pixel-hunting at your display scale) · ☐ Shift (or no-Shift) keeps aspect as you expected
2. Move a layer near the center and the edges. ☐ Snapping guides appear and help
3. Arrow keys nudge 1 px, Shift+Arrow 10 px. ☐
4. Crop with handles, then try the 16:9 / 4:3 / 1:1 / 9:16 presets. ☐ Results look right
5. Corner radius to max on a square layer. ☐ It's a circle

## M-05 · Graphic tools and laser (CV-04, CV-05) · 5 min
1. Text tool: click the canvas, type `Hello`, Backspace, type `World`. ☐ Caret and editing feel normal
2. Draw a rectangle, ellipse, arrow and pencil stroke. ☐ Each becomes a layer with the current colors
3. Scale a shape up 4×. ☐ Edges stay sharp
4. Start a recording, use the laser pointer for a few seconds, stop. ☐ The laser shows in the recording · ☐ No laser layer was added

## M-06 · Microphone and monitoring (AU-01, AU-03) · 5 min · headphones help
1. Add **Microphone (Scarlett Solo)**. Speak. ☐ Meter moves · ☐ Levels look stereo-correct (or mono where expected)
2. Record 10 s while speaking; play it back. ☐ Voice is there, not clipped or silent
3. Mute it. ☐ Meter shows muted · ☐ A recording while muted is silent
4. Set monitoring to **Monitor and output** with speakers (not headphones).
   - ☐ LOBS warns about echo / feedback · What did it say? ____________

## M-07 · Virtual camera in another app (OUT-03) · 3 min
1. Start **Virtual cam** in LOBS. Open the Windows Camera app or a browser camera test page.
   - ☐ "LOBS" (or OBS Virtual Camera) appears as a camera · ☐ It shows LOBS's output
2. Stop Virtual cam. ☐ The other app loses it cleanly (no frozen frame)

## M-08 · Docking and layout (DK-01, DK-03) · 5 min
1. Drag the Layers panel to float, dock it on the left, then tab it with Sources.
2. Resize panels. Restart LOBS. ☐ The layout comes back exactly
3. Find a "reset layout" option. ☐ Exists and works
4. Make the window as small as it goes. ☐ Nothing overlaps or gets cut off unusably

## M-09 · Global hotkeys (HK-01) · 3 min
1. Settings → hotkeys: bind **Record** to a key combo.
2. Focus another app (e.g., Notepad) and press it. ☐ Recording starts/stops
3. Try binding the same combo to a second action. ☐ LOBS warns about the conflict
4. Restart. ☐ The binding is still there

## M-10 · Twitch panels (TW-01) · 5 min · tell me when you're here; you sign in yourself
1. Open the Twitch chat and stream info panels. Sign in.
2. ☐ Chat loads and updates · ☐ Title/category edits apply on Twitch
3. Sign out. ☐ The panels clearly show you're signed out

## M-11 · Keyboard only (AX-02) · 5 min
Put the mouse away. Tab / Shift+Tab / Enter / Space / arrows only.
1. ☐ Add a source · ☐ Place it · ☐ Switch scenes · ☐ Start and stop a recording
2. ☐ You can always see where focus is
- Where did you get stuck? ____________

---

When you're done, paste your marks and notes back to me (or edit this file). I'll fold them into the report.
