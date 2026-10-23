// Fix pass 3 diagnostic (LOBS-025, PF-04-stress): one move per frame on the stress show, with the
// UI notified and not, and where each frame's time goes (sync = QML/items, render, swap).
t.include("_lib.js");
t.include("_perf.js");
skipSetup();
t.setTimeout(600);
story("probe-drag-frames", "per-frame drag breakdown (stress)", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/stress/stress.lobs.json")); t.waitFrames(2, 20000); t.wait(3000);
	const cams = camInstances();
	sel(cams[0]);
	app.setLiveUnlock(true);
	const out = {};
	// Also idle (a frame forced each time, nothing dragged): what the UI costs with no drag at all.
	for (const [blocked, idle] of [[false, false], [true, false], [false, true], [true, true]]) {
		const tr = eff(cams[0], "transform");
		app.takePerfCounters(); t.takeFramePhases(); t.takeFrameTimes();
		t.blockAppSignals(blocked);
		const r = idle ? { moves: [] } : t.canvasDrag(tr.x + 50, tr.y + 50, tr.x - 400, tr.y - 300, 120, "", -1);
		if (idle) for (let k = 0; k < 120; ++k) t.waitFrames(1, 1000);
		t.blockAppSignals(false);
		const ft = t.takeFrameTimes(), iv = [];
		for (let k = 1; k < ft.length; ++k) iv.push(ft[k] - ft[k - 1]);
		const ph = t.takeFramePhases();
		const col = k => stats(ph.map(p => p[k]));
		out[(idle ? "idle-" : "drag-") + (blocked ? "blocked" : "notified")] = { move: stats(r.moves), interval: stats(iv),
			sync: col("sync"), render: col("render"), swap: col("swap"), perf: app.takePerfCounters() };
		t.wait(1500);
	}
	t.metric("drag", out);
	check("probe ran", true, JSON.stringify(out));
});
