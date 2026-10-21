// Fix-agent profiling (LOBS-025): the one long UI gap after a rename on the stress project.
t.include("_lib.js");
skipSetup();
t.setTimeout(600);
story("probe-autosave", "stress: UI gaps after renames", () => {
	const f = t.scriptDir() + "/../runs/fixtures/stress/stress.lobs.json";
	for (let k = 0; k < 3; ++k) { app.newDocument(); t.wait(300); app.open(t.url(f)); t.waitFrames(2, 10000); }
	const d = t.doc();
	for (let k = 0; k < 5; ++k) app.saveProject();
	const sid = d.sceneOrder[0];
	app.editScene(sid);
	t.metric("gcMs", t.gcTimed());
	t.wait(1000);
	for (let k = 0; k < 3; ++k) { app.editScene(d.sceneOrder[k * 10]); app.selectLayer(app.layers[0].id, false); t.waitFrames(1, 2000); }
	app.editScene(sid);
	t.metric("gcAfterUiWork", t.gcTimed());
	const rows = [];
	for (let k = 0; k < 6; ++k) {
		app.takePerfCounters();
		const a = t.now();
		app.renameScene(sid, "Scene 1 r" + k);
		const call = t.now() - a;
		let worst = 0, at = 0, last = t.now();
		const end = last + 900;
		while (t.now() < end) { t.wait(5); const n = t.now(); if (n - last - 5 > worst) { worst = n - last - 5; at = last - a; } last = n; }
		rows.push({ call: Math.round(call), worstGap: Math.round(worst), gapStartsAfterMs: Math.round(at), perf: app.takePerfCounters() });
	}
	t.metric("renames", rows);
	check("probe ran", true);
});
