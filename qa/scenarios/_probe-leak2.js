// QA diagnostic: why does RG-05 retain ~34 MB per switch when the fix commit reports ~0?
t.include("_lib.js");
skipSetup();
t.setTimeout(300);
function mb() { t.gc(); t.wait(300); return t.proc().privateMB; }
function run(label, ids, n, frames) {
	const m0 = mb();
	for (let k = 0; k < n; ++k) { app.editScene(ids[(k * 37) % ids.length]); if (frames) t.waitFrames(1, 1000); }
	const m1 = mb();
	t.wait(10000);
	const m2 = mb();
	t.metric(label, { perSwitchMB: Math.round((m1 - m0) / n * 100) / 100, afterIdle10sPerSwitchMB: Math.round((m2 - m0) / n * 100) / 100, start: Math.round(m0) });
}
story("probe-leak2", "switch leak: with vs without frames", () => {
	fresh();
	placeColor("A", "#ff0000"); mkScene("B"); placeColor("B", "#00ff00");
	run("tiny, no frames", t.doc().sceneOrder, 50, false);
	run("tiny, with frames", t.doc().sceneOrder, 50, true);
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1000);
	const ids = t.doc().sceneOrder;
	run("heavy, no frames", ids, 50, false);
	run("heavy, with frames", ids, 50, true);
	// Same two scenes back and forth, with frames.
	run("heavy, 2 scenes, with frames", ids.slice(1, 3), 50, true);
	// Grid view on (scene thumbnails), with frames.
	check("probe ran", true);
});
