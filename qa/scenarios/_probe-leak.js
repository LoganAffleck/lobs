// RG-05 follow-up: does memory keep growing per switch, or settle once each scene's been seen?
t.include("_lib.js");
skipSetup();
story("LK-01", "Memory over 400 switches (heavy), every 50", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1000);
	const ids = t.doc().sceneOrder, marks = [Math.round(settledMB())];
	for (let k = 0; k < 400; ++k) {
		app.editScene(ids[(k * 37) % ids.length]); t.waitFrames(1, 1000);
		if (k % 50 === 49) marks.push(Math.round(settledMB()));
	}
	t.metric("scenes", ids.length); t.metric("mb", marks);
	t.note("scenes " + ids.length + " mb " + marks.join(" "));
	// Same two scenes back and forth: a cache would stay flat, a leak keeps climbing.
	const two = [Math.round(settledMB())];
	for (let k = 0; k < 200; ++k) { app.editScene(ids[k % 2]); t.waitFrames(1, 1000); if (k % 50 === 49) two.push(Math.round(settledMB())); }
	t.note("two scenes mb " + two.join(" "));
	check("ran", true);
});
