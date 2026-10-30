// Probe: the vertical canvas (P3-1): each scene's vertical layout, editing it, and keeping it.
t.include("_lib.js");
skipSetup();

story("VT-01", "Each scene gets a vertical layout, edited with the switch, saved with the show", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const red = placeColor("Red", "#ff0000"); setT(red, "x", 0); setT(red, "y", 0); setT(red, "w", 1920); setT(red, "h", 1080);
	const listed = t.doc().sceneOrder.length;
	app.setVerticalOn(true);
	t.waitFrames(3, 3000);
	const d = t.doc();
	check("vertical is on", app.verticalOn && d.vertical && d.vertical.on);
	const v = Object.keys(d.scenes).find(id => d.scenes[id].verticalOf === A);
	check("the scene has a vertical layout", !!v);
	eq("it isn't listed in Scenes", d.sceneOrder.length, listed);
	const layer = v && d.scenes[v].layers.find(id => d.instances[id].sourceId === d.instances[red].sourceId);
	check("the layer came over", !!layer);
	if (layer) {
		const tr = eff(layer, "transform");
		t.metric("fitted", tr);
		check("fitted to the 1080-wide canvas", Math.abs(tr.w - 1080) < 2 && Math.abs(tr.h - 608) < 3, tr);
		check("centered vertically", Math.abs(tr.y - (1920 - 608) / 2) < 3, tr);
	}
	app.setEditingVertical(true);
	check("editing the vertical layout", app.editingVertical && app.editSceneId === v);
	eq("the header names the scene", app.editSceneName, d.scenes[A].name);
	check("still shows as on air", app.editingProgram);
	app.take();
	eq("Take puts the scene on air (not the layout)", t.programScene(), A);
	app.setEditingVertical(false);
	eq("back to the horizontal layout", app.editSceneId, A);
	// Saved with the show, and back after reopening.
	const path = t.out("vertical.lobs.json");
	t.saveDocTo(path);
	app.open(t.url(path)); t.waitFrames(3, 4000);
	const d2 = t.doc();
	check("vertical stays on after reopening", d2.vertical && d2.vertical.on);
	check("the layout stays after reopening", Object.keys(d2.scenes).some(id => d2.scenes[id].verticalOf === A));
	app.setVerticalOn(false);
	check("off", !app.verticalOn && !app.editingVertical);
});
