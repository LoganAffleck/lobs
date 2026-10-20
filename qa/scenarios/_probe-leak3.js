// Fix-agent diagnostic (LOBS-034): what each first scene visit keeps, and whether parking releases it.
t.include("_lib.js");
skipSetup();
t.setTimeout(400);
function mb() { t.gc(); t.wait(300); return Math.round(t.proc().privateMB); }
story("probe-leak3", "first visits: memory and layer resources", () => {
	fresh(); t.wait(1000);
	const empty = mb();
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1000);
	const ids = t.doc().sceneOrder;
	const opened = mb();
	const rows = [];
	for (let k = 0; k < ids.length; ++k) {
		app.editScene(ids[k]); t.waitFrames(2, 1000);
		if (k % 10 === 9) { t.wait(1200); rows.push({ visited: k + 1, mb: mb(), layers: t.engineStats().layers }); }
	}
	t.wait(2000);
	const after = mb();
	t.metric("memory", { empty: empty, opened: opened, afterAll: after, aboveEmpty: after - empty });
	t.metric("rows", rows);
	t.metric("layers", t.engineStats().layers);
	check("probe ran", true);
});
