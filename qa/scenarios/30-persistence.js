// Projects and persistence: PJ-01, PJ-03 (partial), autosave contents.
t.include("_lib.js");

const projRoot = t.out("projects");

story("PJ-01", "New project, save, switch, reopen", () => {
	fresh();
	eq("valid name accepted", app.checkNewProject("My Show", projRoot), "");
	const badNames = ["", "   ", "a/b", "a:b", "what?", "dots.", "CON", "NUL", "COM1", "x".repeat(250)];
	const verdicts = badNames.map(n => [n.length > 20 ? n.slice(0, 8) + "…(" + n.length + ")" : n, app.checkNewProject(n, projRoot)]);
	t.metric("badNameVerdicts", verdicts);
	const accepted = verdicts.filter(v => v[1] === "").map(v => v[0]);
	check("invalid Windows names rejected up front", accepted.length === 0, "accepted: " + JSON.stringify(accepted));
	["CON", "x".repeat(250)].forEach(n => {
		if (app.checkNewProject(n, projRoot) === "") {
			const ok = app.createProject(n, projRoot);
			t.note("createProject('" + n.slice(0, 10) + "') returned " + ok + ", toast: '" + app.toast + "'");
		}
	});
	check("create project A", app.createProject("Show A", projRoot));
	const fileA = app.projectFile;
	check("A has folder + assets", t.exists(fileA) && t.exists(projRoot + "/Show A/assets"), fileA);
	const A0 = t.doc().sceneOrder[0];
	app.editScene(A0);
	placeColor("Saved layer", "#123456");
	check("save A", app.saveProject());
	placeColor("Unsaved layer", "#654321"); // edited after the last save
	t.wait(600);
	const unsavedJson = t.docJson();
	// Switch to project B with Open (the menu's path), then come back to A.
	check("create project B", app.createProject("Show B", projRoot));
	const fileB = app.projectFile;
	const docA = JSON.parse(t.readFile(fileA));
	const names = Object.keys(docA.instances).map(i => docA.instances[i].name);
	check("creating B saved A's latest edits first", names.indexOf("Unsaved layer") >= 0, names);
	app.editScene(t.doc().sceneOrder[0]);
	placeColor("B edit", "#00ff00");
	t.wait(600);
	check("open A from B", app.open(t.url(fileA)));
	const docB = JSON.parse(t.readFile(fileB));
	const bNames = Object.keys(docB.instances).map(i => docB.instances[i].name);
	check("opening A doesn't lose B's unsaved edit", bNames.indexOf("B edit") >= 0,
	      "B on disk has " + JSON.stringify(bNames) + " — the edit made in B after creating it is gone from B's file");
	check("undo can bring B back", (() => { app.undo(); return Object.keys(t.doc().instances).some(i => t.doc().instances[i].name === "B edit"); })(), app.undoLabel);
});

story("PJ-01b", "Save As to a folder that can't be written", () => {
	fresh();
	const good = t.out("pj01b-good.lobs.json");
	check("save as good path", app.saveAs(t.url(good)));
	const bad = "Z:/definitely/not/here/show.lobs.json";
	const ok = app.saveAs(t.url(bad));
	check("save as to a bad path fails", !ok, ok);
	t.note("after failed Save As: projectFile='" + app.projectFile + "', toast='" + app.toast + "'");
	check("project file still points at the last good save", app.projectFile.replace(/\\/g, "/") === good.replace(/\\/g, "/"),
	      "projectFile became '" + app.projectFile + "' after a failed Save As, so the next Save writes nowhere");
	check("next Save still works", app.saveProject(), "saveProject returned false (toast: '" + app.toast + "')");
});

story("PJ-01d", "A New Project that can't be created leaves the open show alone (LOBS-008)", () => {
	fresh();
	const good = t.out("pj01d.lobs.json");
	check("save the current show", app.saveAs(t.url(good)));
	placeColor("Still here", "#336699");
	const before = t.docJson(), fileBefore = app.projectFile;
	// Passes the name check, but the folder can't be made.
	eq("name and folder look fine", app.checkNewProject("Unreachable", "Z:/no/such/drive"), "");
	const ok = app.createProject("Unreachable", "Z:/no/such/drive");
	check("createProject reports failure", !ok, app.toast);
	check("the open show is unchanged", t.docJson() === before);
	eq("the project file is unchanged", app.projectFile, fileBefore);
	check("the show it left was saved first", t.readFile(good).indexOf("Still here") >= 0);
});

story("PJ-01c", "Open rejects junk without touching the current show", () => {
	fresh();
	placeColor("Keep me", "#abcdef");
	const keep = t.docJson();
	const cases = {
		"empty.lobs.json": "",
		"garbage.lobs.json": "{not json",
		"array.lobs.json": "[1,2,3]",
		"wrongver.lobs.json": JSON.stringify({ version: 99, scenes: {} }),
		"missing.lobs.json": null,
		"halfwritten.lobs.json": JSON.stringify(JSON.parse(t.docJson())).slice(0, 300),
	};
	const out = [];
	Object.keys(cases).forEach(name => {
		const f = t.out("junk/" + name);
		if (cases[name] !== null) t.writeFile(f, cases[name]);
		const ok = app.open(t.url(f));
		out.push([name, ok, app.toast]);
		check(name + ": refused", !ok);
		check(name + ": current show untouched", t.docJson() === keep);
	});
	t.metric("openResults", out);
	// A v1 doc with missing pieces (scene references an unknown instance).
	const d = JSON.parse(t.docJson());
	const sid = d.sceneOrder[0];
	d.scenes[sid].layers.push("ins_ghost");
	const f = t.out("junk/dangling.lobs.json");
	t.writeFile(f, JSON.stringify(d));
	const ok = app.open(t.url(f));
	t.note("dangling-reference doc opened: " + ok);
	if (ok) {
		t.wait(500);
		const L = app.layers;
		check("dangling layer reference doesn't crash or show a ghost row", L.every(l => l.id !== "ins_ghost" || l.name), L.map(l => l.id + ":" + l.name));
		check("the dangling id is dropped from the scene on load (LOBS-016)", t.doc().scenes[sid].layers.indexOf("ins_ghost") < 0, t.doc().scenes[sid].layers);
		check("Open reports the repair", /Repaired/.test(app.toast), app.toast);
	}
});

story("PJ-02a", "Autosave writes the current show", () => {
	fresh();
	placeColor("Autosaved", "#ff8800");
	const docPath = t.tempDir() + "/lobs-selftest.lobs.json";
	check("autosave fires within 1 s", t.waitUntil(() => t.exists(docPath) && t.readFile(docPath).indexOf("Autosaved") >= 0, 1500), docPath);
	let parsed = null;
	try { parsed = JSON.parse(t.readFile(docPath)); } catch (e) {}
	check("autosave is valid JSON", !!parsed);
});

story("PJ-01e", "New show, then Open, never overwrites the project you left (LOBS-007)", () => {
	fresh();
	const a = t.out("pj01e-a.lobs.json"), b = t.out("pj01e-b.lobs.json");
	placeColor("Only in A", "#112233");
	check("save A", app.saveAs(t.url(a)));
	const aBefore = t.readFile(a);
	app.newDocument();
	eq("a new show has no project file", app.projectFile, "");
	check("save the new show as B", app.saveAs(t.url(b)));
	app.newDocument();
	check("open A", app.open(t.url(a)));
	eq("A's file is unchanged", t.readFile(a), aBefore);
	check("A still has its layer", Object.keys(t.doc().instances).some(i => t.doc().instances[i].name === "Only in A"));
});

story("PJ-04", "Startup mode: only exact self-test flags use a throwaway show (LOBS-001)", () => {
	const normal = [
		["C:/Users/testuser/AppData/Local/Programs/LOBS/LOBS.exe"],
		["D:/Tests/latest-build/LOBS.exe"],
		["C:/Users/contestant/Downloads/LOBS/LOBS.exe"],
		["C:/LOBS/LOBS.exe", "C:/Users/me/shows/test.lobs.json"],
		["C:/LOBS/LOBS.exe", "--use-engine", "32.2.2"],
	];
	normal.forEach(a => check("normal launch: " + a.join(" "), !t.isSelfTestArgs(a)));
	const tests = [
		["C:/LOBS/LOBS.exe", "--autotest", "out.json"],
		["C:/LOBS/LOBS.exe", "--scenariotest", "s.js", "out"],
		["C:/LOBS/LOBS.exe", "--snapshot", "p.png"],
	];
	tests.forEach(a => check("self-test launch: " + a[1], t.isSelfTestArgs(a)));
});

story("PJ-05", "Self-test recordings never go to the user's Videos folder (LOBS-003)", () => {
	app.saveSettings({ recordFolder: "" }); // no folder chosen: the default applies
	const f = String(app.settings.recordFolder || "");
	t.note("default record folder in a self-test run: " + f);
	check("the default record folder is the temp folder, not Videos", /lobs-selftest-recordings/.test(f) && !/Videos/i.test(f), f);
});

story("PJ-06", "Open is refused while live, so what's on air never changes underneath you", () => {
	fresh();
	const f = t.out("pj06.lobs.json");
	app.saveAs(t.url(f));
	placeColor("On air", "#00ff00");
	const before = t.docJson();
	t.startRecording();
	t.waitUntil(() => app.live, 10000);
	const ok = app.open(t.url(f));
	check("Open returns false while live", ok === false, ok);
	check("the show on air is untouched", t.docJson() === before);
	check("the user is told why", /Stop streaming and recording/.test(app.toast), app.toast);
	t.stopRecording();
	t.waitUntil(() => !app.live, 10000);
});

story("PJ-07", "An untitled show reports unsaved changes; a fresh one doesn't", () => {
	app.newDocument();
	check("a brand-new untitled show has nothing to lose", app.unsavedShow === false);
	placeColor("Work", "#123456");
	check("an edited untitled show has unsaved changes", app.unsavedShow === true);
	app.saveAs(t.url(t.out("pj07.lobs.json")));
	check("a saved project isn't untitled", app.unsavedShow === false);
});
