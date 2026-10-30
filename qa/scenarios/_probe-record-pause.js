// Probe: pausing a recording, and the MP4 copy of an MKV (P2-5).
t.include("_lib.js");
skipSetup();

story("RC-01", "Pause leaves time out of the recording; an MKV gets an MP4 copy when it stops", () => {
	const dir = t.out("rec");
	app.saveSettings({ recordFormat: "mkv", remux: true, recordQuality: "high", recordFolder: dir });
	const before = t.listDir(dir).length;
	app.toggleRecording();
	t.wait(2500);
	check("recording", app.recording);
	check("can pause", app.canPauseRecording);
	app.togglePauseRecording();
	check("paused", app.recordingPaused);
	const atPause = app.liveStats.recTime;
	t.wait(2500);
	eq("the timer stands still while paused", app.liveStats.recTime, atPause);
	app.togglePauseRecording();
	check("resumed", !app.recordingPaused);
	t.wait(1500);
	app.toggleRecording();
	let waited = 0, files = [];
	while (waited < 20000) {
		files = t.listDir(dir);
		if (files.some(f => /\.mp4$/i.test(f)) && files.some(f => /\.mkv$/i.test(f))) break;
		t.wait(250); waited += 250;
	}
	t.metric("files", files);
	check("the MKV is kept", files.some(f => /\.mkv$/i.test(f)), files);
	check("an MP4 copy appears", files.some(f => /\.mp4$/i.test(f)), files);
	const mp4 = files.find(f => /\.mp4$/i.test(f));
	if (mp4) check("the MP4 isn't empty", t.fileSize(dir + "/" + mp4) > 10000, t.fileSize(dir + "/" + mp4));
	check("a new recording", files.length > before);
});
