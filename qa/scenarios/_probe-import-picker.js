// Probe: Import > Choose a file... opens a file picker, on a fresh show and on one with unsaved changes.
t.include("_lib.js");
function ui(text) { return t.items(text); }
function has(text) { return ui(text).length > 0; }

function tryIt(label) {
	t.click("Import"); t.wait(400);
	check(label + ": Import menu open", has("Choose a file"));
	t.click("Choose a file…"); t.wait(1500);
	const native = t.nativeDialogs();
	const askSave = has("Save this show first?");
	t.note(label + ": previewHidden=" + app.previewHidden + " visibleMenus=" + app.visibleMenus);
	if (askSave) { t.wait(3000); check(label + ": the preview stays aside while the prompt is up (past the 2 s safety net)", app.previewHidden === true); }
	t.note(label + ": native dialogs " + JSON.stringify(native) + ", save prompt " + askSave + ", windows " + t.windowCount());
	t.grab("import-" + label + ".png");
	check(label + ": a file picker or the save prompt shows", native.length > 0 || askSave);
	t.closeNativeDialogs(); t.wait(300);
	if (askSave)
		// The native preview window draws over QML: unless it steps aside, the prompt is hidden
		// behind it and only the dimmed backdrop shows (window grabs don't show the preview).
		check(label + ": the video preview steps aside for the save prompt", app.previewHidden === true);
	if (askSave) {
		t.click("Don't save"); t.wait(1500);
		const native2 = t.nativeDialogs();
		t.note(label + ": after Don't save, native dialogs " + JSON.stringify(native2));
		t.grab("import-" + label + "-after.png");
		check(label + ": after Don't save, the file picker shows", native2.length > 0);
		t.closeNativeDialogs(); t.wait(300);
	}
	t.key("Escape"); t.wait(300);
}

story("IMP-picker", "Import > Choose a file opens a file picker", () => {
	skipSetup();
	fresh();
	tryIt("fresh");
	placeColor("Changed", "#ff0000");
	// The same prompt without a menu: Ctrl+O.
	t.key("Ctrl+O"); t.wait(1000);
	check("Ctrl+O: the save prompt shows with the preview aside", has("Save this show first?") && app.previewHidden === true);
	t.click("Cancel"); t.wait(500);
	check("Ctrl+O: Cancel brings the preview back", app.previewHidden === false);
	tryIt("unsaved");
});
