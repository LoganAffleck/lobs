// Bounded diagnostic: how time and memory grow per scene switch and per linked edit (heavy project).
t.include("_lib.js");
skipSetup();
t.setTimeout(140);
const file = t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json";
story("probe-growth", "per-step time and memory", () => {
	app.open(t.url(file)); t.waitFrames(2, 5000);
	const ids = t.doc().sceneOrder, rows = [];
	const m0 = t.proc();
	for (let k = 0; k < 30; ++k) {
		const a = t.now(); app.editScene(ids[(k * 37) % ids.length]); const ms = t.now() - a;
		const p = t.proc();
		rows.push({ step: "switch " + k, ms: Math.round(ms), privateMB: Math.round(p.privateMB), handles: p.handles });
		t.metric("rows", rows);
		t.writeFile(t.out("progress.txt"), JSON.stringify(rows[rows.length - 1]));
	}
	const d = t.doc();
	const cam = d.sourceOrder.filter(s => d.sources[s].kind === "camera")[0];
	const cams = Object.keys(d.instances).filter(i => d.instances[i].sourceId === cam);
	for (let k = 0; k < 3; ++k) {
		sel(cams[k]);
		const a = t.now(); app.setTransform("x", 1300 + k); const ms = t.now() - a;
		const p = t.proc();
		rows.push({ step: "linked edit " + k, ms: Math.round(ms), privateMB: Math.round(p.privateMB), handles: p.handles });
		t.metric("rows", rows);
		t.writeFile(t.out("progress.txt"), JSON.stringify(rows[rows.length - 1]));
	}
	// Same edit on an unlinked layer, for comparison.
	const plain = Object.keys(d.instances).filter(i => d.instances[i].kind === "source" && !Object.keys(d.instances[i].links || {}).length && d.sources[d.instances[i].sourceId].kind === "color")[0];
	sel(plain);
	const a = t.now(); app.setTransform("x", 10); const ms = t.now() - a;
	rows.push({ step: "unlinked edit", ms: Math.round(ms), privateMB: Math.round(t.proc().privateMB) });
	t.metric("rows", rows);
	t.metric("start", m0);
	check("probe ran", true);
});
