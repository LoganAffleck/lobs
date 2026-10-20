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
		t.clickItem("Save", tb[0].i); t.wait(1200);
		const dlg2 = t.nativeDialogs();
		t.note("toolbar Save: native dialogs " + JSON.stringify(dlg2) + ", file updated: " + (t.readFile(f).indexOf("After first save") >= 0));
		t.closeNativeDialogs(); t.wait(500);
	}
});
