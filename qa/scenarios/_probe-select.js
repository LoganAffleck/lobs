// Bounded diagnostic: layer selection and plain edits in a fresh process (heavy project).
t.include("_lib.js");
skipSetup();
t.setTimeout(140);
story("probe-select", "selection and edit cost, fresh", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1000);
	const d = t.doc();
	const sid = d.sceneOrder[1];
	app.editScene(sid); t.wait(300);
	const ids = app.layers.map(l => l.id);
	const m0 = t.proc().privateMB, sel = [];
	for (let k = 0; k < 20; ++k) { const a = t.now(); app.selectLayer(ids[k % ids.length], false); sel.push(t.now() - a); }
	const m1 = t.proc().privateMB, ed = [];
	app.selectLayer(ids[0], false);
	for (let k = 0; k < 40; ++k) { const a = t.now(); app.setTransform("x", 100 + k); ed.push(t.now() - a); }
	const m2 = t.proc().privateMB;
	const nop = [];
	for (let k = 0; k < 20; ++k) { const a = t.now(); app.clearSelection(); nop.push(t.now() - a); app.selectLayer(ids[0], false); }
	const m3 = t.proc().privateMB;
	t.metric("selectLayer", Object.assign(stats(sel), { mbPerCall: Math.round((m1 - m0) / 20 * 10) / 10 }));
	t.metric("setTransform", Object.assign(stats(ed), { mbPerCall: Math.round((m2 - m1) / 40 * 10) / 10 }));
	t.metric("clearSelection+select", Object.assign(stats(nop), { mbPerPair: Math.round((m3 - m2) / 20 * 10) / 10 }));
	t.metric("layersInScene", ids.length);
	check("probe ran", true);
});
