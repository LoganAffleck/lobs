// Bounded diagnostic: does per-switch memory growth depend on scene content?
t.include("_lib.js");
skipSetup();
t.setTimeout(140);
function run(label, ids, n) {
	const rows = [], m0 = t.proc().privateMB;
	for (let k = 0; k < n; ++k) { const a = t.now(); app.editScene(ids[k % ids.length]); rows.push(t.now() - a); }
	t.wait(1000);
	const m1 = t.proc().privateMB;
	t.metric(label, { switches: n, firstMs: Math.round(rows[0]), lastMs: Math.round(rows[n - 1]), mbPerSwitch: Math.round((m1 - m0) / n * 10) / 10, totalMB: Math.round(m1 - m0) });
}
story("probe-growth2", "growth vs content", () => {
	fresh();
	placeColor("A", "#ff0000"); mkScene("B"); placeColor("B", "#00ff00");
	run("tiny project, 2 scenes", t.doc().sceneOrder, 30);
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000);
	const d = t.doc();
	const plain = d.sceneOrder.filter((s, k) => k % 4 === 1 && k % 10 !== 0 && k >= 30); // no group, no display, no camera
	const camScenes = d.sceneOrder.filter((s, k) => k < 30 && k % 10 !== 0 && k % 4 !== 0);
	const dispScenes = d.sceneOrder.filter((s, k) => k % 10 === 0);
	run("heavy, 2 plain graphics scenes", plain.slice(0, 2), 20);
	run("heavy, 2 camera scenes", camScenes.slice(0, 2), 20);
	run("heavy, 2 display-capture scenes", dispScenes.slice(0, 2), 20);
	check("probe ran", true);
});
