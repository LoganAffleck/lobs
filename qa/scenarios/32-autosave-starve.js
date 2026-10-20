// PJ-02b: does continuous editing starve the 400 ms debounced autosave? (heavy project)
t.include("_lib.js");
skipSetup();
t.setTimeout(300);
story("PJ-02b", "Autosave keeps up while you keep editing", () => {
	app.open(t.url(t.scriptDir() + "/../runs/fixtures/heavy/heavy.lobs.json")); t.waitFrames(2, 5000); t.wait(1500);
	const docPath = t.tempDir() + "/lobs-selftest.lobs.json";
	const sid = t.doc().sceneOrder[1];
	app.editScene(sid);
	const id = app.layers[0].id; app.selectLayer(id, false);
	const mtime = () => t.exists(docPath) ? t.fileSize(docPath) + ":" + t.readFile(docPath).length : "";
	// 20 s of steady nudging, one edit every ~250 ms (like holding an arrow key or scrubbing).
	let saves = 0, last = mtime(), lastSaveAt = t.now(), longest = 0, edits = 0;
	const marker = k => "Starve " + k;
	const end = t.now() + 20000;
	let lastSavedEdit = -1;
	while (t.now() < end) {
		app.renameScene(sid, marker(edits)); ++edits;
		t.wait(250);
		const txt = t.exists(docPath) ? t.readFile(docPath) : "";
		const m = /"name": "Starve (\d+)"/.exec(txt);
		const savedEdit = m ? +m[1] : -1;
		if (savedEdit !== lastSavedEdit) { ++saves; longest = Math.max(longest, t.now() - lastSaveAt); lastSaveAt = t.now(); lastSavedEdit = savedEdit; }
	}
	longest = Math.max(longest, t.now() - lastSaveAt);
	t.metric("result", { edits: edits, autosaves: saves, longestGapMs: Math.round(longest), lastSavedEdit: lastSavedEdit });
	check("autosave lands at least every 5 s during continuous editing", longest <= 5000,
	      "longest stretch with no autosave: " + Math.round(longest) + " ms over " + edits + " edits (" + saves + " saves)");
});
