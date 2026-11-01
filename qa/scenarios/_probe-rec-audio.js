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
	addKind("desktop_audio", "Mic", "", false); t.wait(300); // no Tune button: the EQ stays in view
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

story("RA-05", "Effect chains copy, paste, save, load, and start from presets", () => {
	fresh();
	addKind("mic", "Mic", "", false); addKind("desktop_audio", "Desktop", "", false); t.wait(300);
	const mic = app.mixer.find(m => m.name === "Mic").id, desk = app.mixer.find(m => m.name === "Desktop").id;
	const fx = (id) => app.mixer.find(m => m.id === id).fx;
	eq("four presets", app.audioPresets().length, 4);
	app.applyAudioPreset(mic, "broadcast");
	eq("broadcast chain", fx(mic).map(f => f.type).join(","), "noise_suppress,noise_gate,eq,compressor,limiter");
	eq("its settings", fx(mic)[3].params.find(p => p.key === "ratio").value, 4);
	app.undo();
	eq("one undo step", fx(mic).length, 0);
	app.redo();
	app.copyAudioFilters(mic);
	check("clipboard has a chain", app.canPasteAudioFilters());
	app.pasteAudioFilters([desk], true);
	eq("pasted", fx(desk).map(f => f.type).join(","), fx(mic).map(f => f.type).join(","));
	app.pasteAudioFilters([desk], false);
	eq("added to the end", fx(desk).length, 10);
	const path = t.out("mic.lobsfx");
	check("saved", app.saveAudioFilters(mic, path, "My mic"));
	check("a lobsfx file", JSON.parse(t.readFile(path)).format === "lobsfx");
	check("loaded", app.loadAudioFilters([desk], path, true));
	eq("replaced", fx(desk).length, 5);
	t.writeFile(t.out("junk.lobsfx"), "{}");
	check("junk refused", !app.loadAudioFilters([desk], t.out("junk.lobsfx"), true));
	eq("and nothing changed", fx(desk).length, 5);
});

story("RA-06", "Tune my mic: what it heard becomes an ordinary chain, one undo step", () => {
	fresh();
	addKind("mic", "Mic", "", false); addKind("desktop_audio", "Desktop", "", false); t.wait(300);
	const mic = app.mixer.find(m => m.name === "Mic").id, desk = app.mixer.find(m => m.name === "Desktop").id;
	const fx = () => app.mixer.find(m => m.id === mic).fx;
	const val = (type, key) => { const f = fx().find(f => f.type === type); return f && f.params.find(p => p.key === key).value; };
	// A quiet room: no noise processing at all, just level and gentle compression.
	app.applyMicTuning(mic, { floor: -70, speech: -24, loud: -14 });
	eq("quiet room: no noise processing", fx().map(f => f.type).join(","), "gain,compressor,limiter");
	app.undo();
	// A noisy room, a soft voice, big yells, loud game audio.
	app.applyMicTuning(mic, { floor: -48, speech: -30, loud: -10, desk: -6, deskId: desk });
	eq("chain", fx().map(f => f.type).join(","), "noise_suppress,noise_gate,gain,compressor,limiter");
	check("gate opens well under the voice", val("noise_gate", "open_threshold") <= -48, val("noise_gate", "open_threshold"));
	check("soft voice raised", val("gain", "db") > 10, val("gain", "db"));
	check("gentle ratio, even with big yells", val("compressor", "ratio") <= 4, val("compressor", "ratio"));
	check("attack keeps consonants", val("compressor", "attack_time") >= 10);
	check("a little makeup", val("compressor", "output_gain") >= 0 && val("compressor", "output_gain") <= 3);
	eq("limiter at -1", val("limiter", "threshold"), -1);
	const dv = app.mixer.find(m => m.id === desk).volume;
	check("game audio turned down", dv < 0.5, dv);
	app.undo();
	eq("one undo step", fx().length, 0);
	eq("desktop volume back", app.mixer.find(m => m.id === desk).volume, 1);
	// The wizard opens from the mic's settings.
	app.selectSource(mic); t.waitFrames(5, 2000);
	if (t.items("Sync offset").length) { t.clickItem("Advanced", 0); t.waitFrames(5, 2000); }
	check("Tune my mic offered", t.items("Tune my mic").length >= 1);
	t.clickItem("Tune my mic", 0); t.waitFrames(10, 2000);
	check("wizard opens", t.waitUntil(() => t.items("Tune your mic").length >= 1, 3000));
	check("shows its steps", t.items("Quiet").length >= 1 && t.items("Compare").length >= 1 && t.items("Game").length >= 1);
	t.grab("wizard.png");
	t.clickItem("Start", t.items("Start").length - 1); t.waitFrames(5, 2000);
	if (t.waitUntil(() => t.items("Get ready").length >= 1, 2000)) t.grab("wizard-ready.png");
	check("quiet step counts itself in", t.waitUntil(() => t.items("Starting in").length >= 1 || t.items("Listening").length >= 1, 3000));
	t.grab("wizard-quiet.png");
	t.key("Escape"); t.wait(400);
	check("Esc doesn't close it mid-run", t.items("Stay quiet").length >= 1);
	// A clip records and saves.
	app.startClip(mic); t.wait(600);
	const clip = app.stopClip("probe");
	t.metric("clip", { path: clip, bytes: clip ? t.fileSize(clip) : 0 });
});

story("RA-07", "Tune my mic: the results screen, with the effects to adjust", () => {
	fresh();
	t.resizeWindow(1600, 1100);
	addKind("mic", "Mic", "", false); t.wait(300);
	const mic = app.mixer.find(m => m.name === "Mic").id;
	app.applyMicTuning(mic, { floor: -55, speech: -22, loud: -8 });
	t.clickItem("Cancel", t.items("Cancel").length - 1); t.wait(600); // the wizard from RA-06
	const ok = t.mount('Item { MicWizard { id: w; Component.onCompleted: { w.micId = "' + mic + '"; w.measured = ({ floor: -55, speech: -22, loud: -8 }); w.step = 6; w.open() } } }');
	check("mounted", ok);
	t.waitFrames(10, 2000);
	check("shows the effects", t.waitUntil(() => t.items("Edit filters").length >= 1, 3000));
	t.grab("wizard-results.png");
	t.clickItem("Edit filters", t.items("Edit filters").length - 1); t.waitFrames(5, 2000);
	check("Edit filters opens the effects", t.waitUntil(() => t.items("Noise gate").length >= 1 && t.items("Record After again").length >= 1, 3000));
});

story("RA-08", "Ducking: tuning can make game audio dip under the voice", () => {
	fresh();
	addKind("mic", "Mic", "", false); addKind("desktop_audio", "Desktop", "", false); t.wait(300);
	const mic = app.mixer.find(m => m.name === "Mic").id, desk = app.mixer.find(m => m.name === "Desktop").id;
	const dfx = () => app.mixer.find(m => m.id === desk).fx;
	app.applyMicTuning(mic, { floor: -60, speech: -22, loud: -10, desk: -8, deskId: desk, duck: true });
	const d = dfx().find(f => f.type === "duck");
	check("a ducker on desktop audio", !!d);
	eq("keyed to the mic", d && d.sidechain, mic);
	app.applyMicTuning(mic, { floor: -60, speech: -22, loud: -10, desk: -8, deskId: desk, duck: true });
	eq("tuning again replaces it", dfx().filter(f => f.type === "duck").length, 1);
	app.addAudioFilter(desk, "duck");
	eq("also addable by hand", dfx().filter(f => f.type === "duck").length, 2);
});
