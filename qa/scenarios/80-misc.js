// Remaining automatable stories: TR-01/03, CV-02/03, CV-06, SA-02, AU-02, PJ-03, OUT-05.
t.include("_lib.js");
skipSetup();
t.setTimeout(900);
const img = t.scriptDir() + "/../runs/fixtures/heavy/assets/img3.png";

function isColor(px, r, g, b) { return px.ok && Math.abs(px.r - r) < 40 && Math.abs(px.g - g) < 40 && Math.abs(px.b - b) < 40; }

story("TR-01", "Every transition type lands on the target scene", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); placeColor("Red", "#ff0000"); app.fit("fill");
	const B = mkScene("B"); placeColor("Green", "#00ff00"); app.fit("fill");
	const types = ["cut", "fade", "swipe", "slide", "fade_to_color", "luma_wipe", "stinger"];
	const res = [];
	types.forEach(ty => {
		const id = app.addTransition(ty);
		check(ty + ": added", !!id, id);
		if (!id) return;
		app.setCurrentTransition(id);
		app.updateTransition(id, "durationMs", ty === "cut" ? 0 : 400);
		[B, A].forEach(target => {
			app.editScene(target);
			const s = t.now();
			app.take();
			t.wait(ty === "cut" ? 300 : 1300);
			const px = t.pixel(t.snapshot("", "tr01-" + ty + "-" + (target === B ? "B" : "A") + ".png", 480), 240, 135);
			const want = target === B ? [0, 255, 0] : [255, 0, 0];
			res.push({ type: ty, to: target === B ? "B" : "A", ok: isColor(px, want[0], want[1], want[2]), px: px.hex });
		});
	});
	t.metric("results", res);
	const bad = res.filter(r => !r.ok);
	check("every transition ends on the right scene", bad.length === 0, bad);
	t.note("stinger with no video file: " + JSON.stringify(res.filter(r => r.type === "stinger")) + "; toast='" + app.toast + "'");
});

story("TR-03", "Rapid switching, including mid-transition", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); placeColor("Red", "#ff0000"); app.fit("fill");
	const B = mkScene("B"); placeColor("Green", "#00ff00"); app.fit("fill");
	const fade = t.doc().transitions.filter(x => x.type === "fade")[0];
	app.setCurrentTransition(fade.id);
	app.updateTransition(fade.id, "durationMs", 800);
	const inputs0 = t.obsInputs().length;
	for (let k = 0; k < 50; ++k) { app.editScene(k % 2 ? A : B); app.take(); t.wait(150); }
	app.editScene(B); app.take();
	t.wait(1500);
	eq("program ends on the last scene asked for", t.programScene(), B);
	check("pixel is the last scene", isColor(t.pixel(t.snapshot("", "tr03.png", 480), 240, 135), 0, 255, 0));
	check("no leaked transition sources", t.obsInputs().length <= inputs0 + 1, [inputs0, t.obsInputs().length]);
});

story("CV-02", "Crop presets and crop limits", () => {
	fresh();
	app.addSource("image", "Pic", img, true);
	const id = app.inspector.id;
	t.waitUntil(() => app.inspector.nativeSize, 5000);
	t.metric("nativeSize", app.inspector.nativeSize);
	const res = {};
	[[16 / 9, "16:9"], [4 / 3, "4:3"], [1, "1:1"], [9 / 16, "9:16"]].forEach(([r, n]) => {
		sel(id); app.cropToAspect(r);
		const c = eff(id, "crop");
		const vis = (320 - c.l - c.r) / (180 - c.t - c.b);
		res[n] = { crop: [c.l, c.t, c.r, c.b], visibleAspect: Math.round(vis * 1000) / 1000 };
		near(n + ": visible aspect", vis, r, 0.02);
		check(n + ": centered", Math.abs(c.l - c.r) <= 1 && Math.abs(c.t - c.b) <= 1, c);
	});
	t.metric("presets", res);
	sel(id); app.cropToAspect(0);
	eq("remove crop", [eff(id, "crop").l, eff(id, "crop").t, eff(id, "crop").r, eff(id, "crop").b], [0, 0, 0, 0]);
	sel(id); app.setCrop("l", 250); app.setCrop("r", 250);
	const c = eff(id, "crop");
	check("crop can't exceed the source (no negative visible width)", c.l + c.r < 320, c);
	sel(id); app.setCrop("t", -50);
	check("negative crop rejected", eff(id, "crop").t >= 0, eff(id, "crop"));
});

story("CV-03", "Rounded corners up to a circle", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const id = placeColor("Sq", "#ffffff");
	sel(id); app.setTransform("w", 400); app.setTransform("h", 400); app.setTransform("x", 760); app.setTransform("y", 340);
	app.setCornerRadius(50, true);
	app.take(); t.wait(500);
	const p = t.snapshot("", "cv03.png", 0);
	const corner = t.pixel(p, 770, 350), center = t.pixel(p, 960, 540), edgeMid = t.pixel(p, 762, 540);
	t.metric("pixels", { corner: corner.hex, center: center.hex, edgeMid: edgeMid.hex });
	check("corner is cut away", !isColor(corner, 255, 255, 255));
	check("center and edge midpoint are filled", isColor(center, 255, 255, 255) && isColor(edgeMid, 255, 255, 255));
	sel(id); app.setCornerRadius(80, true);
	eq("radius clamps at 50%", eff(id, "crop").radius, 50);
});

story("CV-06", "Numeric fields: math and junk", () => {
	fresh();
	const id = placeColor("N", "#ffffff");
	t.note("math in fields is evaluated in QML (FNumField), not the controller; checking the controller rejects junk");
	sel(id); const before = eff(id, "transform").w;
	app.setTransform("w", NaN);
	check("NaN width rejected", isFinite(eff(id, "transform").w) && eff(id, "transform").w >= 1, eff(id, "transform"));
	sel(id); app.setTransform("w", -100);
	check("negative width clamps to ≥ 1", eff(id, "transform").w >= 1, eff(id, "transform").w);
	sel(id); app.setTransform("x", 1e12);
	t.note("x = 1e12 accepted as " + eff(id, "transform").x);
	app.setOpacity(1.7, true);
	check("opacity clamps to ≤ 1", eff(id, "visibility").opacity <= 1, eff(id, "visibility").opacity);
	app.setOpacity(-1, true);
	check("opacity clamps to ≥ 0", eff(id, "visibility").opacity >= 0, eff(id, "visibility").opacity);
	integrity();
	t.saveDocTo(t.out("cv06.lobs.json"));
	let parsed = null; try { parsed = JSON.parse(t.readFile(t.out("cv06.lobs.json"))); } catch (e) {}
	check("document with odd values still serializes to valid JSON", !!parsed);
});

story("SA-02", "Setup assistant: server pings and recommendations", () => {
	const got = [];
	app.serverPings.connect(list => got.push(list));
	app.pingServers("Twitch");
	t.waitUntil(() => got.length > 0, 20000);
	const list = got[0] || [];
	t.metric("pings", list.slice(0, 5));
	check("servers measured", list.length > 0, list.length);
	const ms = list.map(s => s.ms !== undefined ? s.ms : s.rtt).filter(v => typeof v === "number" && v >= 0);
	check("sorted fastest first", ms.every((v, k) => k === 0 || v >= ms[k - 1]), ms.slice(0, 10));
	const rec = [1000, 3000, 6000, 10000, 50000].map(k => Object.assign({ upload: k }, app.recommendSettings(k, "Twitch")));
	t.metric("recommendations", rec);
	check("bitrate ≤ 75% of upload (except the 1.5 Mbps floor)", rec.every(r => r.bitrate <= Math.max(1500, r.upload * 0.75)), rec.map(r => [r.upload, r.bitrate]));
	check("never above Twitch's 6000 kbps cap", rec.every(r => r.bitrate <= 6000));
	const lowUp = rec[0];
	t.note("1 Mbps upload → " + lowUp.bitrate + " kbps at " + lowUp.outWidth + "x" + lowUp.outHeight + "@" + lowUp.fps);
	check("a 1 Mbps upload isn't told to stream above its upload", lowUp.bitrate <= 1000 * 0.9, lowUp);
	check("a slow upload is flagged so the setup assistant warns (LOBS-028)", lowUp.slow === true && rec.filter(r => r.upload >= 2000).every(r => r.slow === false), rec.map(r => [r.upload, r.slow]));
	t.note("bandwidth test to Twitch needs a stream key; the QA profile has none (run it from your installed LOBS: MANUAL M-02)");
});

story("AU-02", "Audio effects chain on a source", () => {
	fresh();
	const mic = addKind("mic", "Mic", "", false).source;
	const types = app.audioFxTypes().map(x => x.type);
	t.metric("types", types);
	types.forEach(ty => app.addAudioFilter(mic, ty));
	eq("all effects added", t.doc().sources[mic].audio.filters.length, types.length);
	app.moveAudioFilter(mic, 0, 1);
	eq("move reorders", t.doc().sources[mic].audio.filters[1].type, types[0]);
	app.setAudioFilterEnabled(mic, 0, false);
	eq("disable", t.doc().sources[mic].audio.filters[0].enabled, false);
	const h0 = app.history.length;
	const key = Object.keys(t.doc().sources[mic].audio.filters[0].settings)[0];
	for (let k = 0; k <= 10; ++k) app.setAudioFilterValue(mic, 0, key, k / 10, k === 10);
	eq("value scrub = one undo step", app.history.length - h0, 1);
	app.removeAudioFilter(mic, 0);
	eq("remove", t.doc().sources[mic].audio.filters.length, types.length - 1);
	app.setAudio(mic, "volume", 0.5, true);
	app.setAudio(mic, "muted", true, true);
	app.setAudio(mic, "monitor", "monitor_and_output", true);
	const a = t.doc().sources[mic].audio;
	check("volume/mute/monitor stored", a.volume === 0.5 && a.muted === true && a.monitor === "monitor_and_output", a);
	t.note("toast after monitor_and_output on a mic: '" + app.toast + "'");
	const row = app.mixer.filter(m => m.id === mic)[0];
	t.metric("mixerRow", row || null);
	check("mixer warns when a mic is monitored to output (echo risk, PLAN §3.12)", row && row.warnings && row.warnings.length > 0, row && row.warnings);
});

story("PJ-03", "Assets: import, folder, rename, in-use protection", () => {
	fresh();
	const f = t.out("pj03-project.lobs.json");
	app.saveAs(t.url(f));
	t.wait(300);
	app.importAssets([t.url(img)]);
	t.wait(800);
	const names = app.assets.map(a => a.name);
	t.metric("assets", names);
	check("imported into the project's assets", names.some(n => /img3/.test(n)), names);
	const folder = app.createAssetFolder();
	check("new folder", !!folder && t.exists(folder), folder);
	check("rename folder", app.renameAsset(folder, "Overlays"));
	const a = app.assets.filter(x => /img3/.test(x.name))[0];
	if (a) {
		app.placeAsset(a.path);
		t.wait(300);
		const layers = app.layers.length;
		app.trashAssets([a.path]);
		t.wait(800);
		t.note("trashing an asset in use: file exists after=" + t.exists(a.path) + ", layers " + layers + " -> " + app.layers.length + ", toast='" + app.toast + "', missing=" + JSON.stringify(app.missingAssets));
		check("an asset used by a layer isn't silently trashed", t.exists(a.path) || /use|layer/i.test(app.toast), app.toast);
	}
});

story("MD-01", "Media controls: play, pause, restart, stop, seek, and allowed on air", () => {
	fresh();
	app.addSource("color", "Bg", "#203040", true);
	const n = t.recordingsFinished();
	t.recordClip(4);
	check("a clip to play", t.waitUntil(() => t.recordingsFinished() > n, 20000));
	const clip = String(t.lastRecording().path);
	const m = addKind("media", "Clip", clip);
	sel(m.inst);
	check("the Inspector knows it's a media layer", app.inspector.sourceKind === "media", app.inspector.sourceKind);
	t.wait(400); t.grab("md01-inspector.png");
	t.waitUntil(() => app.mediaInfo(m.source).durationMs > 0, 5000);
	const i0 = app.mediaInfo(m.source);
	t.metric("start", i0);
	check("duration is known", i0.durationMs > 2000, i0);
	t.waitUntil(() => app.mediaInfo(m.source).state === "playing", 3000); // OBS ignores Stop while a file is still opening
	app.mediaCommand(m.source, "stop"); t.wait(300);
	eq("stop", app.mediaInfo(m.source).state, "stopped");
	app.mediaCommand(m.source, "playPause"); t.waitUntil(() => app.mediaInfo(m.source).state === "playing", 3000);
	eq("play after stop starts it", app.mediaInfo(m.source).state, "playing");
	app.mediaCommand(m.source, "playPause"); t.wait(300);
	eq("pause", app.mediaInfo(m.source).state, "paused");
	app.mediaCommand(m.source, "seek", 2000); t.wait(400);
	const s = app.mediaInfo(m.source).timeMs;
	check("seek lands near 2 s", s >= 1500 && s <= 2600, s);
	// Live control: works on the on-air scene with live lock on.
	app.take();
	t.startRecording(); t.waitUntil(() => app.live, 10000);
	check("live and locked", app.liveLocked);
	app.mediaCommand(m.source, "restart"); t.waitUntil(() => app.mediaInfo(m.source).state === "playing", 3000);
	const r = app.mediaInfo(m.source);
	check("restart works on air while locked", r.state === "playing" && r.timeMs < 2000, r);
	t.stopRecording(); t.waitUntil(() => !app.live, 10000);
});

story("AU-04", "Mixer: its own order, drag to reorder, pin to the front, saved and undoable", () => {
	fresh();
	const ids = ["Mic A", "Mic B", "Mic C"].map(n => addKind("mic", n, "", false).source);
	const order = () => app.mixer.map(m => m.name).filter(n => /^Mic /.test(n));
	eq("starts in Sources order", order(), ["Mic A", "Mic B", "Mic C"]);
	const srcBefore = t.doc().sourceOrder.slice();
	app.moveMixer(ids[2], 0);
	eq("move C to the front", order(), ["Mic C", "Mic A", "Mic B"]);
	check("Sources order is untouched", JSON.stringify(t.doc().sourceOrder) === JSON.stringify(srcBefore));
	app.undo();
	eq("one undo step", order(), ["Mic A", "Mic B", "Mic C"]);
	app.setMixerPinned(ids[1], true);
	eq("pinned goes first", order(), ["Mic B", "Mic A", "Mic C"]);
	check("row says pinned", app.mixer.filter(m => m.id === ids[1])[0].pinned === true);
	app.moveMixer(ids[2], 0);
	eq("an unpinned strip can't jump ahead of a pinned one", order(), ["Mic B", "Mic C", "Mic A"]);
	const d = addKind("mic", "Mic D", "", false).source;
	eq("a new source joins at the end", order(), ["Mic B", "Mic C", "Mic A", "Mic D"]);
	roundTrip("au04");
	eq("order and pins survive save and reopen", order(), ["Mic B", "Mic C", "Mic A", "Mic D"]);
	app.setMixerPinned(ids[1], false);
	eq("unpin keeps its place among the rest", order()[0], "Mic B");
});

story("CC-01", "Live captions: speech in an audio source becomes caption text", () => {
	// Needs the speech model: LOBS_WHISPER_MODEL points at a local copy (see qa/README) and
	// qa/runs/fixtures/speech.wav (Windows text-to-speech, made by qa/tools/make_speech.ps1).
	const wav = t.scriptDir() + "/../runs/fixtures/speech.wav";
	if (captions.modelState !== "ready" || !t.exists(wav)) {
		t.note("skipped: model " + captions.modelState + ", speech clip " + t.exists(wav));
		check("speech model and clip available for this check", false, captions.modelPath());
		return;
	}
	fresh();
	const m = addKind("media", "Speech", wav);
	sel(m.inst);
	app.mediaCommand(m.source, "restart");
	check("captions start on the clip's audio", captions.start(m.source), captions.status);
	t.waitUntil(() => /water/i.test(captions.lines.map(l => l.text).join(" ")), 45000); // the last phrase
	const text = captions.lines.map(l => l.text).join(" ");
	t.note("captions: " + text);
	captions.stop();
	check("speech turned into text, nothing lost at the seams", /stream/i.test(text) && /today/i.test(text) && /haunted/i.test(text) && /follow/i.test(text) && /water/i.test(text), text);
	check("stopped", !captions.running);
});

// LOBS-040: libobs logs from its own threads (audio, video, task queues) at the same time.
story("LG-01", "Logging from many threads at once doesn't crash LOBS", () => {
	const n = t.logStorm(32, 20000);
	eq("every line was logged", n, 640000);
	check("LOBS is still running and responsive", t.waitFrames(2, 3000) >= 0);
});
