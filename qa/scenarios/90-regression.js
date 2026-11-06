// Fix-pass verification: probes around the new live lock and other fixes, written by QA after
// reading the fix commits (not by the fixing agent).
t.include("_lib.js");
skipSetup();

// Scene A on program with a layer, scene B off air with a layer, recording (live).
function liveSetup() {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.renameScene(A, "OnAir");
	app.editScene(A);
	const a = placeColor("AirLayer", "#ff0000"); setT(a, "x", 200);
	const B = mkScene("OffAir");
	const b = placeColor("OffLayer", "#00ff00"); setT(b, "x", 200);
	app.editScene(A); app.take(); t.wait(400);
	t.startRecording();
	t.waitUntil(() => app.live, 10000);
	return { A: A, B: B, a: a, b: b };
}
function liveTeardown() { t.stopRecording(); t.waitUntil(() => !app.live, 10000); }

story("RG-01", "Live lock: off-air scenes stay fully editable while live", () => {
	const s = liveSetup();
	check("live and locked", app.live && app.liveLocked);
	sel(s.b); app.setTransform("x", 400);
	eq("Inspector edit on the off-air scene works", eff(s.b, "transform").x, 400);
	sel(s.b); app.nudge(10, 0);
	eq("arrow-key nudge on the off-air scene works", eff(s.b, "transform").x, 410);
	t.note("toast after off-air nudge: '" + app.toast + "'");
	sel(s.b); app.addFilter("color");
	eq("filter on the off-air scene works", eff(s.b, "filters").length, 1);
	app.editScene(s.B); app.addSource("color", "New off-air", "#0000ff", true);
	check("adding a source to the off-air scene works", Object.keys(t.doc().instances).some(i => t.doc().instances[i].name === "New off-air"));
	liveTeardown();
});

story("RG-02", "Live lock: live controls allowed, no back doors for the rest", () => {
	const s = liveSetup();
	// Visibility is a live control (product decision 2026-10-20): the eye icon and the Inspector agree.
	app.editScene(s.A);
	app.toggleVisible(s.a);
	const eyeHid = eff(s.a, "visibility").visible === false;
	app.toggleVisible(s.a);
	sel(s.a); app.setVisibleFlag(false);
	const inspHid = eff(s.a, "visibility").visible === false;
	sel(s.a); app.setVisibleFlag(true);
	check("eye icon and Inspector both hide on air (visibility is a live control)", eyeHid && inspHid, { eye: eyeHid, inspector: inspHid });
	const before = t.docJson();
	// Delete the source that's on air.
	const src = t.doc().instances[s.a].sourceId;
	app.deleteSource(src);
	const gone = !t.doc().sources[src];
	t.note("deleteSource of an on-air source while locked: deleted=" + gone + ", toast='" + app.toast + "'");
	check("deleting a source that's on air is refused while locked", !gone, "on-air layer removed through Sources");
	if (gone) app.undo();
	// Delete the scene on program.
	app.deleteScene(s.A);
	const prog = t.programScene();
	t.note("deleteScene(program) while locked: scene exists=" + !!t.doc().scenes[s.A] + ", program now " + (t.doc().scenes[prog] ? t.doc().scenes[prog].name : prog));
	check("deleting the on-air scene is refused while locked", !!t.doc().scenes[s.A] && prog === s.A, "program switched to " + (t.doc().scenes[prog] ? t.doc().scenes[prog].name : prog));
	if (!t.doc().scenes[s.A]) app.undo();
	liveTeardown();
});

story("RG-03", "Live lock: copying out of the on-air scene isn't blocked (it changes nothing on air)", () => {
	const s = liveSetup();
	app.editScene(s.A); sel(s.a);
	const n = t.doc().scenes[s.B].layers.length;
	app.copySelectedTo(s.B, true);
	const copied = t.doc().scenes[s.B].layers.length === n + 1;
	t.note("Copy to OffAir from the on-air scene: copied=" + copied + ", toast='" + app.toast + "'");
	check("Copy to another scene works from the on-air scene", copied, app.toast);
	liveTeardown();
});

story("RG-04", "Live lock: Alt overrides, as the toast promises", () => {
	const s = liveSetup();
	t.note("Alt is read from the real keyboard (queryKeyboardModifiers), so the harness can't hold it for Inspector edits; canvas Alt-drag is covered in LY-06");
	sel(s.a); app.nudge(5, 0);
	t.note("nudge on air while locked: x=" + eff(s.a, "transform").x + ", toast='" + app.toast + "'");
	check("nudge refusal mentions how to get past it", /Alt|unlock/i.test(app.toast), app.toast);
	liveTeardown();
});

story("RG-05", "Leak: memory stays flat over 300 switches and 100 clicks (heavy)", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1000);
	const ids = t.doc().sceneOrder;
	// Memory swings by a couple of hundred MB as scenes come and go (2026-11-01: 2000–2250 MB over
	// 400 switches, no climb), so one reading before and after can't tell a leak. Warm up with a
	// pass over the scenes, then compare the lowest readings early and late: a leak lifts the floor.
	const sw = [], marks = [];
	for (let k = 0; k < 300; ++k) {
		const a = t.now(); app.editScene(ids[(k * 37) % ids.length]); t.waitFrames(1, 1000); sw.push(t.now() - a);
		if (k >= 99 && k % 50 === 49) marks.push(settledMB());
	}
	const m0 = Math.min(marks[0], marks[1]), m1 = Math.min(marks[3], marks[4]);
	app.editScene(ids[1]); t.wait(200);
	const L = app.layers.map(l => l.id), cl = [];
	for (let k = 0; k < 100; ++k) { const a = t.now(); app.selectLayer(L[k % L.length], false); t.waitFrames(1, 1000); cl.push(t.now() - a); }
	const m2 = settledMB();
	t.metric("switch", stats(sw)); t.metric("click", stats(cl));
	t.metric("memory", { readings: marks.map(Math.round), early: Math.round(m0), late: Math.round(m1), after100Clicks: Math.round(m2) });
	check("≤ 0.5 MB retained per switch", (m1 - m0) / 150 <= 0.5, ((m1 - m0) / 150).toFixed(2) + " MB/switch");
	check("≤ 0.5 MB retained per click", (m2 - m1) / 100 <= 0.5, ((m2 - m1) / 100).toFixed(2) + " MB/click");
	check("switch + frame p95 ≤ 100 ms", stats(sw).p95 <= 100, stats(sw));
	check("click + frame p95 ≤ 100 ms", stats(cl).p95 <= 100, stats(cl));
	check("no slowdown: last 20 switches no slower than first 20 (×1.5)", stats(sw.slice(-20)).p50 <= stats(sw.slice(0, 20)).p50 * 1.5 + 5, [stats(sw.slice(0, 20)).p50, stats(sw.slice(-20)).p50]);
});

story("RG-06", "UI still updates after the per-list signals change (no stale panels)", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const a = placeColor("Stale?", "#ff00ff");
	t.wait(200);
	check("new layer appears in Layers", t.items("Stale?").length > 0);
	app.renameLayer(a, "Renamed row");
	t.wait(200);
	check("rename shows in the Layers row", t.items("Renamed row").length > 0 && t.items("Stale?").length === 0, t.items("Renamed row").map(r => r.text));
	const B = mkScene("Fresh scene"); t.wait(200);
	check("new scene shows in Scenes", t.items("Fresh scene").length > 0);
	app.renameScene(B, "Renamed scene"); t.wait(200);
	check("scene rename shows", t.items("Renamed scene").length > 0);
	app.undo(); t.wait(200);
	check("undo of the rename shows", t.items("Fresh scene").length > 0 && t.items("Renamed scene").length === 0);
	app.editScene(A); sel(a); app.setTransform("x", 321); t.wait(200);
	check("Inspector X field shows the new value", t.items("321").length > 0, t.items("321").map(r => r.cls));
	const src = addKind("camera", "Cam row", "", false).source; t.wait(300);
	check("new source shows in Sources", t.items("Cam row").length > 0);
	app.deleteSource(src); t.wait(300);
	check("deleted source leaves Sources", t.items("Cam row").length === 0);
	check("History panel lists the latest edit", app.history.length > 0 && /Delete source|Remove|Delete/.test(app.history[app.history.length - 1].label), app.history.slice(-2));
	t.grab("rg06.png");
});

// The canvas stays opaque where the scene draws nothing (the window is translucent).
story("AL-01", "empty canvas areas aren't see-through", () => {
	fresh();
	app.layers.forEach(l => { if (l.visible !== false) app.toggleVisible(l.id) }); // an empty scene
	t.wait(800);
	const ws = t.windowSize();
	const g = t.grab("al01.png");
	const pts = [[0.23, 0.5], [0.77, 0.4], [0.77, 0.6]].map(p => t.pixel(g, Math.round(ws.w * ws.dpr * p[0]), Math.round(ws.h * ws.dpr * p[1])));
	check("canvas is opaque", pts.every(p => p.ok && p.a === 255), pts);
	t.note("layers: " + JSON.stringify(app.layers.map(l => l.visible)) + " edit=program " + (app.programSceneId === app.editSceneId));
});

// Transitions dialog: the stinger plays in the preview, and a missing video says so.
story("TU-01", "stinger dialog shows the video", () => {
	fresh();
	t.wait(1200);
	if (t.items("Skip setup").length) { t.click("Skip setup"); t.wait(400); }
	const vid = t.scriptDir() + "/../runs/fixtures/stinger-blue.webm";
	const id = app.addTransition("stinger");
	app.updateTransition(id, "path", vid);
	app.setCurrentTransition(id);
	check("mounted opener", t.mount("Item { Timer { interval: 100; running: true; onTriggered: parent.Window.window.openTransitions() } }"));
	t.wait(2500);
	// The preview plays the stinger's video (blue) between the sample scenes A and B.
	let sawVideo = false;
	for (let i = 0; i < 16 && !sawVideo; i++) {
		const g = t.grab("tu01-f" + i + ".png"), ws = t.windowSize();
		const p = t.pixel(g, Math.round(ws.w * ws.dpr * 0.5), Math.round(ws.h * ws.dpr * 0.32));
		sawVideo = p.ok && p.b > 230 && p.r < 90;
		t.wait(200);
	}
	check("preview shows the stinger video", sawVideo);
	check("shows the video's length", t.items(" s · ").length > 0, t.items("Reading").map(i => i.text));
	app.updateTransition(id, "path", "C:/nope/missing.webm");
	t.wait(800);
	t.grab("tu01-missing.png");
	check("says the video is missing", t.items("Video not found").length > 0);
	check("offers Find video", t.items("Find video…").length > 0);
	app.setCurrentTransition(app.transitions[0].id);
	t.wait(300);
	t.grab("tu01-fade.png");
});
