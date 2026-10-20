// OBS import: IM-01 (real collections, copies), IM-02 (robustness), import while live.
t.include("_lib.js");
skipSetup();
t.setTimeout(900);
const dir = t.scriptDir() + "/../runs/fixtures/obs";
const AUDIO = ["wasapi_input_capture", "wasapi_output_capture", "wasapi_process_output_capture", "sck_audio_capture", "coreaudio_input_capture"];

function obsModel(o) {
	const byName = {};
	o.sources.forEach(s => byName[s.name] = s);
	const helper = (n, id) => /^\[LOBS\]|\[LOBS\]$/.test(n) || id === "lobs_instance";
	const allScenes = (o.scene_order || []).map(x => x.name).filter(n => byName[n]);
	// Scenes made only of old LOBS-for-OBS plugin helpers are skipped on purpose.
	const scenes = allScenes.filter(n => !helper(n, "scene") && (byName[n].settings.items || []).some(it => !helper(it.name, byName[it.name] ? byName[it.name].id : "")));
	const droppedScenes = allScenes.filter(n => scenes.indexOf(n) < 0);
	const items = {};
	scenes.forEach(n => {
		items[n] = (byName[n].settings.items || []).map(it => ({ name: it.name, visible: it.visible !== false,
			kind: byName[it.name] ? byName[it.name].id : "?", pos: it.pos, scale: it.scale, crop: [it.crop_left, it.crop_top, it.crop_right, it.crop_bottom] }));
	});
	const inputs = o.sources.filter(s => s.id !== "scene" && s.id !== "group");
	return { scenes: scenes, droppedScenes: droppedScenes, items: items, inputs: inputs, current: o.current_program_scene || o.current_scene, byName: byName };
}

["OLD", "Untitled"].forEach(name => {
	story("IM-01-" + name, "Import OBS collection '" + name + "'", () => {
		const path = dir + "/" + name + ".json";
		const raw = t.readFile(path);
		check("fixture present", raw.length > 0, path);
		const o = JSON.parse(raw), m = obsModel(o);
		fresh();
		const ok = app.importObs(path);
		const toastNow = app.toast;
		check("import succeeds", ok, toastNow);
		t.note("toast right after import: " + toastNow);
		t.wait(12000); // placements resolve once sources report their sizes
		const d = t.doc();
		const lobsScenes = d.sceneOrder.map(s => d.scenes[s].name);
		eq("scene names and order (plugin-helper scenes excluded)", lobsScenes, m.scenes);
		if (m.droppedScenes.length) {
			t.metric("droppedScenes", m.droppedScenes);
			const told = m.droppedScenes.every(n => toastNow.indexOf(n) >= 0) || /plugin|skipp|LOBS helper/i.test(toastNow);
			check("user is told which scenes were left out", told, "dropped " + JSON.stringify(m.droppedScenes) + "; toast: '" + toastNow + "'");
		}
		t.note("import notes/toast: " + app.toast);
		t.metric("toast", app.toast);
		// Every OBS input comes over once (audio inputs as pool-wide sources, graphics/captures placed).
		const used = {};
		m.scenes.forEach(n => m.items[n].forEach(it => used[it.name] = true));
		const expected = m.inputs.filter(s => used[s.name] && !/\[LOBS\]/.test(s.name) && s.id !== "lobs_instance" && s.id !== "sck_audio_capture").map(s => s.name).sort();
		const got = d.sourceOrder.map(s => d.sources[s]).filter(s => !(s.settings && s.settings.background)).map(s => s.name).sort();
		const missing = expected.filter(n => got.indexOf(n) < 0);
		check("each used OBS input arrives exactly once", missing.length === 0 && new Set(got).size === got.length, { missing: missing, got: got });
		t.metric("sources", { expected: expected.length, got: got.length, kinds: d.sourceOrder.map(s => d.sources[s].kind) });
		// Per scene: visible-video item count and hidden flags.
		const per = [];
		m.scenes.forEach((n, k) => {
			const sid = d.sceneOrder.filter(x => d.scenes[x].name === n)[0];
			if (!sid) { per.push({ scene: n, missing: true }); return; }
			const flat = [];
			const walk = ids => ids.forEach(id => { const i = d.instances[id]; if (!i) return; if (i.kind === "group") walk(i.children); else flat.push(i); });
			walk(d.scenes[sid].layers);
			const placed = flat.filter(i => !(d.sources[i.sourceId] && d.sources[i.sourceId].settings.background));
			const obsVideo = m.items[n].filter(it => AUDIO.indexOf(it.kind) < 0);
			const hiddenObs = obsVideo.filter(it => !it.visible).map(it => it.name).sort();
			const hiddenLobs = placed.filter(i => !eff(i.id, "visibility", d).visible).map(i => i.name).sort();
			per.push({ scene: n, obsItems: obsVideo.length, lobsLayers: placed.length, hiddenObs: hiddenObs, hiddenLobs: hiddenLobs });
		});
		t.metric("perScene", per);
		const countBad = per.filter(p => p.obsItems !== p.lobsLayers);
		check("every OBS item becomes a layer (or a note says why)", countBad.length === 0 || /skip|didn't|not supported|unsupported/i.test(app.toast), countBad);
		const hiddenBad = per.filter(p => JSON.stringify(p.hiddenObs) !== JSON.stringify(p.hiddenLobs));
		check("hidden items stay hidden", hiddenBad.length === 0, hiddenBad);
		// Crops come over.
		const crops = [];
		m.scenes.forEach((n, k) => m.items[n].forEach(it => {
			if (it.crop.some(v => v)) {
				const sk = d.sceneOrder.filter(x => d.scenes[x].name === n)[0];
				const inst = Object.keys(d.instances).map(i => d.instances[i]).filter(i => i.sceneId === sk && i.name === it.name)[0];
				const c = inst ? eff(inst.id, "crop", d) : null;
				crops.push({ scene: n, item: it.name, obs: it.crop, lobs: c ? [c.l, c.t, c.r, c.b] : null });
			}
		}));
		t.metric("crops", crops);
		check("crops match", crops.every(c => c.lobs && c.lobs.join() === c.obs.map(v => v || 0).join()), crops.filter(c => !c.lobs || c.lobs.join() !== c.obs.map(v => v || 0).join()));
		eq("OBS program scene is on program", d.scenes[t.programScene()] && d.scenes[t.programScene()].name, m.current);
		integrity();
		d.sceneOrder.forEach((s, k) => t.snapshot(s, "im01-" + name + "/scene" + (k + 1) + ".png", 960));
		t.setAllScenesShowing(false);
		check("original OBS file untouched", t.readFile(path) === raw);
	});
});

story("IM-02", "Import robustness: broken files never touch the open show", () => {
	fresh();
	placeColor("Keep", "#ffffff");
	const keep = t.docJson();
	const src = t.readFile(dir + "/Untitled.json");
	const cases = {
		"truncated.json": src.slice(0, Math.floor(src.length / 2)),
		"empty.json": "",
		"notobs.json": JSON.stringify({ hello: "world" }),
		"noscenes.json": JSON.stringify({ name: "x", sources: [], scene_order: [] }),
		"weird-types.json": JSON.stringify({ name: "x", current_scene: 5, sources: [{ id: "scene", name: 7, settings: { items: "nope" } }], scene_order: [{ name: null }] }),
	};
	const res = [];
	Object.keys(cases).forEach(n => {
		const f = t.out("im02/" + n);
		t.writeFile(f, cases[n]);
		const ok = app.importObs(f);
		t.wait(300);
		res.push([n, ok, app.toast, t.doc().sceneOrder.length]);
		if (!ok) check(n + ": open show untouched", t.docJson() === keep);
		else { t.note(n + " imported (" + t.doc().sceneOrder.length + " scenes); toast='" + app.toast + "'"); integrity(n + " imported doc integrity"); app.undo(); }
	});
	t.metric("results", res);
	check("no crash on any broken file", true);
});

story("IM-03", "Import while recording doesn't switch what's on air without asking", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); placeColor("Live", "#ff0000"); app.take(); t.wait(400);
	t.startRecording();
	t.waitUntil(() => app.live, 5000);
	const before = t.programScene();
	app.importObs(dir + "/OLD.json");
	t.wait(1500);
	const after = t.programScene();
	t.note("program before import: " + before + ", after: " + after + " (" + (t.doc().scenes[after] ? t.doc().scenes[after].name : "?") + ")");
	check("import while live keeps the on-air scene (or asks first)", after === before, "program switched to the imported scene while recording");
	t.stopRecording();
	t.waitUntil(() => !app.live, 10000);
	t.setAllScenesShowing(false);
});
