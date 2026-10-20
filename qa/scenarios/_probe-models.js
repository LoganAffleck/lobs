// Diagnostic (LOBS-022/024): cost and memory of each QML-facing model getter on the heavy project.
t.include("_lib.js");
skipSetup();
t.setTimeout(200);
story("probe-models", "per-model cost", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1000);
	app.editScene(t.doc().sceneOrder[1]); t.wait(300);
	const names = ["scenes", "pool", "layers", "inspector", "history", "mixer", "transitions", "assets", "sourceStatus", "settings", "levels", "hotkeyActions"];
	names.forEach(n => {
		let x; const m0 = t.proc().privateMB, a = t.now();
		for (let k = 0; k < 20; ++k) x = app[n];
		const ms = (t.now() - a) / 20, m1 = t.proc().privateMB;
		t.metric(n, { msPerRead: Math.round(ms * 100) / 100, mbPer20: Math.round((m1 - m0) * 10) / 10, size: x && x.length !== undefined ? x.length : (x ? Object.keys(x).length : 0) });
	});
	// docChanged with nothing else: how much is the QML reaction?
	const ids = app.layers.map(l => l.id);
	const m0 = t.proc().privateMB, a = t.now();
	for (let k = 0; k < 20; ++k) app.selectLayer(ids[k % ids.length], false);
	t.metric("selectLayer", { ms: Math.round((t.now() - a) / 20), mbPer20: Math.round(t.proc().privateMB - m0) });
	const m2 = t.proc().privateMB; t.gc ? t.gc() : 0; t.wait(500);
	t.metric("afterGc", { mb: Math.round(t.proc().privateMB - m2) });
	check("probe ran", true);
});
