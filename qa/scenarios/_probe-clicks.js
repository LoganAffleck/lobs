// LOBS-039 probe: settled memory over 100-click rounds of the layer clicks RG-05 makes (heavy),
// with the UI notified and not. Growth in every notified round is a UI leak.
t.include("_lib.js");
skipSetup();
story("CL-p", "probe: memory over 300 layer clicks", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1000);
	const ids = t.doc().sceneOrder;
	app.editScene(ids[1]); t.wait(200);
	const L = app.layers.map(l => l.id), m = [Math.round(settledMB())];
	const f = L.map(id => { app.selectLayer(id, false); t.wait(50); const fs = app.inspector.filters || []; return fs.map(x => x.type + ":" + x.params.length).join(","); });
	// Rounds: UI notified, UI not notified (C++ only), UI notified again.
	for (const block of [false, true, false, true]) {
		t.blockAppSignals(block);
		for (let k = 0; k < 100; ++k) { app.selectLayer(L[k % L.length], false); t.waitFrames(1, 1000); }
		t.blockAppSignals(false);
		m.push(Math.round(settledMB()));
	}
	t.metric("clicks", { layers: L.length, filtersPerLayer: f, settledMB: m });
	check("recorded", true, JSON.stringify({ layers: L.length, filtersPerLayer: f, settledMB: m }));
});
