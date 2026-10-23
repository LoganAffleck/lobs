// Fix pass 3 diagnostic (LOBS-025): what each controller signal costs the UI per emission, on
// the stress show with a camera layer selected (as PF-04 drags it).
t.include("_lib.js");
t.include("_perf.js");
skipSetup();
t.setTimeout(600);
story("probe-signals", "UI cost per signal (stress)", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/stress/stress.lobs.json")); t.waitFrames(2, 20000); t.wait(3000);
	sel(camInstances()[0]); t.wait(500);
	const mx = app.mixer; t.metric("mixer", { count: mx.length, kinds: mx.slice(0, 5).map(m => JSON.stringify(m).slice(0, 160)), levelsKeys: Object.keys(app.levels).length });
	const c = t.signalCost(20), top = Object.keys(c).sort((a, b) => c[b] - c[a]).slice(0, 12).map(k => k + " " + c[k].toFixed(2));
	t.metric("signalCostMs", c);
	check("probe ran", true, JSON.stringify(t.metric ? { mixer: mx.length, levels: Object.keys(app.levels).length } : {}) + " " + top.join(", "));
});
