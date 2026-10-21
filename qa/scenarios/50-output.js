// Output: OUT-01 record, OUT-02 replay buffer, OUT-03 virtual camera, OUT-04 local RTMP stream.
// Run with qa\run-output.ps1, which starts a local ffmpeg RTMP receiver on 127.0.0.1:1935 and
// kills it when this script writes <out>/kill-receiver (to simulate a network drop).
t.include("_lib.js");
skipSetup();

function scene() {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	placeColor("Bg", "#204060"); app.fit("fill");
	app.addSource("text", "Clock", "LOBS QA", true);
	app.take(); t.wait(500);
}
function recordFor(seconds) {
	const n = t.recordingsFinished();
	app.toggleRecording();
	const started = t.waitUntil(() => app.recording, 5000);
	t.wait(seconds * 1000);
	app.toggleRecording();
	const done = t.waitUntil(() => t.recordingsFinished() > n, 20000);
	return { started: started, done: done, rec: t.lastRecording() };
}

story("OUT-01", "Record: start, stop, file is valid, formats", () => {
	scene();
	const r = recordFor(5);
	check("recording starts", r.started);
	check("recording finishes", r.done, r.rec);
	check("file written", r.rec.ok && r.rec.bytes > 10000, r.rec);
	t.metric("mp4", r.rec);
	["mkv", "mov"].forEach(fmt => {
		app.saveSettings({ recordFormat: fmt, recordFolder: t.out("rec") });
		const x = recordFor(3);
		check(fmt + ": file written with ." + fmt, x.rec.ok && String(x.rec.path).toLowerCase().endsWith("." + fmt), x.rec);
		t.metric(fmt, x.rec);
	});
	app.saveSettings({ recordFormat: "mp4", recordFolder: t.out("rec") });
	t.writeFile(t.out("probe-list.txt"), [t.metric ? "" : ""].join(""));
});

story("OUT-01b", "Record into a folder that can't be written", () => {
	scene();
	app.saveSettings({ recordFolder: "Z:/no/such/drive" });
	const n = t.recordingsFinished();
	app.toggleRecording();
	t.wait(2500);
	const rec = app.recording;
	const toast = app.toast, err = t.lastError();
	t.note("unwritable folder: recording=" + rec + ", toast='" + toast + "', lastError='" + err + "', status='" + app.status + "'");
	if (rec) { app.toggleRecording(); t.waitUntil(() => t.recordingsFinished() > n, 10000); }
	check("fails visibly (no silent 'recording' that produces nothing)", !rec && (toast || err), { recording: rec, toast: toast, err: err, last: t.lastRecording() });
	check("the error uses current Windows wording (LOBS-035)", !/Windows 10 Ransomware/.test(err + toast + app.status), err);
	app.saveSettings({ recordFolder: t.out("rec") });
});

story("OUT-02", "Replay buffer: saves the last N seconds", () => {
	scene();
	app.saveSettings({ replaySeconds: 10, recordFolder: t.out("rec") });
	app.toggleReplay();
	check("replay buffer on", t.waitUntil(() => app.replayActive, 5000), app.toast);
	t.wait(14000);
	const n = t.replaysSaved();
	app.saveReplay();
	check("replay saved", t.waitUntil(() => t.replaysSaved() > n, 15000));
	const p = t.lastReplay();
	t.metric("replay", { path: p, bytes: t.fileSize(p) });
	check("toast names the file", /Replay saved/.test(app.toast), app.toast);
	// While recording too.
	const r = recordFor(3);
	check("record works while replay is on", r.rec.ok, r.rec);
	app.toggleReplay();
	t.waitUntil(() => !app.replayActive, 5000);
	app.saveReplay();
	check("save with replay off explains why", /replay buffer/i.test(app.toast), app.toast);
});

story("OUT-03", "Virtual camera", () => {
	scene();
	t.metric("available", app.vcamAvailable);
	if (!app.vcamAvailable) {
		t.blocked("virtual camera not installed/registered on this PC; UI text: see screenshot");
		t.grab("out03-vcam-unavailable.png");
		return;
	}
	app.toggleVirtualCam();
	check("virtual camera starts", t.waitUntil(() => app.vcamActive, 5000), app.toast);
	t.wait(2000);
	app.toggleVirtualCam();
	check("virtual camera stops", t.waitUntil(() => !app.vcamActive, 5000));
});

story("OUT-04", "Stream to a local RTMP receiver", () => {
	scene();
	app.saveSettings({ service: "Custom", customUrl: "rtmp://127.0.0.1:1935/live", key: "qa", bitrate: 4000 });
	app.toggleStreaming();
	const up = t.waitUntil(() => app.streaming, 10000);
	check("stream connects", up, { toast: app.toast, err: t.lastError(), status: app.status });
	if (!up) return;
	check("live lock engages on going live", app.liveLocked);
	t.wait(10000);
	const st = app.streamStats;
	t.metric("streamStats", st);
	t.metric("liveStats", t.engineStats().live);
	check("stream stats update", st && Object.keys(st).length > 0, st);
	// Network drop: the receiver goes away.
	t.writeFile(t.out("kill-receiver"), "now");
	const trace = [];
	const t0 = t.now();
	const sawDrop = t.waitUntil(() => {
		const st = app.streamStats || {};
		if (!trace.length || t.now() - trace[trace.length - 1].ms - t0 >= 2000)
			trace.push({ ms: Math.round(t.now() - t0), streaming: app.streaming, bytes: st.bytes, kbps: Math.round(st.kbps || 0), health: st.health, reconnecting: st.reconnecting, status: app.status });
		return !app.streaming || st.reconnecting || /reconnect|disconnect|lost/i.test(app.status + " " + app.toast + " " + t.streamErrors().join(" "));
	}, 60000, 100);
	trace.push({ ms: Math.round(t.now() - t0), streaming: app.streaming, health: (app.streamStats || {}).health, reconnecting: (app.streamStats || {}).reconnecting, status: app.status, final: true });
	t.metric("afterDropTrace", trace);
	t.metric("detectMs", sawDrop ? Math.round(t.now() - t0) : -1);
	t.note("after receiver dropped: streaming=" + app.streaming + ", status='" + app.status + "', toast='" + app.toast + "', errors=" + JSON.stringify(t.streamErrors()));
	t.grab("out04-after-drop.png");
	check("a dropped connection is reported to the user", sawDrop);
	if (app.streaming) { app.toggleStreaming(); t.waitUntil(() => !app.streaming, 15000); }
	check("stream stops cleanly", !app.streaming);
});

story("OUT-04b", "Stream to a server that isn't there", () => {
	scene();
	app.saveSettings({ service: "Custom", customUrl: "rtmp://127.0.0.1:1937/live", key: "qa" });
	const e0 = t.streamErrors().length;
	app.toggleStreaming();
	t.waitUntil(() => t.streamErrors().length > e0 || /fail|couldn|can't|error/i.test(app.toast), 20000);
	t.wait(1000);
	const msg = [app.toast, t.lastError()].concat(t.streamErrors().slice(e0)).filter(Boolean);
	t.note("bad server: streaming=" + app.streaming + ", messages=" + JSON.stringify(msg));
	t.grab("out04b.png");
	check("clear error, not stuck 'connecting'", !app.streaming && msg.length > 0, { streaming: app.streaming, msg: msg });
	if (app.streaming) app.toggleStreaming();
});
