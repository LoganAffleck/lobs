// Diagnostic (LOBS-024): one canvas drag on the heavy project; the [perf] line in lobs.log breaks it down.
t.include("_lib.js");
t.include("_perf.js");
skipSetup();
t.setTimeout(200);
story("probe-drag", "drag breakdown", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1000);
	const cams = camInstances();
	sel(cams[0]);
	const tr = eff(cams[0], "transform");
	const r = t.canvasDrag(tr.x + 50, tr.y + 50, tr.x - 400, tr.y - 300, 120, "", 16);
	t.metric("dragMoveCallMs", stats(r.moves));
	check("probe ran", true);
});
