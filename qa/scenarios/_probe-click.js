// Probe: what clicking a scene does (P3-2b).
t.include("_lib.js");
skipSetup();

story("SW-01", "Click shows puts the scene live; Click edits only opens it", () => {
	fresh();
	app.addScene();
	const [A, B] = t.doc().sceneOrder;
	const was = app.clickToShow;
	app.editScene(A); app.take();
	app.clickToShow = false;
	app.pickScene(B);
	eq("Click edits: opened", app.editSceneId, B);
	eq("Click edits: not live", t.programScene(), A);
	app.clickToShow = true;
	app.pickScene(B);
	eq("Click shows: live", t.programScene(), B);
	app.clickToShow = was;
});
