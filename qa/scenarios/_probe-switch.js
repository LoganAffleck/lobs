// Debug probe: 10 scene switches on the heavy project with per-step timings.
t.include("_lib.js");
t.include("_perf.js");
skipSetup();
const file = t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json";
story("probe", "switch timings", () => {
	app.open(t.url(file)); t.waitFrames(2, 5000);
	app.setCurrentTransition("tr_cut");
	const ids = t.doc().sceneOrder, rows = [];
	for (let k = 0; k < 10; ++k) {
		const id = ids[(k * 37) % ids.length];
		const a = t.now(); app.editScene(id); const b = t.now();
		const fw = t.waitFrames(1, 1000); const c = t.now();
		app.take(); const d = t.now();
		const okp = t.waitUntil(() => t.programScene() === id, 3000, 1); const e = t.now();
		const fw2 = t.waitFrames(1, 1000); const f = t.now();
		rows.push({ editCall: Math.round(b - a), frame1: Math.round(fw), takeCall: Math.round(d - c), toProgram: Math.round(e - d), okp: okp, frame2: Math.round(fw2) });
	}
	t.metric("rows", rows);
	check("probe ran", true);
});
