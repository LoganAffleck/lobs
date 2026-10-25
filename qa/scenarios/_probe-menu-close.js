// Temporary: does the layer row menu close after Copy to > scene > Linked copy?
t.include("_lib.js");
function ui(text) { return t.items(text); }
function has(text) { return ui(text).length > 0; }
story("MC", "Layer menu closes after a submenu choice", () => {
	skipSetup();
	fresh();
	const A = t.doc().sceneOrder[0];
	const a = placeColor("Menu copy", "#00ffff");
	mkScene("Menu B");
	app.editScene(A); sel(a); t.wait(300);
	let opened = false;
	for (let i = 0; i < ui("Menu copy").length && !opened; i++) {
		t.clickItem("Menu copy", i, "", true); t.wait(300);
		opened = has("Bring forward    Ctrl ]");
		if (!opened) { t.key("Escape"); t.wait(150); }
	}
	t.click("Copy to"); t.wait(300);
	t.clickItem("Menu B", ui("Menu B").length - 1); t.wait(300);
	t.click("Linked copy"); t.wait(300);
	t.grab("after-copy.png");
	t.note("after 0.3s: copies in B " + Object.keys(t.doc().instances).filter(i => t.doc().instances[i].sceneId !== A).length + " menu " + has("Bring forward    Ctrl ]") + " hidden=" + app.previewHidden);
	t.wait(3000);
	t.note("after 3.3s: menu " + has("Bring forward    Ctrl ]") + " hidden=" + app.previewHidden);
	check("menu closed", !has("Bring forward    Ctrl ]"));
});
