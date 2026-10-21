// Fix-agent profiling (LOBS-025): where the time goes on the stress project, per phase.
t.include("_lib.js");
skipSetup();
t.setTimeout(900);
function timed(label, n, fn) {
	app.takePerfCounters();
	const ms = [], fr = [], ph = [];
	for (let k = 0; k < n; ++k) {
		t.waitFrames(1, 2000); t.takeFramePhases();
		const a = t.now(); fn(k); const b = t.now(); t.waitFrames(1, 2000); ms.push(b - a); fr.push(t.now() - b);
		const p = t.takeFramePhases().filter(x => x.start >= b - 1)[0];
		if (p) ph.push({ lead: Math.round(p.start - b), polish: Math.round(p.polish), sync: Math.round(p.sync), render: Math.round(p.render), swap: Math.round(p.swap) });
	}
	t.metric(label + "Phases", ph.slice(0, 8));
	const c = app.takePerfCounters(), per = {};
	for (const key in c) per[key] = key === "changes" ? c[key] : Math.round(c[key] / n * 10) / 10;
	t.metric(label, { call: stats(ms), frameAfter: stats(fr), perCall: per, render: t.engineStats().live.renderMs });
}
story("probe-stress", "stress: per-phase cost", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/stress/stress.lobs.json")); t.waitFrames(2, 20000); t.wait(3000);
	const ids = t.doc().sceneOrder;
	timed("selectScene", 15, k => app.editScene(ids[(k * 37) % ids.length]));
	app.editScene(ids[1]); t.wait(500);
	const L = app.layers.map(l => l.id);
	timed("selectLayer", 30, k => app.selectLayer(L[k % L.length], false));
	const d = t.doc(), cam = d.sourceOrder.filter(s => d.sources[s].kind === "camera")[0];
	const cams = Object.keys(d.instances).filter(i => d.instances[i].sourceId === cam);
	t.note("camera layers: " + cams.length);
	sel(cams[0]); timed("linkedEditSameScene", 20, k => app.setTransform("x", 100 + k));
	timed("selInOtherScene", 20, k => sel(cams[(k * 7) % cams.length]));
	sel(L[0]); timed("plainEdit", 20, k => app.setTransform("x", 100 + k));
	check("probe ran", true);
});
