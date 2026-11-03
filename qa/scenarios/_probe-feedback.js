// Probe: tester feedback. Crop is Free unless a ratio is picked; shapes resize freely; the arrow
// tool keeps drawing; switching scenes plays the transition on the canvas.
t.include("_lib.js");

story("FB-01", "Cropping a 16:9 layer stays Free", () => {
	skipSetup();
	fresh();
	const a = placeColor("Wide", "#ff00ff");
	setT(a, "x", 100); setT(a, "y", 100); setT(a, "w", 800); setT(a, "h", 450);
	sel(a); t.wait(300);
	app.cropMode = true; t.wait(200);
	t.canvasDrag(900, 550, 880, 400, 10); t.wait(200);
	app.cropMode = false; t.wait(100);
	const r = eff(a, "transform");
	t.note("after crop drag: " + JSON.stringify(eff(a, "crop")) + " " + JSON.stringify(r));
	eq("no ratio held", app.cropAspect, 0);
	check("crop is not 16:9 any more", Math.abs(r.w / r.h - 16 / 9) > 0.05);
});

story("FB-02", "A shape's edge moves only that side", () => {
	fresh();
	app.clearSelection();
	app.tool = "rect"; t.wait(200);
	t.canvasDrag(300, 300, 700, 500, 10); t.wait(500);
	const id = app.inspector.id;
	app.tool = "select"; sel(id); t.wait(200);
	const r0 = eff(id, "transform");
	t.canvasDrag(r0.x + r0.w, r0.y + r0.h / 2, r0.x + r0.w + 100, r0.y + r0.h / 2, 10); t.wait(200);
	const r = eff(id, "transform");
	t.note(JSON.stringify(r0) + " -> " + JSON.stringify(r));
	eq("wider by 100", r.w, r0.w + 100);
	eq("height unchanged", r.h, r0.h);
	eq("top unchanged", r.y, r0.y);
});

story("FB-03", "The arrow tool keeps drawing and selects nothing", () => {
	fresh();
	app.clearSelection();
	const before = Object.keys(t.doc().instances).length;
	app.tool = "arrow"; t.wait(200);
	t.canvasDrag(200, 200, 500, 300, 10); t.wait(300);
	t.canvasDrag(200, 600, 500, 700, 10); t.wait(300);
	eq("still the arrow tool", app.tool, "arrow");
	eq("two arrows added", Object.keys(t.doc().instances).length, before + 2);
	eq("nothing selected", app.inspector.id || "", "");
	app.tool = "select";
});

story("FB-04", "Switching scenes plays the transition on the canvas", () => {
	fresh();
	const s1 = t.doc().sceneOrder[0];
	app.editScene(s1);
	const a = placeColor("Red", "#ff0000");
	setT(a, "x", 0); setT(a, "y", 0); setT(a, "w", 1920); setT(a, "h", 1080);
	const s2 = mkScene("Blue");
	const b = placeColor("Blue", "#0000ff");
	setT(b, "x", 0); setT(b, "y", 0); setT(b, "w", 1920); setT(b, "h", 1080);
	const tr = app.addTransition("fade");
	app.setCurrentTransition(tr);
	app.updateTransition(tr, "durationMs", 3000);
	app.editScene(s1); t.wait(3500);
	app.clearSelection();
	app.editScene(s2); t.wait(1200);
	t.grab("fb04-mid.png");
	t.wait(2500);
	t.grab("fb04-end.png");
});

story("FB-05", "A stinger plays on the canvas and on air", () => {
	fresh();
	const s1 = t.doc().sceneOrder[0];
	app.editScene(s1);
	const a = placeColor("Red", "#ff0000");
	setT(a, "x", 0); setT(a, "y", 0); setT(a, "w", 1920); setT(a, "h", 1080);
	const s2 = mkScene("Green");
	const b = placeColor("Green", "#00ff00");
	setT(b, "x", 0); setT(b, "y", 0); setT(b, "w", 1920); setT(b, "h", 1080);
	const tr = app.addTransition("stinger");
	app.updateTransition(tr, "path", t.scriptDir() + "/../runs/fixtures/stinger-blue.webm");
	app.setCurrentTransition(tr);
	t.note("transition " + JSON.stringify(t.doc().transitions.filter(x => x.id === tr)[0]));
	app.editScene(s1); t.wait(3000);
	app.clearSelection();
	app.editScene(s2);
	for (let i = 0; i < 6; i++) { t.wait(250); t.grab("fb05-canvas-" + i + ".png"); }
	t.wait(2000);
	app.editScene(s1); t.wait(2000);
	app.take(); t.wait(3000);
});
