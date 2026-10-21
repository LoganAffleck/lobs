// Fix-agent profiling (LOBS-025): selecting a layer in another scene on the stress project.
t.include("_lib.js");
skipSetup();
t.setTimeout(600);
story("probe-sel2", "stress: select a layer in another scene", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/stress/stress.lobs.json")); t.waitFrames(2, 20000); t.wait(3000);
	const d = t.doc(), cam = d.sourceOrder.filter(s => d.sources[s].kind === "camera")[0];
	const cams = Object.keys(d.instances).filter(i => d.instances[i].sourceId === cam);
	for (let k = 0; k < 20; ++k) { sel(cams[(k * 7) % cams.length]); t.waitFrames(1, 2000); }
	check("probe ran", true);
});
