// Diagnostic (LOBS-022): is per-switch growth a leak, or garbage/deferred deletes that come back?
t.include("_lib.js");
skipSetup();
t.setTimeout(300);
story("probe-leak", "switch back and forth, then collect", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1500);
	const d = t.doc();
	const a = d.sceneOrder[1], b = d.sceneOrder[2];
	app.editScene(a); app.editScene(b); t.wait(1000); t.gc(); t.wait(1000);
	const m0 = t.proc().privateMB;
	for (let k = 0; k < 100; ++k) { app.editScene(k % 2 ? a : b); t.waitFrames(1, 500); }
	const m1 = t.proc().privateMB;
	t.gc(); t.wait(3000); t.gc(); t.wait(1000);
	const m2 = t.proc().privateMB;
	const ids = app.layers.map(l => l.id);
	for (let k = 0; k < 100; ++k) { app.selectLayer(ids[k % ids.length], false); t.waitFrames(1, 500); }
	const m3 = t.proc().privateMB;
	t.gc(); t.wait(3000);
	const m4 = t.proc().privateMB;
	app.selectLayer(ids[0], false);
	for (let k = 0; k < 100; ++k) { app.setTransform("x", 100 + k); t.waitFrames(1, 500); }
	const m5 = t.proc().privateMB;
	t.gc(); t.wait(3000);
	const m6 = t.proc().privateMB;
	// Same switches with the UI not told (C++ and engine only).
	t.blockAppSignals(true);
	t.gc(); t.wait(1000);
	const n0 = t.proc().privateMB;
	for (let k = 0; k < 100; ++k) { app.editScene(k % 2 ? a : b); t.waitFrames(1, 500); }
	t.gc(); t.wait(3000);
	const n1 = t.proc().privateMB;
	t.blockAppSignals(false);
	t.metric("switchesNoUi", { retainedAfterGcMB: Math.round(n1 - n0) });
	t.metric("switches", { mbPer: Math.round((m1 - m0) / 100 * 100) / 100, retainedAfterGcMB: Math.round(m2 - m0) });
	t.metric("clicks", { mbPer: Math.round((m3 - m2) / 100 * 100) / 100, retainedAfterGcMB: Math.round(m4 - m2) });
	t.metric("edits", { mbPer: Math.round((m5 - m4) / 100 * 100) / 100, retainedAfterGcMB: Math.round(m6 - m4) });
	check("probe ran", true);
});
