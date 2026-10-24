// Probe (not in the default run): add every source kind through the real Add Source dialog,
// by clicks, and check what each one gets (device, layer, size).
t.include("_lib.js");

function ui(text) { return t.items(text); }
function has(text) { return ui(text).length > 0; }
function newSources(before) { return t.doc().sourceOrder.filter(s => before.indexOf(s) < 0); }

function openAdd() {
	// Icon-only when the Sources panel is narrow, so found by its tooltip.
	const btn = ui("tip=Add a source");
	check("'+ Source' button found", btn.length > 0);
	t.clickItem("tip=Add a source", 0);
	t.wait(500);
	return has("Add a source");
}

// The kind tile in the dialog (the Repeater's Text; the last match is the dialog's, over the panels).
function pickKind(label) {
	const items = ui(label);
	if (!items.length) return false;
	t.clickItem(label, items.length - 1);
	t.wait(700); // device lists take up to ~500 ms on Windows
	return true;
}

story("AS-01u", "Add Source dialog: every kind on Windows", () => {
	skipSetup();
	fresh();
	const canvas = t.doc().canvas;
	const kinds = [["Camera", "camera"], ["Display", "display"], ["Window", "window"], ["Game", "game"],
		["Browser", "browser"], ["Mic", "mic"], ["Desktop", "desktop_audio"]];
	check("dialog opens", openAdd());
	for (const [label, kind] of kinds)
		check("kind tile '" + label + "' is offered", has(label));
	t.grab("as01-dialog.png");
	t.key("Escape"); t.wait(300);

	for (const [label, kind] of kinds) {
		const before = t.doc().sourceOrder.slice();
		if (!openAdd()) { check(label + ": dialog opens", false); continue; }
		check(label + ": tile clicked", pickKind(label));
		const devs = app.devicesForKind(kind);
		t.metric(kind + "Devices", devs.length);
		let picked = "";
		if (kind === "window") {
			// Add waits for a window: nothing is added until one is chosen.
			t.click("Add source"); t.wait(400);
			eq("window: Add does nothing before a window is chosen", newSources(before).length, 0);
			const target = devs.find(d => /explorer|LOBS|Code|Chrome|Claude/i.test(d.name)) || devs[0];
			check("window: open windows are listed", devs.length > 0, devs.length + " windows");
			if (!target) { t.key("Escape"); continue; }
			const rows = ui(String(target.name));
			check("window: the window shows in the dialog's list", rows.length > 0, target.name);
			t.clickItem(String(target.name), rows.length - 1); t.wait(200);
			picked = target.value;
			t.grab("as01-window-picked.png");
		} else if (kind === "camera" || kind === "display") {
			check(kind + ": device list shown", devs.length > 0, JSON.stringify(devs.map(d => d.name)));
			const real = devs.find(d => d.name.toLowerCase().indexOf("virtual") < 0);
			picked = real ? real.value : "";
		}
		t.click("Add source"); t.wait(1500);
		const added = newSources(before);
		eq(label + ": one source added", added.length, 1);
		if (!added.length) continue;
		const s = t.doc().sources[added[0]];
		eq(label + ": kind", s.kind, kind);
		if (picked)
			eq(label + ": device is the one picked in the dialog", s.settings.device, picked);
		if (kind === "window")
			check("window: named after its window", s.name.length > 0 && s.name !== "Window Capture", s.name);
		t.note(label + ": '" + s.name + "' device=" + (s.settings.device || "(none)"));
		const engine = t.obsInputs().find(i => i.name === added[0] || i.id === added[0]);
		t.metric(kind + "Engine", engine ? JSON.stringify(engine) : "not in engine");
	}

	// Layers without a fixed size (display, window, game) fit their real size once it's known.
	t.wait(3000);
	const d = t.doc();
	const scene = d.scenes[app.editSceneId];
	for (const id of scene.layers) {
		const inst = d.instances[id];
		if (!inst) continue;
		const s = d.sources[inst.sourceId];
		if (!s || ["display", "window", "game"].indexOf(s.kind) < 0) continue;
		const size = t.sourceSize(inst.sourceId);
		const tr = eff(id, "transform", d) || {};
		t.note(s.kind + ": source " + size.w + "x" + size.h + ", layer " + JSON.stringify(tr));
		if (size.w > 0 && tr.w) {
			const k = Math.min(1, canvas.w / size.w, canvas.h / size.h);
			check(s.kind + ": layer fits the capture's own shape on the canvas",
				Math.abs(tr.w - Math.round(size.w * k)) <= 2 && Math.abs(tr.h - Math.round(size.h * k)) <= 2,
				"layer " + tr.w + "x" + tr.h + ", want " + Math.round(size.w * k) + "x" + Math.round(size.h * k));
			check(s.kind + ": layer stays on the canvas", tr.w <= canvas.w && tr.h <= canvas.h);
		}
	}
	t.grab("as01-canvas.png");
	integrity();
});
