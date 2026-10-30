// Probe: the vertical canvas (P3-1): each scene's vertical layout, editing it, and keeping it.
t.include("_lib.js");
skipSetup();

story("VT-01", "Each scene gets a vertical layout, edited with the switch, saved with the show", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const red = placeColor("Red", "#ff0000"); setT(red, "x", 0); setT(red, "y", 0); setT(red, "w", 1920); setT(red, "h", 1079); // not quite full: fitted, not stretched
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
		check("fitted to the 1080-wide canvas", Math.abs(tr.w - 1080) < 2 && Math.abs(tr.h - 607) < 3, tr);
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

story("VT-02", "The vertical layout follows the scene one way, until a setting is changed there", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const cam = placeColor("Cam", "#00ff00"); setT(cam, "x", 0); setT(cam, "y", 0); setT(cam, "w", 960); setT(cam, "h", 540);
	app.setVerticalOn(true);
	const vOf = () => { const d = t.doc(); return Object.keys(d.scenes).find(id => d.scenes[id].verticalOf === A); };
	const follower = (main) => { const d = t.doc(); return d.scenes[vOf()].layers.find(id => d.instances[id].follows === main); };
	const f = follower(cam);
	check("the layer has a follower", !!f);
	// Desktop moves: vertical follows (fitted).
	app.editScene(A); setT(cam, "x", 960);
	eq("follows desktop's move, fitted", eff(f, "transform").x, Math.round(960 * 1080 / 1920));
	// Vertical moves: desktop stays.
	app.setEditingVertical(true); setT(f, "y", 100);
	eq("desktop doesn't follow vertical", eff(cam, "transform").x, 960);
	eq("vertical keeps its move", eff(f, "transform").y, 100);
	// Desktop moves again: the vertical position is its own now; visibility still follows.
	app.setEditingVertical(false); setT(cam, "x", 0);
	eq("its own position stays", eff(f, "transform").y, 100);
	app.toggleVisible(cam);
	eq("visibility still follows", eff(f, "visibility").visible, false);
	app.toggleVisible(cam);
	// Layers added or removed on desktop come and go in vertical.
	const extra = placeColor("Extra", "#0000ff");
	check("a new desktop layer appears in vertical", !!follower(extra));
	const extraF = follower(extra);
	sel(extra); app.deleteSelected();
	check("a deleted desktop layer goes from vertical", !t.doc().instances[extra] && !t.doc().instances[extraF], [!!t.doc().instances[extra], !!t.doc().instances[extraF]]);
	// Saved: follows and own come back.
	const path = t.out("follow.lobs.json"); t.saveDocTo(path);
	app.open(t.url(path)); t.waitFrames(2, 3000);
	const d = t.doc(), fi = d.instances[f];
	check("follows is saved", fi && fi.follows === cam, fi);
	check("its own settings are saved", fi && (fi.own || []).indexOf("transform") >= 0, fi && fi.own);
});

story("VT-03", "An older layout's copies are adopted, and picking a scene keeps Vertical", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const cam = placeColor("Cam", "#00ff00");
	app.setVerticalOn(true);
	// A show saved before layers followed: the vertical copy has no "follows".
	const path = t.out("legacy.lobs.json"); t.saveDocTo(path);
	const j = JSON.parse(t.readFile(path));
	for (const id in j.instances) { delete j.instances[id].follows; delete j.instances[id].own; }
	t.writeFile(path, JSON.stringify(j));
	app.open(t.url(path)); t.waitFrames(2, 3000);
	app.editScene(A); setT(cam, "x", 500); // any edit syncs
	const d = t.doc(), v = Object.keys(d.scenes).find(id => d.scenes[id].verticalOf === A);
	const same = d.scenes[v].layers.filter(id => d.instances[id].sourceId === d.instances[cam].sourceId);
	eq("the layer is there once", same.length, 1);
	check("and follows the scene's layer", same.length && d.instances[same[0]].follows === cam);
	// Vertical, then another scene: still Vertical.
	app.addScene();
	const B = t.doc().sceneOrder.find(id => id !== A);
	app.editScene(A); app.setEditingVertical(true);
	app.editScene(B);
	check("still editing Vertical", app.editingVertical, app.editSceneId);
	eq("of the scene picked", t.doc().scenes[app.editSceneId].verticalOf, B);
});

story("VT-04", "A full-canvas background covers the vertical canvas", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const bg = placeColor("Bg", "#2563eb"); setT(bg, "x", 0); setT(bg, "y", 0); setT(bg, "w", 1920); setT(bg, "h", 1080);
	app.setVerticalOn(true);
	const d = t.doc(), v = Object.keys(d.scenes).find(id => d.scenes[id].verticalOf === A);
	const f = d.scenes[v].layers.find(id => d.instances[id].follows === bg);
	const tr = eff(f, "transform");
	check("covers 1080 × 1920", tr.x <= 0 && tr.y <= 0 && tr.x + tr.w >= 1080 && tr.y + tr.h >= 1920, tr);
});
