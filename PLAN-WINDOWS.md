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

**Done 2026-10-30** (0e8f78a, 54ce94c): Inspector showed "Stable @ 5,803 Kbps" and Configuration
Check "Excellent". Also found on the way: Twitch flags 6,000 of video plus audio as too high, so
Twitch video stops at 5,800, and a bitrate change in Settings now applies while live.

### P1-2. I can see how my stream is doing, and look back at it
*As a streamer, I can tell while live whether my stream is healthy, and find out afterwards what
went wrong.*
- Bitrate averaged over a few seconds, dropped frames (network), congestion and encoder lag, in the
  status bar and the Stats panel.
- Keep the last few session logs instead of overwriting `lobs.log` at each launch.

**Done when** a dropped connection or an overloaded encoder shows in the stats while live, and the
log from that stream is still there after LOBS restarts.

**Built 2026-10-30** (e881fec): logs carry over (checked). Health and congestion in Stats haven't
yet been seen with a bad connection.

### P1-3. I can test my stream without going live
*As a streamer, I can check my stream on Twitch before my viewers see it.*
- A Twitch test stream viewers can't see (a bandwidth test), started from Settings, with the P1-2
  stats showing.

**Done when** a test stream runs to Twitch and doesn't show on the channel.

**Done 2026-10-30** (2420f9d): Inspector showed "Stable @ 5,803 Kbps" while Stream Manager stayed
Offline.

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

**Built 2026-10-30:** a LOBS test stream got Twitch's five qualities (1080p60 to 160p30) and
Inspector showed "Stable @ 12,716 Kbps", "Excellent" (OBS's plan totals 12,700). Still to check:
the qualities in the player and the VOD on a real stream. Not done yet: Twitch's dynamic bitrate
(the "bitrate_interpolation_points" in its plan), which lowers bitrates when the connection
struggles.

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

**Built 2026-10-30:** _probe-obs-switch passes against this PC's OBS (5 profiles, 2 collections,
last used found; Twitch, key, 1920x1080@60, 6000, NVENC, Enhanced Broadcasting, recording folder
and format, and the scenes came over). Still to try: the setup step itself, and a third-party
plugin (none on this PC). Not done: checking a plugin's OBS version before copying it; Settings >
Plugins shows the ones that don't load.

### P1-6. Nothing changes on air by accident
*As a streamer, while I'm live, nothing I didn't mean to change changes what my viewers see.*
- Close the remaining ways to change the on-air scene while live-locked (LOBS-006, reopened).

**Done when** RG-02 (qa/scenarios/90-regression.js) passes.

**Done 2026-10-30:** RG-01..04 and LY-06 pass, and a scan of every edit without a live-lock check
found none that changes what's on air (see LOBS-006 in qa/BUGS.md). LY-06's wording check was
updated for the copy pass's toast.

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
   **Netlify Functions** on the website's site (decided 2026-10-30), in `web/netlify/functions`,
   with the secret in Netlify's environment variables (`TWITCH_CLIENT_SECRET`, never in the repo).
   Redirect URL: `https://streamlobs.com/twitch/callback` (add it in the Twitch console next to the
   localhost one). Endpoints: the callback (code → tokens, handed to LOBS) and a refresh.
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

**Built 2026-10-30** (8652480): signing in worked and a test stream ran with the fetched key. Still
to confirm: the token a month later. Setup's account step has the button but hasn't been tried.

## P2

### P2-1. I rename things where they are
*As a user, I rename assets, sources and scenes in place, as I do layers, without a popup.*
- F2, a slow second click, or Rename in the menu edits the name where it is. Enter keeps it and Esc
  cancels. The rename popup goes.

**Done when** nothing in LOBS opens the rename popup.

**Built 2026-10-30:** the popup is gone; sources and assets (list rows, folder and file tiles,
new folders) rename in place like layers and scenes. _probe-scenes-assets passes.

### P2-2. My Stream Deck and VTuber tools can control LOBS (obs-websocket)
*As a streamer, Stream Deck, Touch Portal and VTuber apps such as VNyan control LOBS the way they
control OBS.*
- The obs-websocket protocol, covering scenes, sources, streaming and recording, and filters
  first. It's the biggest gap for VTubers (see the VTuber support notes).

**Done when** Stream Deck's OBS plugin and VNyan's OBS connection work with LOBS.

**Built 2026-10-30:** LOBS speaks obs-websocket v5 itself (native/src/websocket_server.cpp; OBS's
plugin needs OBS's interface). Settings > Remote control: on/off, port 4455, password. Covers
scenes, inputs (mute, volume), scene items (visibility), stream, record, replay buffer, virtual
camera, and their events. _probe-websocket with qa/tools/ws_client.py passes (password, scenes,
layers, events, batches). Still to check: mute and volume (no audio source in the probe), and
real apps (Stream Deck, VNyan). Not yet: filters, transitions, source settings, studio mode.
OBS Web (obs-web.niek.tv) works against it in Chrome (2026-10-30): scenes, program picture
(GetSourceScreenshot), audio mixer with meters (InputVolumeMeters, GetSpecialInputs, monitor
type). It also asks for GetProfileList, GetSceneCollectionList and GetSceneTransitionList, which
LOBS doesn't answer yet; unsupported requests are logged as "[lobs] obs-websocket: X isn't
supported yet".

### P2-3. My stream comes back on its own
*As a streamer, if my connection drops, LOBS reconnects without me, and tells me it did.*
- Automatic reconnect with a countdown, and a health warning built on P1-2's stats.

**Done when** pulling the network cable and plugging it back in brings the stream back unattended.

**Done 2026-10-30:** a test stream came back by itself after the cable was pulled and replugged.
25 tries from 2 s apart (OBS's defaults); the status bar counts down each try, a toast says when
it's back, and a clear message says when it gave up. Before OBS's engine notices (its send buffer
has to fill), the status bar says "Connection lost? Nothing sent for N s" (not yet seen live).

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

### P2-7. A saved stream key looks saved
*As a streamer, once my key is saved, Settings shows that it's set instead of an empty box.*
- Saved: a checkmark and an Edit button, with no input. Edit opens the input (with Save and
  Cancel), and the remove button sits with it.

**Done when** Settings › Stream shows no key input until you click Edit.

**Built 2026-10-30.**

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
