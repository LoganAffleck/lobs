// Probe: the live preview is drawn in the UI's own scene (shared GPU textures), so a window grab
// shows it, menus and dialogs draw over it without freezing it, and canvas input still works.
t.include("_lib.js");
function redShare(path) {
	// How much of a coarse grid over the window is the red layer.
	const ws = t.windowSize(), n = 40;
	let red = 0;
	for (let i = 1; i < n; i++)
		for (let j = 1; j < n; j++) {
			const p = t.pixel(path, Math.floor(ws.w * ws.dpr * i / n), Math.floor(ws.h * ws.dpr * j / n));
			if (p.r > 200 && p.g < 50 && p.b < 50) red++;
		}
	return red / ((n - 1) * (n - 1));
}

story("PV-01", "The preview is part of the window: a grab shows the canvas", () => {
	skipSetup();
	fresh();
	const k = placeColor("Red", "#ff0000");
	sel(k); app.fit("fill"); app.clearSelection();
	t.wait(800);
	check("preview isn't hidden", !app.previewHidden);
	const share = redShare(t.grab("pv01-live.png"));
	t.metric("redShare", share);
	check("the red canvas is in the grab", share > 0.1, "share=" + share);
});

story("PV-02", "A menu over the canvas leaves the video live", () => {
	fresh();
	const k = placeColor("Red", "#ff0000");
	sel(k); app.fit("fill"); app.clearSelection();
	t.wait(600);
	t.canvasClick(960, 540); t.wait(200);
	// Right-click the middle of the preview item itself (the native window's host when in-scene is off).
	const pv = t.items("objectName=previewItem").concat(t.items("objectName=preview"))[0];
	check("found the preview", !!pv);
	t.clickAt(Math.round(pv.x + pv.w / 2), Math.round(pv.y + pv.h / 2), "", true); t.wait(500);
	check("a menu is open", t.items("Open projector").length > 0);
	check("preview still not hidden", !app.previewHidden);
	const before = redShare(t.grab("pv02-menu-red.png"));
	// Change the canvas while the menu is up: the grab must show the new color (not a still).
	app.setSourceSetting(t.doc().instances[k].sourceId, "color", "#0000ff"); t.wait(600);
	const after = redShare(t.grab("pv02-menu-blue.png"));
	t.metric("redBefore", before); t.metric("redAfter", after);
	check("red under the menu", before > 0.1, "before=" + before);
	check("and it turns blue live, menu still open", after < 0.02, "after=" + after);
	t.key("Escape"); t.wait(300);
});

story("PV-03", "Dragging on the preview still moves a layer", () => {
	fresh();
	const k = placeColor("Box", "#00ff00");
	sel(k);
	app.setTransform("x", 100); app.setTransform("y", 100); app.setTransform("w", 400); app.setTransform("h", 300);
	t.wait(300);
	t.canvasDrag(300, 250, 500, 350, 10); t.wait(300);
	const tr = eff(k, "transform");
	check("moved by the drag", Math.abs(tr.x - 300) <= 2 && Math.abs(tr.y - 200) <= 2, JSON.stringify(tr));
});
