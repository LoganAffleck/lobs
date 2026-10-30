// Probe: a scene placed inside another (nested scenes), scene rename in grid view, group icons,
// and dragging an asset into a folder.
t.include("_lib.js");
function ui(text) { return t.items(text); }
function has(text) { return ui(text).length > 0; }
function red(p) { return p.r > 180 && p.g < 80 && p.b < 80; }
const img = t.scriptDir() + "/../runs/fixtures/halves.png";

story("NS-01", "Show a scene inside another: it renders, it can't contain itself, deleting it removes it", () => {
	skipSetup();
	fresh();
	const A = t.doc().sceneOrder[0];
	app.renameScene(A, "Main");
	const B = mkScene("Inner");
	app.editScene(B);
	const r = placeColor("Red box", "#ff0000");
	sel(r); app.setTransform("x", 0); app.setTransform("y", 0); app.setTransform("w", 400); app.setTransform("h", 300);
	app.addFilter("color"); t.wait(200); // a filter, so parking (LOBS-034) has something to take away
	app.editScene(A); t.wait(300);
	const before = t.doc().sourceOrder.length;
	app.addSceneSource(B); t.wait(800);
	const ss = t.doc().sourceOrder.filter(id => t.doc().sources[id].kind === "scene");
	eq("one scene source", ss.length, 1);
	check("placed in Main", t.doc().scenes[A].layers.some(l => t.doc().instances[l].sourceId === ss[0]));
	app.take(); t.wait(1500); // Main on program; Inner isn't edited or shown on its own
	app.editScene(A); t.wait(1500);
	const p = t.snapshot("", "ns01-program.png", 0);
	check("Inner's red box shows inside Main on program", red(t.pixel(p, 100, 100)), JSON.stringify(t.pixel(p, 100, 100)));
	// Itself, directly and through another scene.
	app.editScene(B);
	app.addSceneSource(A); t.wait(300);
	check("Main can't go inside Inner (Inner is inside Main)", !t.doc().scenes[B].layers.some(l => t.doc().sources[t.doc().instances[l].sourceId].kind === "scene"));
	check("and LOBS says why", /itself/.test(app.toast), app.toast);
	app.editScene(A);
	app.addSceneSource(A); t.wait(300);
	eq("a scene can't go inside itself", t.doc().sourceOrder.filter(id => t.doc().sources[id].kind === "scene").length, 1);
	app.editScene(B); app.deleteScene(B); t.wait(500);
	eq("deleting the scene removes its source", t.doc().sourceOrder.filter(id => t.doc().sources[id].kind === "scene").length, 0);
	integrity();
});

story("NS-02", "Rename a scene from the grid view; groups show a folder", () => {
	fresh();
	app.renameScene(t.doc().sceneOrder[0], "Opening");
	t.click("tip=Grid with live thumbnails"); t.wait(500);
	const tile = ui("Opening").filter(i => i.text === "Opening");
	check("tile shows", tile.length > 0);
	t.note(JSON.stringify(ui("Opening")));
	t.clickItem("Opening", ui("Opening").findIndex(i => i.text === "Opening" && i.cls === "Body"), "", true); t.wait(400); // the tile, not the canvas header
	check("right-click menu on the tile", has("Rename"));
	t.click("Rename"); t.wait(300);
	t.key("Ctrl+A"); t.typeText("Starting soon"); t.key("Return"); t.wait(300);
	eq("renamed in place", t.doc().scenes[t.doc().sceneOrder[0]].name, "Starting soon");
	t.grab("ns02-grid.png");
	t.click("tip=List"); t.wait(300);
	const a = placeColor("One", "#ff0000"), b = placeColor("Two", "#00ff00");
	app.selectLayer(a, false); app.selectLayer(b, true); app.groupSelected(); t.wait(300);
	const g = app.layers.find(r => r.group);
	eq("an open group shows an open folder", g.icon, "folder-open");
	app.setGroupCollapsed(g.id, true); t.wait(200);
	eq("a closed one a folder", app.layers.find(r => r.group).icon, "folder");
});

story("AD-01", "Drag an asset onto a folder: it moves there, the list doesn't scroll", () => {
	fresh();
	app.saveAs(t.url(t.out("ad01.lobs.json"))); t.wait(300);
	app.importAssets([t.url(img)]); t.wait(800);
	const folder = app.createAssetFolder(); t.wait(300);
	app.renameAsset(folder, "Overlays"); t.wait(500);
	t.click("Assets"); t.wait(300);
	const file = ui("halves").filter(i => /halves/.test(i.text))[0], dest = ui("Overlays").filter(i => i.text === "Overlays")[0];
	check("file and folder show", !!file && !!dest, JSON.stringify([file, dest]));
	if (!file || !dest) return;
	t.grab("ad01-before.png");
	t.dragAt(file.x + file.w / 2, file.y + file.h / 2, dest.x + dest.w / 2, dest.y + dest.h / 2, 16); t.wait(800);
	t.grab("ad01-after.png");
	const moved = !app.assets.some(a => /halves/.test(a.name));
	check("the file moved out of this folder", moved, JSON.stringify(app.assets.map(a => a.name)));
	app.openAssetFolder(app.assets.find(a => a.name === "Overlays").path); t.wait(400);
	check("into Overlays", app.assets.some(a => /halves/.test(a.name)), JSON.stringify(app.assets.map(a => a.name)));
	// And back up, onto "Assets" in the path above.
	const f2 = ui("halves").filter(i => /halves/.test(i.text))[0], crumb = ui("Assets").filter(i => i.text === "Assets" && i.y < 900).pop();
	if (f2 && crumb) {
		t.dragAt(f2.x + f2.w / 2, f2.y + f2.h / 2, crumb.x + crumb.w / 2, crumb.y + crumb.h / 2, 16); t.wait(800);
		check("dragged onto the path above, it moves back up", !app.assets.some(a => /halves/.test(a.name)), JSON.stringify(app.assets.map(a => a.name)));
	} else check("crumb and file found", false, JSON.stringify([f2, crumb]));
});
