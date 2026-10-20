// Scenes, layers, groups, live lock, undo: SC-01, SC-02, LY-01, LY-02, LY-04, LY-05, LY-06, UH-01, UH-02, MP-03.
t.include("_lib.js");
skipSetup();

function pixelAt(sceneId, name, x, y) { return t.pixel(t.snapshot(sceneId, name, 0), x, y); }
function isColor(px, r, g, b) { return px.ok && Math.abs(px.r - r) < 30 && Math.abs(px.g - g) < 30 && Math.abs(px.b - b) < 30; }

story("SC-01", "Scene create, rename, reorder, delete — edge cases", () => {
	fresh();
	const a = t.doc().sceneOrder[0];
	const b = mkScene(), c = mkScene();
	const names = t.doc().sceneOrder.map(s => t.doc().scenes[s].name);
	t.note("names of 3 new scenes: " + JSON.stringify(names));
	check("new scenes get distinct names", new Set(names).size === names.length, names);
	app.renameScene(b, "   ");
	eq("blank rename ignored", t.doc().scenes[b].name, names[1]);
	app.renameScene(b, "Gameplay");
	eq("rename", t.doc().scenes[b].name, "Gameplay");
	// Delete the scene on program.
	app.editScene(b); app.take(); t.wait(600);
	eq("B on program", t.programScene(), b);
	app.deleteScene(b);
	t.wait(600);
	const prog = t.programScene();
	check("program falls back to an existing scene after deleting it", t.doc().scenes[prog] !== undefined, "program=" + prog);
	check("edited scene still exists", t.doc().scenes[app.editSceneId] !== undefined, "edit=" + app.editSceneId);
	app.undo();
	check("undo restores deleted scene", !!t.doc().scenes[b]);
	// Delete every scene.
	t.doc().sceneOrder.slice().forEach(s => app.deleteScene(s));
	const left = t.doc().sceneOrder.length;
	t.note("after deleting all scenes: " + left + " scenes, edit=" + app.editSceneId + ", program=" + t.programScene());
	check("can't end up with zero scenes (or one is recreated)", left >= 1, left);
	// Adding a layer with no scenes must not crash or orphan.
	app.addSource("color", "Orphan", "#ffffff", true);
	integrity("integrity after zero-scene edit");
	t.doc().sceneOrder.length === 0 && app.addScene();
});

story("SC-02", "Edit one scene while another is on program", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const ra = placeColor("RedA", "#ff0000"); app.fit("fill");
	app.take(); t.wait(600);
	const B = mkScene("B");
	const gb = placeColor("GreenB", "#00ff00"); app.fit("fill");
	t.wait(300);
	eq("program stays A while editing B", t.programScene(), A);
	check("program pixel is red", isColor(pixelAt("", "sc02-prog.png", 960, 540), 255, 0, 0));
	eq("app reports edit vs program", [app.editSceneId, app.programSceneId], [B, A]);
	app.take();
	check("program becomes B after Show", t.waitUntil(() => t.programScene() === B, 3000));
	t.wait(800);
	check("program pixel is green", isColor(pixelAt("", "sc02-prog2.png", 960, 540), 0, 255, 0));
});

story("LY-01", "Layer order matches render order", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const red = placeColor("Red", "#ff0000"); app.fit("fill");
	const blue = placeColor("Blue", "#0000ff"); app.fit("fill");
	app.take(); t.wait(500);
	check("top layer (blue) draws on top", isColor(pixelAt("", "ly01-a.png", 960, 540), 0, 0, 255));
	sel(blue); app.moveSelected(1);
	t.wait(300);
	check("after send backward, red on top", isColor(pixelAt("", "ly01-b.png", 960, 540), 255, 0, 0));
	eq("one undo step", app.undoLabel, "Send backward");
	sel(blue); app.moveSelected(-100);
	eq("bring to front puts blue first", t.doc().scenes[A].layers[0], blue);
	sel(blue); app.moveSelected(100);
	const L = t.doc().scenes[A].layers;
	check("send to back stays above the background", L[L.length - 1] !== blue, L.map(l => t.doc().instances[l].name));
	// Keyboard shortcuts through the real window: put red below blue first.
	sel(blue); app.moveSelected(-100);
	sel(red);
	const before = t.doc().scenes[A].layers.indexOf(red);
	check("setup: red is below the top", before > 0, before);
	const lbl0 = app.undoLabel, sel0 = JSON.stringify(app.inspector.id);
	t.key("Ctrl+]"); t.wait(100);
	t.note("Ctrl+] debug: undo label '" + lbl0 + "' -> '" + app.undoLabel + "', selection " + sel0 + ", red=" + red + ", toast='" + app.toast + "', layers=" + JSON.stringify(t.doc().scenes[A].layers.map(l => t.doc().instances[l].name)));
	const after = t.doc().scenes[A].layers.indexOf(red);
	t.note("Ctrl+] moved red from index " + before + " to " + after + " (focus window: " + t.windowCount() + " visible windows)");
	check("Ctrl+] brings forward", after === before - 1, [before, after]);
	t.key("Ctrl+["); t.wait(100);
	eq("Ctrl+[ sends backward", t.doc().scenes[A].layers.indexOf(red), before);
});

story("LY-02", "Group and ungroup keep what's on screen (R-02)", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const r = placeColor("R", "#ff0000"); setT(r, "x", 100); setT(r, "y", 100); setT(r, "w", 400); setT(r, "h", 300);
	const g2 = placeColor("G", "#00ff00"); setT(g2, "x", 700); setT(g2, "y", 400); setT(g2, "w", 400); setT(g2, "h", 300);
	app.take(); t.wait(400);
	app.selectLayer(r, false); app.selectLayer(g2, true); app.groupSelected();
	const grp = app.inspector.id;
	eq("group selected after grouping", app.inspector.mode, "group");
	// Move and scale the group, then ungroup: children must stay where they are drawn.
	app.setTransform("x", 300);
	app.setTransform("y", 200);
	t.wait(300);
	const before = t.snapshot("", "ly02-before.png", 0);
	const tb = [eff(r, "transform"), eff(g2, "transform")];
	sel(grp); app.ungroupSelected();
	t.wait(300);
	const after = t.snapshot("", "ly02-after.png", 0);
	const ta = [eff(r, "transform"), eff(g2, "transform")];
	eq("child transforms unchanged by ungroup", ta, tb);
	const pts = [[350, 250], [950, 550], [100, 100], [1500, 900]];
	check("pixels unchanged by ungroup", pts.every(p => t.pixel(before, p[0], p[1]).hex === t.pixel(after, p[0], p[1]).hex),
	      pts.map(p => [t.pixel(before, p[0], p[1]).hex, t.pixel(after, p[0], p[1]).hex]));
	// Edge cases.
	app.undo(); // regroup
	const inner = t.doc().instances[grp] ? t.doc().instances[grp].children[0] : null;
	const outer = placeColor("Outer", "#ffffff");
	app.selectLayer(inner, false); app.selectLayer(outer, true);
	const before2 = t.docJson();
	app.groupSelected();
	t.note("grouping a child with a top-level layer: " + (t.docJson() === before2 ? "no-op" : "grouped " + JSON.stringify(t.doc().instances[app.inspector.id] && t.doc().instances[app.inspector.id].children)));
	integrity("integrity after mixed-level group");
	const gid = Object.keys(t.doc().instances).filter(i => t.doc().instances[i].kind === "group")[0];
	const pre = t.docJson();
	app.dropLayer(gid, gid, "into");
	app.dropLayer(gid, t.doc().instances[gid].children[0], "into");
	eq("can't drop a group into itself or its child", t.docJson(), pre);
});

story("LY-04", "Lock and hide", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const r = placeColor("R", "#ff0000"); setT(r, "x", 200); setT(r, "y", 200); setT(r, "w", 400); setT(r, "h", 400);
	app.toggleLock(r);
	sel(r);
	t.canvasDrag(400, 400, 700, 600, 10);
	eq("locked layer doesn't move on canvas", eff(r, "transform").x, 200);
	const row = app.layers.filter(l => l.id === r)[0];
	check("Layers shows locked", row.locked);
	app.toggleLock(r);
	app.toggleVisible(r);
	app.take(); t.wait(400);
	check("hidden layer not rendered", !isColor(pixelAt("", "ly04.png", 400, 400), 255, 0, 0));
});

story("LY-05", "Duplicate, multi-delete, one undo step each", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const a = placeColor("A", "#ff0000"), b = placeColor("B", "#00ff00");
	sel(a); app.duplicateSelected();
	const dup = app.inspector.id;
	near("duplicate offsets by 20 px", eff(dup, "transform").x - eff(a, "transform").x, 20, 0.01);
	eq("duplicate name", t.doc().instances[dup].name, t.doc().instances[a].name);
	t.note("duplicate is named '" + t.doc().instances[dup].name + "' (same as original: " + (t.doc().instances[dup].name === t.doc().instances[a].name) + ")");
	app.selectLayer(a, false); app.selectLayer(b, true); app.selectLayer(dup, true);
	const h = app.history.length;
	app.deleteSelected();
	eq("3 layers deleted in one step", app.history.length - h, 1);
	eq("label", app.undoLabel, "Delete 3 layers");
	app.undo();
	check("all three back", [a, b, dup].every(i => !!t.doc().instances[i]));
});

story("LY-06", "Live lock while recording", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const r = placeColor("R", "#ff0000"); setT(r, "x", 200); setT(r, "y", 200); setT(r, "w", 400); setT(r, "h", 400);
	app.take(); t.wait(300);
	t.startRecording();
	check("recording counts as live", t.waitUntil(() => app.live, 5000));
	check("live lock on", app.liveLocked);
	sel(r);
	t.canvasDrag(400, 400, 700, 600, 10);
	eq("canvas drag refused while live", eff(r, "transform").x, 200);
	t.canvasDrag(400, 400, 700, 600, 10, "alt");
	check("Alt overrides the lock", eff(r, "transform").x !== 200, eff(r, "transform").x);
	app.undo();
	sel(r); app.nudge(10, 0);
	eq("nudge refused while live", eff(r, "transform").x, 200);
	sel(r); app.setTransform("x", 900);
	t.note("inspector edit while live-locked: x=" + eff(r, "transform").x);
	check("inspector edits on the live scene respect live lock (or are clearly allowed)", eff(r, "transform").x === 200,
	      "Inspector changed the on-air layer while locked (x=" + eff(r, "transform").x + ")");
	sel(r); app.deleteSelected();
	check("deleting an on-air layer is guarded while live-locked", !!t.doc().instances[r], "layer deleted while live-locked");
	// LOBS-006 regression: every other way of moving the on-air layer goes through the same guard.
	sel(r); app.scrubTransform("x", 950, true);
	eq("scrub refused while live", eff(r, "transform").x, 200);
	sel(r); app.fit("fill");
	eq("Fit/Fill refused while live", eff(r, "transform").w, 400);
	sel(r); app.setCrop("l", 40);
	eq("crop refused while live", eff(r, "crop").l, 0);
	sel(r); app.moveSelected(1000);
	check("reorder refused while live", t.doc().scenes[A].layers.indexOf(r) === 0, t.doc().scenes[A].layers);
	check("refusals say why", app.toast.indexOf("Live lock") >= 0, app.toast);
	// Unlocking lifts it.
	app.setLiveUnlock(true);
	sel(r); app.setTransform("x", 300);
	eq("edits work after Unlock", eff(r, "transform").x, 300);
	app.setLiveUnlock(false);
	t.stopRecording();
	t.waitUntil(() => !app.live, 10000);
});

story("UH-01", "Every edit type is undoable to the exact prior state", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const r = placeColor("R", "#ff0000");
	sel(r); app.setTransform("w", 300); // so Fill is a real change
	const edits = [
		["setTransform", () => { sel(r); app.setTransform("rot", 15); }],
		["setCrop", () => { sel(r); app.setCrop("b", 20); }],
		["scrubTransform", () => { sel(r); for (let k = 0; k <= 10; ++k) app.scrubTransform("x", 10 * k, k === 10, false); }],
		["setOpacity", () => { sel(r); app.setOpacity(0.3, true); }],
		["setCornerRadius", () => { sel(r); app.setCornerRadius(25, true); }],
		["addFilter", () => { sel(r); app.addFilter("color"); }],
		["renameLayer", () => app.renameLayer(r, "Renamed")],
		["toggleVisible", () => app.toggleVisible(r)],
		["toggleLock", () => app.toggleLock(r)],
		["fit", () => { sel(r); app.fit("fill"); }],
		["addScene", () => app.addScene()],
		["renameSource", () => app.renameSource(t.doc().instances[r].sourceId, "Src2")],
		["setSourceSetting", () => app.setSourceSetting(t.doc().instances[r].sourceId, "color", "#00ff00")],
		["addTransition", () => app.addTransition("swipe")],
		["setSwatches", () => app.setSwatches(["#123456"])],
		["duplicateSelected", () => { app.editScene(A); sel(r); app.duplicateSelected(); }],
	];
	const bad = [];
	edits.forEach(e => {
		app.editScene(A);
		const before = t.docJson(), hb = app.history.length;
		e[1]();

		const changed = t.docJson() !== before;
		const label = app.undoLabel;
		app.undo();
		const ok = t.docJson() === before;
		if (!changed || !ok)
			bad.push(e[0] + (changed ? " (undo not exact)" : " (no change / not recorded)") + " label=" + label);
		app.redo(); app.undo(); // redo/undo cycle stays exact
		if (t.docJson() !== before)
			bad.push(e[0] + " (redo/undo drift)");
	});
	check("all edit types undo exactly", bad.length === 0, bad);
	// A no-op edit should not add an undo step.
	sel(r); app.fit("fill");
	const h0 = app.history.length, j0 = t.docJson();
	sel(r); app.fit("fill");
	t.note("repeat Fill: history " + h0 + " -> " + app.history.length + ", doc changed: " + (t.docJson() !== j0));
	check("no-op edit adds no history step", app.history.length === h0, "history grew by " + (app.history.length - h0) + " for an edit that changed nothing");
});

story("UH-02", "Jump in history; new edit drops the redo branch", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const r = placeColor("R", "#ff0000");
	const snaps = [];
	for (let k = 1; k <= 20; ++k) { sel(r); app.setTransform("x", k * 10); snaps.push(t.docJson()); }
	const top = app.historyIndex;
	app.jumpHistory(top - 10);
	eq("jump back 10", t.docJson(), snaps[9]);
	app.jumpHistory(top);
	eq("jump forward again", t.docJson(), snaps[19]);
	app.jumpHistory(top - 5);
	sel(r); app.setTransform("y", 5);
	check("redo branch dropped", !app.canRedo);
	t.metric("historyLength", app.history.length);
});

story("MP-03", "Rename and delete a source used in several scenes", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const a = placeColor("Logo", "#ff00ff");
	const src = t.doc().instances[a].sourceId;
	const B = mkScene("B"), C = mkScene("C");
	copyTo(a, B, true); copyTo(a, C, false);
	const inC = Object.keys(t.doc().instances).filter(i => t.doc().instances[i].sceneId === C && t.doc().instances[i].sourceId === src)[0];
	app.renameLayer(inC, "My logo");
	app.renameSource(src, "Brand Logo");
	eq("a layer renamed by hand keeps its name", t.doc().instances[inC].name, "My logo");
	sel(a);
	t.note("layer name after source rename: '" + t.doc().instances[a].name + "', inspector sourceName: '" + app.inspector.sourceName + "'");
	const shown = app.layers.filter(l => l.id === a)[0].name;
	eq("never-renamed layer shows the new source name", shown, "Brand Logo");
	const poolRow = app.pool.filter(p => p.id === src)[0];
	t.metric("sourcesRow", poolRow || null);
	t.note("Sources row for a color source: " + (poolRow ? JSON.stringify(poolRow) : "not listed in Sources"));
	const before = t.docJson();
	app.deleteSource(src);
	t.note("deleteSource ran with no confirmation step at controller level; UI calls it directly from the menu (Main.qml:589)");
	eq("all 3 instances removed", Object.keys(t.doc().instances).filter(i => t.doc().instances[i].sourceId === src).length, 0);
	app.undo();
	eq("undo restores all exactly", t.docJson(), before);
});
