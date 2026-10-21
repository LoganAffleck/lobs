// Diagnostic (LOBS-024): one canvas drag on the heavy project; the [perf] line in lobs.log breaks it down.
t.include("_lib.js");
t.include("_perf.js");
skipSetup();
t.setTimeout(600);
story("probe-drag-stress", "drag breakdown (stress)", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/stress/stress.lobs.json")); t.waitFrames(2, 20000); t.wait(3000);
	const cams = camInstances();
	sel(cams[0]);
	const tr = eff(cams[0], "transform");
	app.takePerfCounters(); t.takeFramePhases(); t.takeFrameTimes();
	const r = t.canvasDrag(tr.x + 50, tr.y + 50, tr.x - 400, tr.y - 300, 120, "", 16);
	const ft = t.takeFrameTimes(), iv = [];
	for (let k = 1; k < ft.length; ++k) iv.push(ft[k] - ft[k - 1]);
	const ph = t.takeFramePhases();
	const col = k => stats(ph.map(p => p[k]));
	t.metric("dragMoveCallMs", stats(r.moves));
	t.metric("frameInterval", stats(iv));
	t.metric("phases", { render: col("render"), swap: col("swap"), sync: col("sync"), total: col("total") });
	t.metric("perf", app.takePerfCounters());
	check("probe ran", true);
});
