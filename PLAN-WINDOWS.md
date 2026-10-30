# LOBS on Windows — Next plan

Written 2026-10-30, after 0.6.5, as user stories by priority. **P1:** a streamer notices it on
every stream, or it keeps them on OBS. **P2:** makes LOBS clearly better day to day. **P3:** worth
doing once P1 and P2 are in. Each story says when it's done. PLAN.md has the product plan,
PLAN-MACOS.md the Mac work.

## P1

### P1-1. My stream holds a steady bitrate
*As a streamer, when I go live my bitrate stays steady and Twitch doesn't call my stream unstable.*

A test stream swung between 3,000 and 6,000 kbps and Stream Manager called it unstable. Fps and
CPU were fine, and the connection is wired at 1 Gbit, so the cause is in LOBS.
- `makeVideoEncoder` (native/src/engine_output.cpp) sends NVENC `preset2`, which OBS 32 no longer
  reads (it reads `preset`, p1–p7), so the preset is ignored. Match OBS's NVENC setup: tune hq,
  multipass qres, profile high, 2 B-frames, lookahead, adaptive quantization, keyframes every 2 s.
- Check x264, AMF and Quick Sync the same way against OBS 32.2.2.
- Log the encoder and output settings when a stream starts, as OBS does.

**Done when** a test stream holds its bitrate, Stream Manager doesn't warn, and LOBS's logged
settings match OBS's.

### P1-2. I can see how my stream is doing, and look back at it
*As a streamer, I can tell while live whether my stream is healthy, and find out afterwards what
went wrong.*
- Bitrate averaged over a few seconds, dropped frames (network), congestion and encoder lag, in the
  status bar and the Stats panel.
- Keep the last few session logs instead of overwriting `lobs.log` at each launch.

**Done when** a dropped connection or an overloaded encoder shows in the stats while live, and the
log from that stream is still there after LOBS restarts.

### P1-3. I can test my stream without going live
*As a streamer, I can check my stream on Twitch before my viewers see it.*
- A Twitch test stream viewers can't see (a bandwidth test), started from Settings, with the P1-2
  stats showing.

**Done when** a test stream runs to Twitch and doesn't show on the channel.

### P1-4. My viewers can choose a quality (Enhanced Broadcasting)
*As a streamer, my viewers and VOD always get quality options, as they do with OBS's Enhanced
Broadcasting.*

Without it, Twitch makes lower qualities only when it has capacity; a LOBS test stream's VOD was
1080p only. The GPU encodes every quality itself instead.
- **How OBS does it:** before going live, it sends Twitch the hardware, canvas and preferences
  ("Go live POST", schema 2025-01-25, service IVS). Twitch answers with the qualities, bitrates,
  encoder settings and dynamic bitrate settings. OBS makes one encoder per quality, sharing one
  output, and streams to `rtmp://ingest.global-contribute.live-video.net/app`.
- **What LOBS needs:** libobs already has the shared output and the multi-track protocol. The
  request to Twitch and the setup of the encoders are in OBS's interface code
  (`multitrack-video-output`, `goliveapi-*`), which LOBS doesn't include, so that part is ported.
- An Enhanced Broadcasting switch in the stream settings (on where Twitch allows it), each
  quality's bitrate in the Stats panel, and a clear message plus a single-quality stream when Twitch
  says no.
- Builds on P1-1 and P1-2.

**Done when** a LOBS stream offers the same qualities as OBS's (see Reference) in the player and
the VOD.

### P1-5. Switching from OBS takes one step
*As an OBS user trying LOBS, first-time setup brings over my settings, scenes and plugins at once.*
- Setup starts with "Coming from OBS?". It finds `%APPDATA%\obs-studio`, defaults to the profile
  and scene collection used last, lists the others, and offers a folder picker if nothing is found.
- One import: the profile (canvas, encoder, bitrate, stream service and key, recording, Enhanced
  Broadcasting), the scene collection (through native/src/model/obs_import.cpp), and plugins
  (from OBS's plugin folders, through Settings › Plugins). Setup pairs a profile with a scene
  collection instead of asking twice.
- A summary afterwards of what came over and what didn't, with the reason.
- Plugin files are copied into LOBS's own plugin folder (decided 2026-10-30), so LOBS keeps
  working if OBS is updated or uninstalled. Only plugins built for LOBS's OBS version are copied;
  the summary lists the rest.

**Done when** someone with OBS set up streams from LOBS straight after setup, with no other
settings to change.

### P1-6. Nothing changes on air by accident
*As a streamer, while I'm live, nothing I didn't mean to change changes what my viewers see.*
- Close the remaining ways to change the on-air scene while live-locked (LOBS-006, reopened).

**Done when** RG-02 (qa/scenarios/90-regression.js) passes.

### P1-7. I sign in with Twitch instead of pasting a stream key
*As a Twitch streamer, I connect my account in LOBS the way I do in OBS, and LOBS fills in my
stream key and adds my Twitch panels.*

The same design as OBS's "Connect Account":
1. **LOBS's own Twitch app.** Register "LOBS" in the Twitch Developer Console (free; the account
   needs two-factor sign-in) as a Confidential client. **Registered 2026-10-30:** "LOBS",
   Broadcaster Suite, client ID `y0mck0jqerrkhzvqiy1pzhagusz1rn`, redirect
   `http://localhost:5340/twitch/callback` for local testing. That gives a client ID, which ships in LOBS,
   and a secret, which never does. OBS's client ID can't be reused (Twitch's developer terms, and
   it only works with OBS's server).
2. **A small auth server** holding the secret, like OBS's `auth.obsproject.com`. It takes Twitch's
   redirect, swaps the one-time code for tokens, hands them to LOBS, and refreshes them later.
   A few endpoints: a serverless function on the website's host would do. **Open:** where the site
   is hosted, and the server's address (it becomes the app's redirect URL).
3. **Sign-in inside LOBS.** "Connect account" in the stream settings and in first-time setup opens
   Twitch's sign-in page in a LOBS window (the browser engine LOBS already bundles), with the
   permissions OBS asks for: reading the stream key, chat, and the channel's details.
4. **After sign-in:** LOBS fetches the stream key and channel name through Twitch's API (Helix) and
   adds OBS's Twitch panels: chat, activity feed, and stream info (title and category). Tokens are
   kept in Windows Credential Manager. "Disconnect" removes them, and the panels go with them.
- Pairs with P1-5: setup offers "Connect with Twitch" instead of a stream key box. P1-4 (Enhanced
  Broadcasting) still authenticates with the stream key, as OBS does.

**Done when** someone streams to Twitch after only signing in, never seeing the stream key, and the
token still works a month later.

## P2

### P2-1. I rename things where they are
*As a user, I rename assets, sources and scenes in place, as I do layers, without a popup.*
- F2, a slow second click, or Rename in the menu edits the name where it is. Enter keeps it and Esc
  cancels. The rename popup goes.

**Done when** nothing in LOBS opens the rename popup.

### P2-2. My Stream Deck and VTuber tools can control LOBS (obs-websocket)
*As a streamer, Stream Deck, Touch Portal and VTuber apps such as VNyan control LOBS the way they
control OBS.*
- The obs-websocket protocol, covering scenes, sources, streaming and recording, and filters
  first. It's the biggest gap for VTubers (see the VTuber support notes).

**Done when** Stream Deck's OBS plugin and VNyan's OBS connection work with LOBS.

### P2-3. My stream comes back on its own
*As a streamer, if my connection drops, LOBS reconnects without me, and tells me it did.*
- Automatic reconnect with a countdown, and a health warning built on P1-2's stats.

**Done when** pulling the network cable and plugging it back in brings the stream back unattended.

### P2-4. Separate settings for each place I stream
*As a streamer, I keep separate settings sets (profiles) for Twitch, YouTube and local recording,
and switch between them.*

**Done when** switching profiles changes the stream service, encoder and recording settings at once.

### P2-5. Recording does what OBS's does
*As a creator, I can pause a recording, and my MKV recordings come out as MP4 to edit.*
- Pause and resume, and convert MKV to MP4 automatically or on request.

**Done when** a paused recording plays back without the gap, and the converted file opens in an
editor.

### P2-6. My plugins' panels show up
*As a plugin user, the panels (docks) my OBS plugins add appear in LOBS.*
- Next on the list in qa/PLUGINS_PLAN.md.

**Done when** a plugin that adds a dock in OBS adds the same panel in LOBS.

## P3

### P3-1. I stream vertical at the same time
*As a streamer, I send Twitch's vertical stream next to my horizontal one.*
- Twitch's dual format, through the same system as Enhanced Broadcasting (builds on P1-4). LOBS's
  scene editor suits a second, vertical canvas.

### P3-2. I watch all my scenes at once (Multiview)

### P3-3. Very large shows stay fast
- Stress targets PF-01 to PF-06, and memory that grows with each scene visited (LOBS-022,
  LOBS-034, LOBS-036).

### P3-4. The OBS-parity list is current
- Update PLAN.md's audit: hotkeys, the status bar, the virtual camera, projectors, transitions and
  Interact have shipped since it was written. Add what's still missing here.

## Reference: OBS with Enhanced Broadcasting, 2026-10-30

From `%APPDATA%\obs-studio\logs\2026-10-30 01-36-07.txt`. A 1920×1080, 60 fps canvas on an
RTX 3080 Ti; Twitch's plan:

| Quality | Bitrate | NVENC preset | fps |
|---|---|---|---|
| 1920×1080 | 7,500 kbps | p6 | 60 |
| 1280×720 | 3,500 kbps | p5 | 60 |
| 852×480 | 1,000 kbps | p5 | 30 |
| 640×360 | 500 kbps | p5 | 30 |
| 284×160 | 200 kbps | p5 | 30 |

All are constant bitrate, with keyframes every 2 s (keyint 120), tune hq, multipass qres, profile
high, and 2 B-frames. Lookahead (4 frames) is on only for the top quality. Audio is 160 kbps. In
total, 12,700 kbps of video over a wired 1 Gbit connection.
