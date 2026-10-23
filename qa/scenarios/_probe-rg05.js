// RG-05 probe: private memory over time after opening heavy, then across 3 rounds of the
// same 100 switches RG-05 does. Each round ends with gc + 1 s (what RG-05 reads) and gc + 6 s.
t.include("_lib.js");
skipSetup();

story("RG-05p", "probe: RG-05 memory trajectory", () => {
	const mb = () => Math.round(t.proc().privateMB * 10) / 10;
	const tr = {};
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000);
	tr.open = []; for (const w of [0, 1000, 2000, 4000, 8000]) { t.wait(w); t.gc && t.gc(); tr.open.push(mb()); }
	const ids = t.doc().sceneOrder;
	for (let pass = 0; pass < 3; ++pass) {
		const s = [];
		for (let k = 0; k < 100; ++k) { app.editScene(ids[(k * 37) % ids.length]); t.waitFrames(1, 1000); if (k % 10 == 9) { s.push(mb()); } }
		t.gc && t.gc(); t.wait(1000); s.push(mb()); t.wait(5000); t.gc && t.gc(); s.push(mb());
		tr["pass" + pass] = s;
	}
	t.metric("trajectory", tr);
	check("trajectory recorded", true, JSON.stringify(tr));
});
