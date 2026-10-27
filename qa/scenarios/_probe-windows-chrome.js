// Probe (not in the default run): after the macOS menu bar work, Windows keeps its own top bar,
// in-window shortcuts, "Ctrl" key labels and Windows fonts.
t.include("_lib.js");

function ui(text) { return t.items(text); }
function has(text) { return ui(text).length > 0; }
function topButton(label) { return ui(label).filter(i => i.text === label && i.y < 60); }

story("WC-01", "Top bar: New, Open, Import, Save and Window are there; Window opens and closes panels", () => {
	skipSetup();
	fresh();
	for (const label of ["New", "Open", "Import", "Save", "Window"])
		check("top bar has " + label, topButton(label).length > 0, JSON.stringify(ui(label).map(i => i.cls + "@" + Math.round(i.y))));
	t.grab("wc01-topbar.png");

	// Window menu: Stats isn't open by default. Open it from the menu, then close it again.
	const statsTabs = () => ui("Stats").filter(i => i.text === "Stats" && i.y > 60).length;
	const before = statsTabs();
	t.click("Window"); t.wait(300);
	check("Window menu lists panels", has("Audio mixer") && has("Stats"));
	t.grab("wc01-window-menu.png");
	const items = ui("Stats").filter(i => i.text === "Stats");
	t.clickItem("Stats", items.length - 1); t.wait(500);
	const opened = statsTabs();
	check("Window > Stats opens the Stats panel", opened > before, before + " -> " + opened);
	t.click("Window"); t.wait(300);
	const items2 = ui("Stats").filter(i => i.text === "Stats");
	t.clickItem("Stats", items2.length - 1); t.wait(500);
	eq("Window > Stats again closes it", statsTabs(), before);
	t.key("Escape");
});

story("WC-02", "Shortcuts: Ctrl+C/Ctrl+V in a text field stay in the field; on layers they copy layers; Delete removes", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const r = placeColor("R", "#ff0000");
	sel(r);
	// Layers: Ctrl+C then Ctrl+V pastes a copy; Delete removes the selection.
	let n = Object.keys(t.doc().instances).length;
	t.key("Ctrl+C"); t.wait(100); t.key("Ctrl+V"); t.wait(200);
	eq("Ctrl+C, Ctrl+V on a layer pastes a copy", Object.keys(t.doc().instances).length, n + 1);
	n = Object.keys(t.doc().instances).length;
	t.key("Delete"); t.wait(200);
	eq("Delete removes the selected layer", Object.keys(t.doc().instances).length, n - 1);
	// A text field: copy and paste go to the text, not the layers.
	sel(r);
	t.clickItem("tip=Add a source", 0); t.wait(400);
	t.click("Name (optional)"); t.wait(100);
	t.typeText("hello"); t.wait(50);
	t.key("Ctrl+A"); t.key("Ctrl+C"); t.wait(50);
	t.key("End"); t.key("Ctrl+V"); t.wait(100);
	n = Object.keys(t.doc().instances).length;
	check("Ctrl+V in a text field pastes the text", has("hellohello"), JSON.stringify(ui("hello").map(i => i.text)));
	t.key("Ctrl+V"); t.wait(100);
	eq("...and doesn't paste layers", Object.keys(t.doc().instances).length, n);
	t.grab("wc02-textfield.png");
	t.key("Escape"); t.wait(200);
});

story("WC-03", "Labels and fonts: tooltips say Ctrl; text layers default to Segoe UI", () => {
	fresh();
	check("Undo tooltip reads 'Ctrl Z'", has("tip=Ctrl Z"));
	check("Redo tooltip reads 'Ctrl Shift Z'", has("tip=Redo  Ctrl Shift Z"));
	check("Save tooltip reads 'Save (Ctrl+S)'", has("tip=Save (Ctrl+S)"));
	check("no Mac key symbols in the UI", !has("⌘") && !has("tip=⌘") && !has("⇧"));
	eq("new text layers default to Segoe UI", app.defaultTextFont, "Segoe UI");
	eq("the text tool starts with Segoe UI", app.toolFont, "Segoe UI");
	// A text layer's Inspector shows its font.
	const k = addKind("text", "Hello");
	t.wait(400);
	t.click("Inspector"); t.wait(300); // History may be the tab in front
	check("a new text layer's font picker shows Segoe UI", has("Segoe UI"));
	t.grab("wc03-text-inspector.png");
});
