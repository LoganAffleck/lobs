// Harness self-check: proves each TestApi capability works before real stories rely on it.

story("H-01", "controller and doc are reachable", () => {
	const d = fresh();
	check("doc has scenes", d.sceneOrder.length >= 1, d.sceneOrder);
	const before = d.sceneOrder.length;
	app.addScene();
	eq("addScene adds one scene", t.doc().sceneOrder.length, before + 1);
	check("app.scenes property mirrors doc", app.scenes.length === before + 1, app.scenes.length);
	check("canUndo after edit", app.canUndo);
	app.undo();
	eq("undo removes it", t.doc().sceneOrder.length, before);
});

story("H-02", "engine snapshot and pixel read", () => {
	fresh();
	app.addSource("color", "Red", "#ff0000", true);
	app.fit("fill");
	app.take();
	t.wait(500);
	const p = t.snapshot("", "h02.png", 0);
	const px = t.pixel(p, 960, 540);
	check("snapshot is canvas-sized", px.w === 1920 && px.h === 1080, px);
	check("center pixel is red", px.ok && px.r > 200 && px.g < 40 && px.b < 40, px);
	t.note("obs inputs: " + t.obsInputs().length);
});

story("H-03", "UI items are findable and window grabs", () => {
	const all = t.items("Scenes");
	check("finds a 'Scenes' label", all.length > 0, all.slice(0, 3));
	check("window grab saved", t.exists(t.grab("h03-window.png")));
	t.metric("window", t.windowSize());
	t.metric("frameWaitMs", t.waitFrames(1));
});

story("H-04", "canvas drag through the controller", () => {
	fresh();
	app.addSource("color", "Box", "#00ff00", true);
	const id = app.layers[0] ? app.layers[0].id : null;
	check("a layer exists", !!id, app.layers);
	const tr0 = inst(id).props.transform;
	app.selectLayer(id, false);
	const cx = tr0.x + tr0.w / 2, cy = tr0.y + tr0.h / 2;
	const r = t.canvasDrag(cx, cy, cx + 100, cy + 50, 10);
	const tr1 = inst(id).props.transform;
	near("moved by ~100 px in x", tr1.x - tr0.x, 100, 3);
	near("moved by ~50 px in y", tr1.y - tr0.y, 50, 3);
	t.metric("dragMoves", stats(r.moves));
});

story("H-05", "process metrics, isolation", () => {
	const p = t.proc();
	check("process metrics", p.workingSetMB > 0 && p.handles > 0, p);
	t.metric("proc", p);
	check("settings isolated (test-mode app data)", t.appDataDir().toLowerCase().indexOf("qttest") >= 0, t.appDataDir());
	check("record folder redirected", String(t.engineSettings().record ? JSON.stringify(t.engineSettings()) : JSON.stringify(t.engineSettings())).indexOf("rec") >= 0, t.engineSettings());
});

story("H-06", "short recording lands in the run folder", () => {
	fresh();
	app.addSource("color", "Bg", "#203040", true);
	const n = t.recordingsFinished();
	t.recordClip(2);
	check("recording finished", t.waitUntil(() => t.recordingsFinished() > n, 20000));
	const r = t.lastRecording();
	check("recording ok", r.ok, r);
	check("inside run folder", String(r.path).replace(/\\/g, "/").indexOf(t.out("rec").replace(/\\/g, "/")) === 0, r.path);
});

// LOBS-041: after any crashed self-test, later ones opened "LOBS closed unexpectedly" over the
// window, and every story that clicks or types failed.
story("H-07", "No crash prompt covers a self-test's window", () => {
	t.wait(1500); // the prompt opens 900 ms after startup when a crash is pending
	eq("no crash report dialog", t.items("Report on GitHub").length, 0);
});
