// Probe: the Spout2 plugin (win-spout, Off World Live) in LOBS. Loopback: a red layer sends itself
// with the plugin's Spout output filter, and a Spout2 Capture source receives it.
// Run twice with one plugins folder (plugins load at start); `zip` is the plugin's release zip.
t.include("_lib.js");
skipSetup();

const zip = "C:/Users/Logan/AppData/Local/Temp/claude/C--Users-Logan-Documents-LOBS/89bc3769-1670-423d-b300-5b250ba87671/scratchpad/spout-dl/win-spout.zip";
const plugin = () => app.plugins().filter(p => p.name === "win-spout")[0];
function isColor(px, r, g, b) { return px.ok && Math.abs(px.r - r) < 30 && Math.abs(px.g - g) < 30 && Math.abs(px.b - b) < 30; }

let installed = !!plugin();
story("SP-00", "win-spout is installed", () => {
	if (installed) return t.metric("plugin", plugin());
	const err = app.addPlugin(zip);
	check("adds the zip", err === "", err + " zip=" + zip);
	t.note("Installed; run again (plugins load at start).");
});

story("SP-01", "win-spout loads and registers its source and filter", () => {
	if (!installed) return t.note("skipped: run again");
	const p = plugin();
	t.metric("plugin", p);
	check("loaded", p.status === "loaded", p.status + ": " + p.reason);
	t.metric("sourceTypes", app.pluginSourceTypes());
	t.metric("filterTypes", app.filterTypes().map(o => o.value).filter(v => v.indexOf("obs:") === 0));
});

story("SP-02", "A Spout2 Capture source receives a Spout sender and renders it", () => {
	if (!installed) return t.note("skipped: run again");
	const kinds = app.pluginSourceTypes();
	const cap = kinds.filter(k => /spout/i.test(k.kind))[0];
	if (!check("Add Source offers a Spout source", !!cap, kinds)) return;
	const filt = app.filterTypes().map(o => o.value).filter(v => /spout/i.test(v))[0];
	if (!check("Add filter offers a Spout output filter", !!filt)) return;
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const red = addKind("color", "Red", "#ff0000");
	sel(red.inst); app.fit("fill");
	app.addFilter(filt);
	t.waitFrames(5, 3000);
	const frows = app.obsProperties("filter", red.inst, 0);
	t.metric("filterProps", frows.map(x => x.key + ":" + x.type + "=" + JSON.stringify(x.value)));
	const nameRow = frows.filter(r => /name/i.test(r.key))[0];
	if (nameRow) { app.setObsProperty("filter", red.inst, 0, nameRow.key, "LOBSProbe", true); t.waitFrames(2, 2000); }
	if (frows.some(r => r.key === "win_spout_apply")) app.clickObsProperty("filter", red.inst, 0, "win_spout_apply"); // starts sending under the new name
	t.waitFrames(10, 3000);

	const s = addKind(cap.kind, cap.label);
	t.wait(1000);
	let rows = app.obsProperties("source", s.source, 0);
	t.metric("captureProps", rows.map(x => x.key + ":" + x.type + "=" + JSON.stringify(x.value) + (x.options ? " options=" + JSON.stringify(x.options) : "")));
	const pick = rows.filter(r => r.options && r.options.some(o => /LOBSProbe/.test(o.value + o.name)))[0];
	if (check("the capture lists the LOBSProbe sender", !!pick, rows.map(r => r.key))) {
		const opt = pick.options.filter(o => /LOBSProbe/.test(o.value + o.name))[0];
		app.setObsProperty("source", s.source, -1, pick.key, opt.value, true);
	}
	t.waitUntil(() => t.sourceSize(s.source).w > 0, 5000);
	t.metric("captureSize", t.sourceSize(s.source));
	sel(s.inst); app.fit("fill");
	// A hidden layer stops feeding its Spout filter: keep the sender showing, tiny in a corner under the capture.
	["x", 1824, "y", 1026, "w", 96, "h", 54].forEach((k, i, a) => { if (i % 2 === 0) setT(red.inst, k, a[i + 1]); });
	t.waitFrames(10, 5000);
	const px = t.pixel(t.snapshot(A, "spout-capture.png", 0), 960, 540);
	check("the capture draws the red sender", isColor(px, 255, 0, 0), px);
});

// VNyan sends its avatar with a transparent background. Sender: an image, left half transparent,
// right half red. Over a blue layer, the capture's left half should show blue when alpha survives.
story("SP-03", "Transparency survives the Spout trip, per composite mode", () => {
	if (!installed) return t.note("skipped: run again");
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const blue = addKind("color", "Blue", "#0000ff");
	sel(blue.inst); app.fit("fill");
	const img = addKind("image", "Half red");
	app.setSourceSetting(img.source, "file", "C:/Users/Logan/AppData/Local/Temp/claude/C--Users-Logan-Documents-LOBS/89bc3769-1670-423d-b300-5b250ba87671/scratchpad/halfred.png");
	t.waitUntil(() => t.sourceSize(img.source).w > 0, 3000);
	sel(img.inst); app.fit("fill");
	app.addFilter("obs:win_spout_filter");
	t.waitFrames(3, 3000);
	app.setObsProperty("filter", img.inst, 0, "spout_filter_name", "LOBSAlpha", true);
	t.waitFrames(2, 2000);
	app.clickObsProperty("filter", img.inst, 0, "win_spout_apply");
	t.waitFrames(10, 3000);
	const s = addKind("obs:spout_capture", "Spout2 Capture");
	t.wait(1000);
	app.setObsProperty("source", s.source, -1, "spoutsenders", "LOBSAlpha", true);
	t.waitUntil(() => t.sourceSize(s.source).w > 0, 5000);
	sel(s.inst); app.fit("fill");
	// A hidden layer stops feeding its Spout filter: keep the sender showing, tiny in a corner under the capture.
	["x", 1824, "y", 1026, "w", 96, "h", 54].forEach((k, i, a) => { if (i % 2 === 0) setT(img.inst, k, a[i + 1]); });
	const startMode = app.obsProperties("source", s.source, 0).filter(r => r.key === "compositemode")[0];
	t.metric("defaultMode", startMode ? startMode.value : "?");
	t.metric("storedSettings", t.doc().sources[s.source].settings);
	const res = {};
	["1", "2", "3", "4"].forEach(m => {
		app.setObsProperty("source", s.source, -1, "compositemode", m, true);
		t.waitFrames(60, 5000); t.wait(500);
		const f = t.snapshot(A, "alpha-mode" + m + ".png", 0);
		res[m] = { left: t.pixel(f, 480, 540), right: t.pixel(f, 1440, 540) };
	});
	t.metric("byMode", res);
	check("right half is red", isColor(res["3"].right, 255, 0, 0), res["3"].right);
	check("Default mode: left half shows the blue behind", isColor(res["3"].left, 0, 0, 255), res["3"].left);
	check("Premultiplied mode: left half shows the blue behind", isColor(res["4"].left, 0, 0, 255), res["4"].left);
	t.metric("opaqueLeft", res["1"].left);
});

// LOBS-042: a hidden layer with a plugin filter keeps it (OBS never drops a hidden source's
// filters), and a parked hidden layer stays parked while other things are edited.
story("SP-04", "Hidden layers: plugin filters stay, parked layers stay parked through edits", () => {
	if (!installed) return t.note("skipped: run again");
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const plain = placeColor("Plain", "#808080");
	app.addFilter("color");
	const sender = placeColor("Sender", "#ff0000");
	app.addFilter("obs:win_spout_filter");
	const other = addKind("color", "Other", "#00ff00");
	t.waitFrames(5, 3000);
	sel(plain); app.toggleVisible(plain);
	sel(sender); app.toggleVisible(sender);
	// Switching away and back runs the park timer over this scene.
	const B = mkScene("B");
	app.editScene(B); app.take(); t.wait(1500);
	app.editScene(A); app.take(); t.wait(1500);
	const st = () => t.engineStats().layers;
	const settled = st();
	t.metric("settled", settled);
	check("hidden layers are parked", settled.parked >= 1, settled);
	check("the hidden Spout layer keeps its filter", settled.filters >= 1, settled);
	const after = [];
	["#00ee00", "#00dd00", "#00cc00", "#00bb00", "#00aa00"].forEach(c => {
		app.setSourceSetting(other.source, "color", c);
		t.waitFrames(2, 2000);
		after.push(st());
	});
	t.metric("afterEdits", after);
	check("edits elsewhere leave the hidden layer parked", after.every(s => s.parked === settled.parked), after.map(s => s.parked));
	check("and the Spout filter is never dropped", after.every(s => s.filters === settled.filters), after.map(s => s.filters));
	sel(plain); app.toggleVisible(plain); t.waitFrames(5, 3000);
	const shown = st();
	check("showing the parked layer again brings it back", shown.parked === settled.parked - 1, { settled: settled.parked, now: shown.parked });
});

// LOBS-042's crash recipe: the sender hidden, the capture's settings changed again and again.
story("SP-05", "A hidden Spout sender survives repeated edits to the capture", () => {
	if (!installed) return t.note("skipped: run again");
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const snd = placeColor("Sender", "#ff0000");
	app.addFilter("obs:win_spout_filter");
	t.waitFrames(3, 3000);
	app.setObsProperty("filter", snd, 0, "spout_filter_name", "LOBSCrash", true);
	app.clickObsProperty("filter", snd, 0, "win_spout_apply");
	const s = addKind("obs:spout_capture", "Spout2 Capture");
	app.setObsProperty("source", s.source, -1, "spoutsenders", "LOBSCrash", true);
	t.waitFrames(5, 3000);
	sel(snd); app.toggleVisible(snd);
	t.wait(1200); // past the park timer
	for (let k = 0; k < 12; ++k) {
		app.setObsProperty("source", s.source, -1, "compositemode", String(1 + k % 4), true);
		t.waitFrames(3, 2000);
	}
	check("still running after 12 edits", true);
});
