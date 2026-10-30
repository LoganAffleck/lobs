// Probe: inline layer rename, collapsible groups, the pencil leaving its strokes unselected,
// and an audio source's mixer settings in the Inspector.
t.include("_lib.js");
function ui(text) { return t.items(text); }
function has(text) { return ui(text).length > 0; }
function layerRowIndex(name) { return ui(name).findIndex(i => i.text === name && i.cls === "Body"); } // the Layers row's name

story("LA-01", "Rename a layer in its row: double-click, F2, Enter keeps, Esc cancels", () => {
	skipSetup();
	fresh();
	const a = placeColor("Box", "#3366ff");
	sel(a); t.wait(300);
	const ri = layerRowIndex("Box");
	check("layer row shows", ri >= 0, JSON.stringify(ui("Box")));
	t.clickItem("Box", ri, "", false, true); t.wait(300);
	t.grab("la01-editing.png");
	check("no rename dialog", !has("Rename") || !ui("Rename").some(i => /Dialog|Text/.test(i.cls) && i.y > 400 && i.y < 700));
	t.key("Ctrl+A"); t.typeText("Webcam frame"); t.key("Return"); t.wait(300);
	eq("Enter renames", t.doc().instances[a].name, "Webcam frame");
	t.grab("la01-renamed.png");
	t.key("F2"); t.wait(300);
	t.key("Ctrl+A"); t.typeText("Nope"); t.key("Escape"); t.wait(300);
	eq("Esc keeps the old name", t.doc().instances[a].name, "Webcam frame");
});

story("LA-02", "Groups collapse and expand in Layers", () => {
	fresh();
	const a = placeColor("One", "#ff0000"), b = placeColor("Two", "#00ff00");
	app.selectLayer(a, false); app.selectLayer(b, true); app.groupSelected(); t.wait(300);
	const g = app.inspector.id;
	const rows = () => app.layers.map(r => r.name);
	check("group open shows its layers", rows().includes("One") && rows().includes("Two"), JSON.stringify(rows()));
	app.clearSelection();
	app.setGroupCollapsed(g, true); t.wait(300);
	check("collapsed hides them", !rows().includes("One") && !rows().includes("Two"), JSON.stringify(rows()));
	t.grab("la02-collapsed.png");
	app.selectLayer(a, false); t.wait(300);
	check("selecting a layer inside opens it (so the selection shows)", rows().includes("One"));
	app.setGroupCollapsed(g, true); t.wait(300);
	eq("collapsing with a layer inside selected selects the group", app.inspector.id, g);
	app.setGroupCollapsed(g, false); t.wait(300);
	check("expanded again", rows().includes("One") && rows().includes("Two"));
});

story("LA-03", "Pencil strokes aren't selected, and the pencil keeps drawing", () => {
	fresh();
	app.clearSelection();
	app.tool = "pencil"; t.wait(200);
	const n = Object.keys(t.doc().instances).length;
	t.canvasDrag(400, 400, 700, 600, 12); t.wait(300);
	eq("a stroke was drawn, in a group", Object.keys(t.doc().instances).length, n + 2);
	eq("nothing is selected", app.inspector.mode, "none");
	eq("still the pencil", app.tool, "pencil");
	t.canvasDrag(800, 300, 900, 500, 12); t.wait(300);
	eq("a second stroke, in the same group", Object.keys(t.doc().instances).length, n + 3);
	app.tool = "select";
});

story("LA-03b", "One pencil session is one group; picking the pencil again starts another", () => {
	fresh();
	const A = app.editSceneId;
	const groups = () => { const d = t.doc(); return d.scenes[A].layers.filter(id => d.instances[id].kind === "group").map(id => d.instances[id]); };
	app.tool = "pencil"; t.wait(200);
	t.canvasDrag(400, 400, 700, 600, 12); t.canvasDrag(800, 300, 900, 500, 12); t.wait(300);
	let g = groups();
	eq("one group", g.length, 1);
	eq("with both strokes", g[0] && g[0].children.length, 2);
	check("named Drawing", g[0] && g[0].name.indexOf("Drawing") === 0, g[0] && g[0].name);
	app.tool = "rect"; app.tool = "pencil"; t.wait(200);
	t.canvasDrag(300, 700, 500, 800, 12); t.wait(300);
	g = groups();
	eq("a second group", g.length, 2);
	app.tool = "select";
	// Strokes in a group show in the vertical layout too.
	app.setVerticalOn(true);
	const d = t.doc(), v = Object.keys(d.scenes).find(id => d.scenes[id].verticalOf === A);
	const vg = d.scenes[v].layers.map(id => d.instances[id]).filter(i => i.kind === "group");
	eq("vertical has both groups", vg.length, 2);
	check("each following its group, with its strokes", vg.every(g => g.follows && d.instances[g.follows].kind === "group" && g.children.length === d.instances[g.follows].children.length), vg.map(g => g.children.length));
	// Hiding a group on desktop hides it in vertical.
	app.toggleVisible(vg[0].follows);
	eq("group visibility follows", eff(vg[0].id, "visibility").visible, false);
	app.setVerticalOn(false);
});

story("LA-04", "An audio source's mixer settings show in the Inspector", () => {
	fresh();
	const k = addKind("mic", "Desk mic", "", false);
	t.wait(500);
	t.click("Audio mixer"); t.wait(400);
	t.grab("la04-mixer.png");
	const more = ui("tip=Audio settings");
	check("mixer has the ... button", more.length > 0);
	t.clickItem("tip=Audio settings", 0); t.wait(500);
	eq("the source is selected", app.inspector.mode, "source");
	eq("it's the mic", app.inspector.id, k.source);
	t.click("Inspector"); t.wait(300);
	check("Inspector shows its audio settings", has("Audio") && has("Monitor off") && has("Effects"));
	check("and its tracks", has("Tracks"));
	t.grab("la04-inspector.png");
	check("no popover over the mixer", ui("Applies to this source in every scene.").length <= 1);
});

story("LA-05", "Shift-click in Layers selects everything between; Ctrl-click toggles one", () => {
	fresh();
	const ids = ["L1", "L2", "L3", "L4", "L5"].map(n => placeColor(n, "#3366ff"));
	t.wait(300);
	const row = n => ui(n).findIndex(i => i.text === n && i.cls === "Body");
	// Newest on top: L5, L4, L3, L2, L1.
	t.clickItem("L5", row("L5")); t.wait(200);
	t.clickItem("L2", row("L2"), "shift"); t.wait(300);
	const sel = app.layers.filter(r => r.selected).map(r => r.name);
	eq("Shift selects L5 down to L2", JSON.stringify(sel), JSON.stringify(["L5", "L4", "L3", "L2"]));
	check("not the background", !sel.includes("Background"));
	t.clickItem("L3", row("L3"), "ctrl"); t.wait(200);
	eq("Ctrl takes one out", JSON.stringify(app.layers.filter(r => r.selected).map(r => r.name)), JSON.stringify(["L5", "L4", "L2"]));
	t.clickItem("L1", row("L1")); t.wait(200);
	t.clickItem("L4", row("L4"), "shift"); t.wait(300);
	eq("Shift upward works too", JSON.stringify(app.layers.filter(r => r.selected).map(r => r.name)), JSON.stringify(["L4", "L3", "L2", "L1"]));
	t.grab("la05-range.png");
});
