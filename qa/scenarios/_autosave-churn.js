// Helper for tools/crash_autosave.ps1 (not a standalone story): opens the stress project, makes an
// edit every ~1.5 s (so the 400 ms autosave fires between edits), and writes "armed" once the
// first autosave has landed. The kill window starts at "armed".
t.include("_lib.js");
skipSetup();
t.setTimeout(600);
const docPath = t.tempDir() + "/lobs-selftest.lobs.json";
app.open(t.url(t.scriptDir() + "/../runs/fixtures/stress/stress.lobs.json"));
const sid = t.doc().sceneOrder[0];
let saves = 0, lastSize = -1;
for (let k = 0; ; ++k) {
	app.renameScene(sid, "Churn " + k);
	const end = t.now() + 1500;
	while (t.now() < end) {
		t.wait(50);
		const sz = t.exists(docPath) ? t.fileSize(docPath) : -1;
		if (sz > 0 && sz !== lastSize) { ++saves; lastSize = sz; }
		if (saves === 1) t.writeFile(t.out("armed"), String(t.now()));
	}
	t.writeFile(t.out("progress.txt"), JSON.stringify({ edits: k + 1, saves: saves, size: lastSize }));
}
