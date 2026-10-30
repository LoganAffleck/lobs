// Probe: LOBS's obs-websocket server (P2-2), driven by a real WebSocket client from outside:
// qa/tools/ws_client.py, started next to this run. They meet through files in `dir`: this writes
// ws-ready.json (port, password, scene names), the client writes ws-result.json when done.
//   qa\run.ps1 _probe-websocket   and, alongside:   python -I qa/tools/ws_client.py <dir>
t.include("_lib.js");
skipSetup();

const dir = "C:/Users/Logan/AppData/Local/Temp/lobs-ws-probe";

story("WS-01", "Apps made for OBS control LOBS over obs-websocket", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.renameScene(A, "Alpha");
	const B = mkScene("Beta");
	app.editScene(A); app.take();
	const red = placeColor("Red box", "#ff0000");
	t.waitFrames(2, 2000);

	t.removeFile(dir + "/ws-result.json");
	websocket.setPort(4466); // not OBS's 4455, so a running OBS doesn't clash
	websocket.setAuthRequired(true);
	websocket.setEnabled(true);
	check("server is up", websocket.enabled && !websocket.error, websocket.error);
	t.writeFile(dir + "/ws-ready.json", JSON.stringify({ port: websocket.port, password: websocket.password, a: "Alpha", b: "Beta", layer: "Red box" }));

	// The client drives LOBS from here on; wait for it.
	let waited = 0;
	while (!t.exists(dir + "/ws-result.json") && waited < 60000) { t.wait(250); waited += 250; }
	check("the client finished", t.exists(dir + "/ws-result.json"), waited + " ms");
	const r = JSON.parse(t.readFile(dir + "/ws-result.json") || "{}");
	t.metric("client", r);
	for (const k of Object.keys(r.checks || {}))
		check("client: " + k, r.checks[k] === true, r.details ? r.details[k] : "");
	eq("program switched to Beta through the socket", t.programScene(), B);
	check("the layer was hidden through the socket", eff(red, "visibility").visible === false);
	websocket.setEnabled(false);
	t.removeFile(dir + "/ws-ready.json");
});
