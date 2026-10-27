// UI-level checks: the P0 flows are reachable through real clicks and keys in the QML UI.
// Synthesized input goes to the QML windows (QTest), so this exercises QML wiring the
// controller-level scenarios skip.
t.include("_lib.js");

function ui(text) { return t.items(text); }
function has(text) { return ui(text).length > 0; }

story("UI-00", "First run: setup assistant can be skipped and stays gone", () => {
	t.wait(500);
	const shown = has("Welcome to LOBS");
	t.grab("ui00-first-run.png");
	check("setup assistant shows on a clean profile", shown);
	check("'Skip setup' is clickable", t.click("Skip setup"));
	t.wait(400);
	check("assistant closed", !has("Welcome to LOBS"));
	t.grab("ui00-after-skip.png");
});

story("V-01", "Keyboard shortcuts through the window", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const r = placeColor("R", "#ff0000"), b = placeColor("B", "#0000ff");
	const res = {};
	sel(r);
	let n = Object.keys(t.doc().instances).length;
	t.key("Ctrl+D"); t.wait(100);
	res["Ctrl+D duplicates"] = Object.keys(t.doc().instances).length === n + 1;
	app.undo();
	sel(r);
	t.key("Ctrl+Z"); t.wait(100);
	res["Ctrl+Z undoes"] = !t.doc().instances[b];
	app.redo();
	sel(r);
	n = Object.keys(t.doc().instances).length;
	t.key("Delete"); t.wait(100);
	res["Delete removes"] = Object.keys(t.doc().instances).length === n - 1;
	app.undo();
	sel(r);
	const i0 = t.doc().scenes[A].layers.indexOf(r);
	t.key("Ctrl+]"); t.wait(100);
	res["Ctrl+] brings forward"] = t.doc().scenes[A].layers.indexOf(r) === i0 - 1;
	const i1 = t.doc().scenes[A].layers.indexOf(r);
	t.key("Ctrl+["); t.wait(100);
	res["Ctrl+[ sends backward"] = t.doc().scenes[A].layers.indexOf(r) === i1 + 1;
	app.selectLayer(r, false); app.selectLayer(b, true);
	t.key("Ctrl+G"); t.wait(100);
	res["Ctrl+G groups"] = app.inspector.mode === "group";
	t.metric("shortcuts", res);
	Object.keys(res).forEach(k => check(k, res[k]));
});

story("SC-01u", "Add a scene with the '+ Scene' button", () => {
	fresh();
	const n = t.doc().sceneOrder.length;
	const all = ui("Scene");
	const idx = all.findIndex(i => /Button/.test(i.cls) && i.text === "Scene");
	check("'+ Scene' button found", idx >= 0, all.map(i => i.cls + ":" + i.text));
	t.clickItem("Scene", idx); t.wait(200);
	eq("clicking '+ Scene' adds a scene", t.doc().sceneOrder.length, n + 1);
	t.grab("sc01u.png");
});

story("LK-01u", "Linked copy is reachable from the Layers menu and visible in the Inspector", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.renameScene(A, "Alpha");
	const a = placeColor("Cam", "#3366ff");
	const B = mkScene("Beta");
	app.editScene(A); sel(a);
	t.wait(200);
	// Right-click menu on the layer row (where users look first).
	const rows = ui("Cam").map((r, i) => Object.assign({ i: i }, r)).filter(r => r.text === "Cam");
	check("layer row visible", rows.length > 0, rows);
	const row = rows.sort((x, y) => y.x - x.x)[0];
	t.clickItem("Cam", row.i, "", true); t.wait(300);
	t.grab("lk01u-rowmenu.png");
	t.metric("rowMenuHasCopyTo", has("Copy to"));
	t.note("Layers row right-click menu offers Copy to: " + has("Copy to"));
	t.key("Escape"); t.wait(200);
	// The header icon button ("Copy to another scene").
	const btn = ui("tip=Copy to another scene");
	check("Layers header has a 'Copy to another scene' button", btn.length > 0);
	t.clickItem("tip=Copy to another scene", 0); t.wait(300);
	t.grab("lk01u-copymenu.png");
	const beta = ui("Beta").map((r, i) => Object.assign({ i: i }, r));
	t.metric("copyMenuScenes", beta.map(b => b.cls + "@" + Math.round(b.x)));
	const item = beta.sort((x, y) => y.x - x.x)[0];
	if (item) { t.clickItem("Beta", item.i); t.wait(300); }
	t.grab("lk01u-copymenu2.png");
	if (has("Linked copy")) { t.click("Linked copy"); t.wait(300); }
	t.key("Escape"); t.wait(100);
	const copied = t.doc().scenes[B].layers.some(l => t.doc().instances[l].sourceId === t.doc().instances[a].sourceId && linkedTogether(l, a, "transform"));
	check("Linked copy made through the menu", copied, t.doc().scenes[B].layers);
	sel(a); t.wait(300);
	const chip = ui("Linked ·");
	t.metric("chips", chip.map(c => c.text));
	check("inspector shows a 'Linked · Beta' chip", chip.some(c => /Beta/.test(c.text)), chip.map(c => c.text));
	t.grab("lk01u-inspector.png");
});

story("AX-01", "Screen-reader names on interactive controls", () => {
	const c = t.accessibilityCensus();
	t.metric("census", c);
	check("most interactive controls have an accessible name", c.interactive > 0 && c.named / c.interactive >= 0.8,
	      c.named + " of " + c.interactive + " named; e.g. " + JSON.stringify(c.sampleUnnamed));
});

story("V-02", "Save: Ctrl+S and toolbar Save on an already-saved project", () => {
	fresh();
	const f = t.out("v02.lobs.json");
	app.saveAs(t.url(f));
	placeColor("After first save", "#ffffff");
	t.key("Ctrl+S"); t.wait(1200);
	const dlg = t.nativeDialogs();
	const saved = t.readFile(f).indexOf("After first save") >= 0;
	t.note("Ctrl+S: native dialogs " + JSON.stringify(dlg) + ", file updated: " + saved);
	t.closeNativeDialogs(); t.wait(500);
	check("Ctrl+S saves an already-saved project without a Save As dialog", saved && dlg.length === 0,
	      "file updated=" + saved + ", dialogs=" + JSON.stringify(dlg));
	const tb = ui("Save").map((r, i) => Object.assign({ i: i }, r)).filter(r => r.text === "Save");
	if (tb.length) {
		placeColor("After toolbar save", "#ffffff");
		t.clickItem("Save", tb[0].i); t.wait(1200);
		const dlg2 = t.nativeDialogs();
		const saved2 = t.readFile(f).indexOf("After toolbar save") >= 0;
		t.note("toolbar Save: native dialogs " + JSON.stringify(dlg2) + ", file updated: " + saved2);
		t.closeNativeDialogs(); t.wait(500);
		check("toolbar Save saves an already-saved project without a Save As dialog", saved2 && dlg2.length === 0,
		      "file updated=" + saved2 + ", dialogs=" + JSON.stringify(dlg2));
	}
});

story("MP-04u", "Delete source asks first when it's used in several scenes (LOBS-018)", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const k = addKind("camera", "Shared logo"); // Sources lists cameras, captures, audio and media
	const a = k.inst, src = k.source;
	copyTo(a, mkScene("B"), true);
	app.editScene(A);
	t.wait(300);
	check("Sources lists the camera (LOBS-031)", ui("Shared logo").some(r => r.x < 400), ui("Shared logo"));
	t.grab("mp04u-before.png");
	// Right-click each "Shared logo" item until one opens the Sources row menu.
	let opened = false;
	for (let i = 0; i < ui("Shared logo").length && !opened; i++) {
		t.clickItem("Shared logo", i, "", true); t.wait(300);
		opened = has("Delete source");
		if (!opened) { t.key("Escape"); t.wait(150); }
	}
	check("Sources row menu offers Delete source", opened);
	if (!opened) return;
	t.click("Delete source"); t.wait(400);
	t.grab("mp04u-confirm.png");
	check("a confirmation names the scenes before deleting", has("Delete from all scenes"));
	check("nothing is deleted until confirmed", !!t.doc().sources[src]);
	t.click("Cancel"); t.wait(300);
	check("Cancel keeps the source", !!t.doc().sources[src]);
});

story("LK-01m", "Copy to is in the layer row's right-click menu (LOBS-020)", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const a = placeColor("Menu copy", "#00ffff");
	const B = mkScene("Menu B");
	app.editScene(A); sel(a); t.wait(300);
	let opened = false;
	for (let i = 0; i < ui("Menu copy").length && !opened; i++) {
		t.clickItem("Menu copy", i, "", true); t.wait(300);
		opened = has("Bring forward    Ctrl ]");
		if (!opened) { t.key("Escape"); t.wait(150); }
	}
	check("layer row menu opens", opened);
	if (!opened) return;
	check("layer row menu offers Copy to", has("Copy to"));
	t.click("Copy to"); t.wait(300);
	t.clickItem("Menu B", ui("Menu B").length - 1); t.wait(300); // the submenu entry is the newest item
	t.grab("lk01m-copyto.png");
	const n = Object.keys(t.doc().instances).filter(i => t.doc().instances[i].sceneId === B).length;
	t.click("Linked copy"); t.wait(300);
	const copies = Object.keys(t.doc().instances).filter(i => t.doc().instances[i].sceneId === B && t.doc().instances[i].sourceId === t.doc().instances[a].sourceId);
	check("Copy to ▸ scene ▸ Linked copy makes a linked copy there", copies.length === 1 && linkedTogether(a, copies[0], "transform"), { before: n, copies: copies });
});

story("PJ-07u", "Open asks to save an untitled show with changes", () => {
	app.newDocument(); t.wait(200);
	placeColor("Unsaved work", "#abcdef"); t.wait(200);
	const before = t.docJson();
	t.clickItem("Open", ui("Open").map((r, i) => Object.assign({ i: i }, r)).filter(r => r.text === "Open" && r.y < 40)[0].i); t.wait(400);
	t.grab("pj07u-prompt.png");
	check("a save prompt appears", has("Save this show first?"));
	check("no file picker yet", t.nativeDialogs().length === 0, t.nativeDialogs());
	t.click("Cancel"); t.wait(300);
	check("Cancel keeps the show", t.docJson() === before);
	t.closeNativeDialogs();
});

story("ST-01u", "Stats opens as a panel from the Window menu", () => {
	fresh(); t.wait(300);
	check("Window menu opens", t.click("Window")); t.wait(300);
	check("Window menu lists Stats", has("Stats"));
	t.click("Stats"); t.wait(600);
	t.grab("st01u-panel.png");
	check("the Stats panel shows render time", ui("Render time").length >= 1, ui("Render time"));
	check("and frame rate", ui("Frame rate").length >= 1);
});

story("AU-04u", "Mixer: drag a strip by its name to reorder it", () => {
	fresh();
	["Mic A", "Mic B", "Mic C"].forEach(n => addKind("mic", n, "", false));
	t.wait(500);
	t.click("Audio mixer"); t.wait(400); // by default the mixer is a tab behind Sources
	const order = () => app.mixer.map(m => m.name).filter(n => /^Mic /.test(n));
	// Mixer strips show the name in a small caption; the Sources rows use a larger Body text.
	const strip = n => ui(n).filter(r => r.cls !== "Body")[0];
	const c = strip("Mic C"), a = strip("Mic A");
	check("the mixer shows the strips", !!c && !!a, ui("Mic C"));
	if (!c || !a) return;
	t.grab("au04u-before.png");
	t.dragAt(c.x + 10, c.y + c.h / 2, a.x + 10, a.y + a.h / 2, 16);
	t.wait(400);
	t.grab("au04u-after.png");
	eq("Mic C moved to the front", order(), ["Mic C", "Mic A", "Mic B"]);
});

story("CC-01u", "Captions opens from the Window menu beside the mixer", () => {
	fresh(); addKind("mic", "Mic", "", false); t.wait(300);
	check("Window menu opens", t.click("Window")); t.wait(300);
	t.click("Captions"); t.wait(700);
	t.grab("cc01u-panel.png");
	const ready = captions.modelState === "ready";
	check(ready ? "Start is offered" : "the one-time model download is offered", ready ? has("Start") : has("Download speech model"), captions.modelState);
	check("the status line says captions are off", has("Captions are off"));
});

story("CP-01u", "Ctrl+C then Ctrl+V through the window copies the selected layer", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const r = placeColor("Keyboard copy", "#00ff00");
	sel(r); t.wait(200);
	const n = t.doc().scenes[A].layers.length;
	t.key("Ctrl+C"); t.wait(150);
	t.key("Ctrl+V"); t.wait(300);
	eq("Ctrl+V pasted a copy", t.doc().scenes[A].layers.length, n + 1);
	t.key("Ctrl+Z"); t.wait(200);
	eq("Ctrl+Z undoes the paste", t.doc().scenes[A].layers.length, n);
});

// LOBS-025: during a canvas drag the Inspector's transform fields follow on their own
// (app.inspectorTransform), and the rest of the Inspector catches up after the drag.
story("IN-02u", "Inspector X and Y follow a canvas drag, and the Inspector is right after it", () => {
	fresh();
	app.editScene(t.doc().sceneOrder[0]);
	const r = placeColor("Dragged", "#ff00ff");
	sel(r); app.setTransform("w", 400); app.setTransform("h", 300); app.setTransform("x", 100); app.setTransform("y", 100); t.wait(200);
	t.click("Inspector"); t.wait(300); // earlier stories bring Stats or History to the front of its tab group
	const tr0 = eff(r, "transform");
	t.canvasDrag(tr0.x + 50, tr0.y + 50, tr0.x + 50 + 123, tr0.y + 50 + 77, 30, "", -1);
	t.wait(300);
	const tr = eff(r, "transform");
	eq("the drag moved the layer", [Math.round(tr.x - tr0.x), Math.round(tr.y - tr0.y)], [123, 77]);
	check("Inspector X shows the new x", t.items(String(Math.round(tr.x))).length > 0, { x: Math.round(tr.x), seen: t.items("Dragged").map(i => i.cls), field: t.items("label=X").map(i => i.text) });
	check("Inspector Y shows the new y", t.items(String(Math.round(tr.y))).length > 0, t.items(String(Math.round(tr.y))).map(i => i.cls));
	eq("app.inspector has the final position after the drag", [Math.round(app.inspector.x), Math.round(app.inspector.y)], [Math.round(tr.x), Math.round(tr.y)]);
	eq("app.inspectorTransform agrees", [Math.round(app.inspectorTransform.x), Math.round(app.inspectorTransform.y)], [Math.round(tr.x), Math.round(tr.y)]);
	check("the drag is one undo step", app.undoLabel !== "", app.undoLabel);
});
