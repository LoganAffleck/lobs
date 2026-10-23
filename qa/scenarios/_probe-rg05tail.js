t.include("_lib.js");
skipSetup();
story("RG-05t", "probe: memory every 250 ms after the switches", () => {
	const mb = () => Math.round(t.proc().privateMB);
	const lr = () => { const l = t.engineStats().layers; return l.parked + "/" + l.showing; };
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1000);
	const ids = t.doc().sceneOrder, tail = [];
	const t0 = t.now(); for (let k = 0; k < 40; ++k) { tail.push([Math.round(t.now() - t0), mb(), lr()]); t.wait(250); }
	for (let k = 0; k < 100; ++k) { app.editScene(ids[(k * 37) % ids.length]); t.waitFrames(1, 1000); }
	const t1 = t.now(); for (let k = 0; k < 48; ++k) { tail.push([Math.round(t.now() - t1), mb(), lr()]); if (k == 4) t.gc(); t.wait(250); }
	t.metric("tail", tail);
	check("recorded", true);
});
