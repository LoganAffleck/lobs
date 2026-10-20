// Linking: LK-01..LK-12, SC-03. The core LOBS idea: one source, many scenes, shared or not.
t.include("_lib.js");

// Two scenes, a color layer in A, and its copy in B.
function pair(linked) {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.renameScene(A, "A");
	app.editScene(A);
	const a = placeColor("Cam", "#3366ff");
	setT(a, "w", 640); setT(a, "h", 360); setT(a, "x", 100); setT(a, "y", 100);
	const B = mkScene("B");
	const b = copyTo(a, B, linked);
	return { A: A, B: B, a: a, b: b };
}

story("LK-01", "Linked copy shares every property group", () => {
	const p = pair(true);
	check("copy created in B", !!p.b, t.doc().scenes[p.B].layers);
	GROUPS.forEach(g => check("linked: " + g, linkedTogether(p.a, p.b, g), t.doc().instances[p.b].links));
	setT(p.b, "x", 777);
	eq("move in B shows in A", eff(p.a, "transform").x, 777);
	sel(p.b); app.setCrop("l", 40);
	eq("crop in B shows in A", eff(p.a, "crop").l, 40);
	sel(p.b); app.addFilter("color");
	eq("filter in B shows in A", eff(p.a, "filters").length, 1);
	sel(p.b); app.setVisibleFlag(false);
	eq("hide in B hides A", eff(p.a, "visibility").visible, false);
	// What the UI shows.
	sel(p.a);
	const row = app.layers.filter(l => l.id === p.a)[0];
	check("Layers row shows linked badge", row && row.linked, row);
	const sec = app.inspector.sections;
	check("inspector lists B as transform peer", sec.transform.peers.length === 1 && sec.transform.peers[0].scene === "B", sec.transform);
	eq("inspector linkTransform text", app.inspector.linkTransform, "B");
	integrity();
});

story("LK-02", "Independent copy shares nothing", () => {
	const p = pair(false);
	GROUPS.forEach(g => check("not linked: " + g, !linkId(p.b, g)));
	setT(p.b, "x", 777);
	eq("A keeps its x", eff(p.a, "transform").x, 100);
	sel(p.b); app.addFilter("sharpness");
	eq("A has no filters", eff(p.a, "filters").length, 0);
	eq("same source underneath", t.doc().instances[p.a].sourceId, t.doc().instances[p.b].sourceId);
	const row = app.layers.filter(l => l.id === p.b)[0];
	check("no linked badge", row && !row.linked, row);
	integrity();
});

story("LK-03", "Unlink one group, the others stay linked", () => {
	const p = pair(true);
	sel(p.b); app.unlinkGroup("transform");
	check("transform unlinked", !linkedTogether(p.a, p.b, "transform"));
	["crop", "filters", "visibility"].forEach(g => check(g + " still linked", linkedTogether(p.a, p.b, g)));
	eq("B keeps the shared value on unlink", eff(p.b, "transform").x, 100);
	setT(p.b, "x", 900);
	eq("move in B no longer moves A", eff(p.a, "transform").x, 100);
	sel(p.b); app.addFilter("color");
	eq("filter still propagates", eff(p.a, "filters").length, 1);
	sel(p.b);
	const sec = app.inspector.sections;
	check("transform section: no peers", sec.transform.peers.length === 0, sec.transform);
	check("transform section offers A to relink", sec.transform.candidates.some(c => c.id === p.a), sec.transform.candidates);
	check("crop section still shows A", sec.crop.peers.length === 1, sec.crop);
	integrity();
});

story("LK-04", "Relink: keep mine vs use theirs", () => {
	const p = pair(false);
	setT(p.b, "x", 500);
	sel(p.b); app.linkGroup("transform", p.a, true);
	check("linked after relink", linkedTogether(p.a, p.b, "transform"));
	eq("keep mine: A takes B's x", eff(p.a, "transform").x, 500);
	sel(p.b); app.unlinkGroup("transform");
	setT(p.a, "x", 42);
	sel(p.b); app.linkGroup("transform", p.a, false);
	eq("use theirs: B takes A's x", eff(p.b, "transform").x, 42);
	eq("…and A is unchanged", eff(p.a, "transform").x, 42);
	// Relink target deleted meanwhile.
	sel(p.b); app.unlinkGroup("transform");
	sel(p.a); app.deleteSelected();
	sel(p.b);
	check("deleted A no longer offered", !app.inspector.sections.transform.candidates.some(c => c.id === p.a), app.inspector.sections.transform.candidates);
	app.linkGroup("transform", p.a, true); // stale id from an old menu: must be a no-op
	check("stale relink is a no-op", !linkId(p.b, "transform") && !t.doc().instances[p.a], t.doc().instances[p.b].links);
	integrity();
});

story("LK-05", "One layer linked into 10 scenes", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const a = placeColor("Cam", "#3366ff");
	const ids = [a], scenes = [A];
	for (let k = 1; k < 10; ++k) {
		const s = mkScene("S" + k);
		scenes.push(s);
		ids.push(copyTo(a, s, true));
	}
	check("10 instances", ids.filter(Boolean).length === 10, ids);
	sel(ids[7]); app.setCrop("t", 33);
	check("crop in scene 7 reaches all 10", ids.every(i => eff(i, "crop").t === 33), ids.map(i => eff(i, "crop").t));
	sel(ids[7]);
	eq("inspector counts 9 peers", app.inspector.sections.crop.peers.length, 9);
	app.unlinkGroup("crop");
	check("others stay linked to each other", linkedTogether(ids[0], ids[9], "crop"));
	sel(ids[0]); app.setCrop("t", 5);
	check("scene 7 unaffected after unlink", eff(ids[7], "crop").t === 33 && eff(ids[9], "crop").t === 5);
	// Unlink every member one at a time: the last one must not keep a stale link.
	ids.forEach(i => { sel(i); app.unlinkGroup("crop"); });
	check("no crop links left", ids.every(i => !linkId(i, "crop")), ids.map(i => linkId(i, "crop")));
	check("no crop link objects left", Object.keys(t.doc().links).every(l => t.doc().links[l].group !== "crop"), Object.keys(t.doc().links).length);
	integrity();
});

story("LK-06", "Undo and redo across linked edits", () => {
	const p = pair(true);
	setT(p.b, "x", 640);
	check("label names the edit", /x/i.test(app.undoLabel), app.undoLabel);
	app.undo();
	check("undo reverts both", eff(p.a, "transform").x === 100 && eff(p.b, "transform").x === 100);
	app.redo();
	check("redo reapplies both", eff(p.a, "transform").x === 640 && eff(p.b, "transform").x === 640);
	sel(p.b); app.unlinkGroup("filters");
	check("unlink label", /unlink/i.test(app.undoLabel), app.undoLabel);
	app.undo();
	check("undo of unlink relinks", linkedTogether(p.a, p.b, "filters"));
	const hist = app.history.map(h => h.label);
	t.metric("history", hist);
	check("history labels are readable (no ids)", hist.every(l => !/ins_|lnk_|src_|scn_/.test(l)), hist);
	integrity();
});

story("LK-07", "Save and reopen keeps the link graph", () => {
	const p = pair(true);
	const C = mkScene("C");
	const c = copyTo(p.a, C, true);
	sel(c); app.unlinkGroup("transform");
	setT(c, "x", 1234);
	sel(p.b); app.addFilter("color");
	const before = t.doc();
	const after = roundTrip("lk07");
	eq("same instance count", Object.keys(after.instances).length, Object.keys(before.instances).length);
	eq("same link count", Object.keys(after.links).length, Object.keys(before.links).length);
	GROUPS.forEach(g => eq("a-b " + g + " still linked", linkedTogether(p.a, p.b, g, after), true));
	eq("c transform stays local", linkId(c, "transform", after), undefined);
	eq("c transform value", eff(c, "transform", after).x, 1234);
	eq("filter survived", eff(p.a, "filters", after).length, 1);
	// Idempotent: a second save of the reopened doc is byte-identical.
	const f1 = t.out("lk07-a.lobs.json"), f2 = t.out("lk07-b.lobs.json");
	app.saveAs(t.url(f1)); app.open(t.url(f1)); app.saveAs(t.url(f2));
	check("second save is byte-identical", t.readFile(f1) === t.readFile(f2), [t.fileSize(f1), t.fileSize(f2)]);
	const beforeSources = Object.keys(before.sources).length, afterSources = Object.keys(after.sources).length;
	eq("open doesn't add sources", afterSources, beforeSources);
	integrity();
});

story("LK-08", "Deleting linked things", () => {
	let p = pair(true);
	const C = mkScene("C");
	const c = copyTo(p.a, C, true);
	sel(p.b); app.deleteSelected();
	check("(a) A and C still linked", linkedTogether(p.a, c, "transform"));
	integrity("(a) integrity");
	app.undo();
	check("(a) undo restores B linked", t.doc().instances[p.b] && linkedTogether(p.a, p.b, "transform"));
	app.deleteScene(C);
	check("(b) A-B unaffected by scene delete", linkedTogether(p.a, p.b, "crop"));
	integrity("(b) integrity");
	app.undo();
	const src = t.doc().instances[p.a].sourceId;
	const beforeJson = t.docJson();
	app.deleteSource(src);
	const d = t.doc();
	check("(c) source gone", !d.sources[src]);
	check("(c) all its instances gone", Object.keys(d.instances).every(i => d.instances[i].sourceId !== src));
	eq("(c) no links left", Object.keys(d.links).length, 0);
	integrity("(c) integrity");
	app.undo();
	eq("(c) undo restores exactly", t.docJson(), beforeJson);
});

story("LK-09", "Linked copy of a group (R-01)", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.renameScene(A, "A");
	app.editScene(A);
	const x = placeColor("X", "#ff0000"), y = placeColor("Y", "#00ff00");
	setT(x, "x", 0); setT(y, "x", 300);
	app.selectLayer(x, false); app.selectLayer(y, true); app.groupSelected();
	const g = t.doc().scenes[A].layers.filter(id => t.doc().instances[id].kind === "group")[0];
	check("group made", !!g, t.doc().scenes[A].layers);
	const B = mkScene("B");
	const gb = copyTo(g, B, true);
	const kids = t.doc().instances[gb].children;
	check("children copied", kids && kids.length === 2, kids);
	check("children linked", kids.every((k, n) => linkedTogether(k, t.doc().instances[g].children[n], "transform")));
	// Move the group in B; does the group in A move? PLAN §3.5 says groups link like layers.
	sel(gb); app.setTransform("x", 500);
	const ax = eff(x, "transform").x;
	check("moving linked group in B moves A's group (PLAN §3.5)", ax !== 0, "A child x=" + ax + " group links=" + JSON.stringify(t.doc().instances[gb].links));
	t.note("Group itself linked: " + JSON.stringify(t.doc().instances[gb].links || {}));
	// The group's own properties: hide or fade the group in B.
	sel(gb); app.setVisibleFlag(false);
	check("hiding the linked group in B hides it in A (PLAN §3.5)", eff(g, "visibility").visible === false, "A group visible=" + eff(g, "visibility").visible);
	sel(gb); app.setVisibleFlag(true);
	sel(gb); app.setOpacity(0.5, true);
	check("group opacity links (PLAN §3.5)", eff(g, "visibility").opacity === 0.5, "A group opacity=" + eff(g, "visibility").opacity);
	sel(gb);
	const row = app.layers.filter(l => l.id === gb)[0];
	t.note("Group row linked badge in B: " + (row && row.linked));
	// Child crop propagates.
	sel(kids[0]); app.setCrop("l", 10);
	eq("child crop propagates", eff(t.doc().instances[g].children[0], "crop").l, 10);
	sel(gb); app.ungroupSelected();
	check("child links survive ungroup", linkedTogether(kids[0], t.doc().instances[g].children[0], "crop"));
	integrity();
});

story("LK-10", "Filters: linked chains propagate, independent diverge", () => {
	const p = pair(true);
	sel(p.a); app.addFilter("color"); app.addFilter("sharpness");
	sel(p.b); app.toggleFilter(0);
	eq("toggle propagates", eff(p.a, "filters")[0].enabled, false);
	const h0 = app.history.length;
	sel(p.b);
	for (let k = 1; k <= 10; ++k)
		app.setFilterValue(1, "amount", k / 10, k === 10);
	eq("scrub = one undo step", app.history.length - h0, 1);
	near("scrubbed value propagates", eff(p.a, "filters")[1].settings.amount, 1.0, 1e-9);
	const engineFilters = t.inputFilters();
	t.metric("engineFilters", engineFilters);
	sel(p.b); app.removeFilter(0);
	eq("remove propagates", eff(p.a, "filters").length, 1);
	integrity();
});

story("LK-12", "Reset on a linked section", () => {
	const p = pair(true);
	sel(p.b); app.setCrop("l", 50);
	t.wait(2500); // let the copy toast expire so we only see what reset says
	const toastBefore = app.toast;
	app.resetGroup("crop");
	const aCrop = eff(p.a, "crop").l;
	const said = app.toast !== toastBefore ? app.toast : "";
	t.note("Reset crop in B: A crop.l " + 50 + " -> " + aCrop + ", still linked: " + linkedTogether(p.a, p.b, "crop") + ", feedback: '" + said + "'");
	check("reset of a linked section tells the user other scenes changed", aCrop === 50 || /scene|linked|everywhere/i.test(said),
	      "A was reset too (crop.l=" + aCrop + ") and feedback was '" + said + "'");
	eq("reset is one undo step", app.undoLabel, "Reset crop");
});

story("SC-03", "Duplicate scene: plain vs linked", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.renameScene(A, "Main");
	app.editScene(A);
	const a = placeColor("Cam", "#3366ff");
	app.duplicateScene(A, false);
	const plain = app.editSceneId;
	eq("plain copy name", t.doc().scenes[plain].name, "Main copy");
	const pa = t.doc().scenes[plain].layers.filter(l => t.doc().instances[l].sourceId === t.doc().instances[a].sourceId)[0];
	setT(pa, "x", 999);
	check("plain copy is independent", eff(a, "transform").x !== 999);
	app.duplicateScene(A, true);
	const linked = app.editSceneId;
	const la = t.doc().scenes[linked].layers.filter(l => t.doc().instances[l].sourceId === t.doc().instances[a].sourceId)[0];
	GROUPS.forEach(g => check("linked dup: " + g, linkedTogether(a, la, g)));
	app.duplicateScene(A, true);
	const third = app.editSceneId;
	const ta = t.doc().scenes[third].layers.filter(l => t.doc().instances[l].sourceId === t.doc().instances[a].sourceId)[0];
	setT(ta, "y", 321);
	check("3-way link", eff(a, "transform").y === 321 && eff(la, "transform").y === 321);
	// The scene background layer: is it linked across duplicates too?
	const bgA = t.doc().scenes[A].layers.filter(l => l !== a);
	t.note("Background layers in Main: " + bgA.length + "; linked to dup: " + bgA.map(b => t.doc().instances[b].links ? Object.keys(t.doc().instances[b].links).length : 0));
	integrity();
});

story("MP-01", "One source in many scenes = one engine input", () => {
	fresh();
	const cam = addKind("camera", "Facecam", "", true);
	const devs = app.devices(cam.source);
	t.metric("cameraDevices", devs);
	if (!devs.length) {
		t.blocked("no camera device visible to LOBS");
		return;
	}
	const first = t.doc().sceneOrder[0];
	const ids = [cam.inst];
	for (let k = 0; k < 4; ++k) ids.push(copyTo(cam.inst, mkScene("Cam" + k), k % 2 === 0));
	t.setAllScenesShowing(true);
	t.wait(3000);
	const inputs = t.obsInputs().filter(i => i.id === "dshow_input");
	t.metric("dshowInputs", inputs);
	eq("exactly one camera capture in libobs", inputs.length, 1);
	check("camera produces frames", inputs.length && inputs[0].width > 0, inputs);
	const st = app.sourceStatus[cam.source];
	t.metric("sourceStatus", st);
	check("no device problem reported", !st || !st.problem, st);
	t.setAllScenesShowing(false);
});
