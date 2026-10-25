// Probe: linking a second facecam layer's crop to a cropped, rounded first one through the
// Inspector's link menu left the menu open over a frozen canvas (LOBS looked stuck). Fails
// before the fix in FSection.qml (the canvas stays frozen), passes after.
t.include("_lib.js");
story("LF-02", "Linking crop through the Inspector's link menu closes the menu and gives the canvas back", () => {
	skipSetup();
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const k = addKind("camera", "Facecam");
	const a = k.inst;
	t.wait(1500);
	sel(a);
	app.cropToAspect(1); app.setCornerRadius(50, true); t.wait(300);
	const B = mkScene("Second");
	app.editScene(B); t.wait(300);
	app.placeSource(k.source); t.wait(800);
	const b = app.inspector.id;
	t.click("Inspector"); t.wait(300);
	check("the crop section's link button shows", t.items("tip=Link crop & corners with this source in another scene").length > 0);
	t.clickItem("tip=Link crop & corners with this source in another scene", 0); t.wait(400);
	check("link menu open", t.items("Link with Scene").length > 0);
	t.click("Link with Scene"); t.wait(400);
	t.click("Use Scene's values"); t.wait(600);
	t.grab("lf02-after.png");
	check("linked", linkedTogether(a, b, "crop"));
	check("the link menu closed", t.items("Link with Scene").length === 0 && t.items("Use Scene's values").length === 0);
	t.wait(500);
	check("the canvas is live again (no frozen frame)", app.previewHidden === false);
});
