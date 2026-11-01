// Probe: keeping a source's sound out of recordings.
t.include("_lib.js");
skipSetup();
story("RA-01", "In recordings: off keeps a source on the stream only, saved with the show", () => {
	fresh();
	addKind("mic", "Alerts", "", false); t.wait(300);
	const k = app.mixer.find(m => m.name === "Alerts").id;
	const row = () => app.mixer.find(m => m.id === k);
	check("in recordings by default", row() && row().inRecording === true);
	app.setAudio(k, "inRecording", false, true);
	eq("off", row().inRecording, false);
	eq("tracks 1-5 shown", row().tracks.length, 5);
	// The Inspector: Goes to (Stream, Recording), Monitor in OBS's words, Advanced closed.
	app.selectSource(k); t.waitFrames(3, 2000);
	check("Active in shows", t.items("Active in").length === 1 && t.items("Recording").length >= 1);
	check("Monitor shows", t.items("Monitor").length >= 1);
	check("Advanced is closed", t.items("Advanced").length >= 1 && t.items("Sync offset").length === 0);
	t.clickItem("Advanced", 0); t.waitFrames(3, 2000);
	check("Advanced opens", t.waitUntil(() => t.items("Sync offset").length >= 1 && t.items("Pinned to the front").length >= 1, 3000));
	t.grab("advanced.png");
	const path = t.out("rec-audio.lobs.json"); t.saveDocTo(path);
	app.open(t.url(path)); t.waitFrames(2, 3000);
	eq("saved", app.mixer.find(m => m.id === k).inRecording, false);
});

story("RA-02", "A global source can stay muted in one scene, and only while that scene is on air", () => {
	fresh();
	addKind("mic", "Mic", "", false); t.wait(300);
	const k = app.mixer.find(m => m.name === "Mic").id;
	const A = t.doc().sceneOrder[0];
	app.addScene();
	const B = t.doc().sceneOrder.find(id => id !== A);
	app.editScene(B);
	app.setMutedInScene(k, true);
	check("muted in B", t.doc().scenes[B].mutedAudio.indexOf(k) >= 0);
	check("not in A", (t.doc().scenes[A].mutedAudio || []).indexOf(k) < 0);
	check("the mixer row says so while editing B", app.mixer.find(m => m.id === k).mutedHere === true);
	app.editScene(A); app.take(); t.wait(500);
	check("not while editing A", app.mixer.find(m => m.id === k).mutedHere === false);
	eq("heard while A is on air", t.inputMuted(k), false);
	app.editScene(B); app.take(); t.wait(800);
	eq("muted while B is on air", t.inputMuted(k), true);
	app.editScene(A); app.take(); t.wait(800);
	eq("heard again on A", t.inputMuted(k), false);
	app.undo();
	check("undoable", (t.doc().scenes[B].mutedAudio || []).indexOf(k) < 0);
	app.redo();
	const path = t.out("scene-mute.lobs.json"); t.saveDocTo(path);
	app.open(t.url(path)); t.waitFrames(2, 3000);
	check("saved", t.doc().scenes[B].mutedAudio.indexOf(k) >= 0);
});

story("RA-03", "Effects are flat rows: one-setting effects inline, others open with a click", () => {
	fresh();
	addKind("mic", "Mic", "", false); t.wait(300);
	const k = app.mixer.find(m => m.name === "Mic").id;
	t.resizeWindow(1600, 1300);
	app.addAudioFilter(k, "noise_suppress"); app.addAudioFilter(k, "compressor"); app.addAudioFilter(k, "gain");
	app.selectSource(k); t.waitFrames(5, 2000);
	if (t.items("Sync offset").length) { t.clickItem("Advanced", 0); t.waitFrames(5, 2000); } // room for the effects
	check("says the order matters", t.waitUntil(() => t.items("Runs top to bottom").length === 1, 3000));
	check("sentence case", t.items("Noise suppression").length >= 1);
	check("compressor closed: no Threshold slider", t.items("Threshold").length === 0);
	t.grab("fx-closed.png");
	t.clickItem("Compressor", t.items("Compressor").length - 1); t.waitFrames(5, 2000);
	check("opens with a click", t.waitUntil(() => t.items("Threshold").length >= 1, 3000));
	check("one-setting effects have no second label", t.items("Suppression").filter(i => i.text === "Suppression").length === 0);
	t.grab("fx-open.png");
	t.clickItem("Compressor", t.items("Compressor").length - 1); t.waitFrames(5, 2000);
	check("and closes with another", t.waitUntil(() => t.items("Threshold").length === 0, 3000));
});

story("RA-04", "Each setting gets its control: gate bar, EQ faders, ratio chips, time fields", () => {
	fresh();
	t.resizeWindow(1600, 1300);
	addKind("mic", "Mic", "", false); t.wait(300);
	const k = app.mixer.find(m => m.name === "Mic").id;
	app.addAudioFilter(k, "noise_gate"); app.addAudioFilter(k, "eq");
	app.selectSource(k); t.waitFrames(5, 2000);
	if (t.items("Sync offset").length) { t.clickItem("Advanced", 0); t.waitFrames(5, 2000); }
	t.clickItem("Noise gate", t.items("Noise gate").length - 1); t.waitFrames(5, 2000);
	t.clickItem("3-band EQ", t.items("3-band EQ").length - 1); t.waitFrames(5, 2000);
	check("gate: both thresholds on one bar", t.waitUntil(() => t.items("Close").length >= 1 && t.items("Open").length >= 1, 3000));
	check("EQ: three faders", t.items("Low").length >= 1 && t.items("High").length >= 1);
	const fx = () => app.mixer.find(m => m.id === k).fx;
	eq("kinds", fx()[0].params.map(p => p.kind).join(","), "level,level,time,time,time");
	t.grab("fx-kinds.png");
	app.removeAudioFilter(k, 1); app.addAudioFilter(k, "compressor"); t.waitFrames(3, 2000);
	eq("compressor kinds", fx()[1].params.map(p => p.kind).join(","), "level,ratio,time,time,gain");
	check("ratio presets", fx()[1].params[1].presets.length === 4);
	// A narrow window: nothing in the Inspector reaches past its edge.
	t.resizeWindow(1100, 1300); t.waitFrames(5, 2000);
	t.clickItem("Compressor", t.items("Compressor").length - 1); t.waitFrames(5, 2000);
	const insp = t.items("Inspector")[0], add = t.items("Effect").find(i => i.text === "Effect");
	t.metric("edges", { insp: insp, add: add });
	t.grab("fx-narrow.png");
});
