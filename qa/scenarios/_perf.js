// Performance stories, shared by 60-perf-heavy.js and 61-perf-stress.js.
// Every latency is "call → next presented frame" unless noted, in ms.

// Frame waits that time out are counted separately so a non-presenting window never shows up
// as app latency; the latency is then measured to the end of the call.
var frameTimeouts = 0;
function lat(fn) {
	const s = t.now();
	fn();
	const callEnd = t.now();
	if (t.waitFrames(1, 1000) < 0) { ++frameTimeouts; return callEnd - s; }
	return t.now() - s;
}
function progress(label) { t.writeFile(t.out("progress.txt"), label + " @ " + Math.round(t.now()) + " ms, frame timeouts " + frameTimeouts); }
function budgetCheck(name, st, p95, p99) {
	t.metric(name, st);
	const ok = st.p95 <= p95 && (p99 === undefined || st.p99 <= p99);
	check(name + ": p95 ≤ " + p95 + (p99 !== undefined ? ", p99 ≤ " + p99 : "") + " ms", ok, st);
	if (st.max > 1000) check(name + ": no single freeze > 1 s", false, "max " + st.max + " ms");
	return ok;
}
function lagDelta(fn) {
	const a = t.engineStats().live;
	fn();
	const b = t.engineStats().live;
	return { rendered: b.rendered - a.rendered, lagged: b.lagged - a.lagged };
}
function camInstances() {
	const d = t.doc();
	const cam = d.sourceOrder.filter(s => d.sources[s].kind === "camera")[0];
	return Object.keys(d.instances).filter(i => d.instances[i].sourceId === cam);
}

// opts: { only: ["PF-01", ...] (default all), switches: 100, edits: 1000, timeoutS }
function runPerf(profile, B, opts) {
	opts = opts || {};
	const want = id => !opts.only || opts.only.indexOf(id) >= 0;
	const file = t.scriptDir() + "/../runs/fixtures/" + profile + "/" + profile + ".lobs.json";
	t.setTimeout(opts.timeoutS || 3000);
	const switches = opts.switches || 100, edits = opts.edits || 1000;
	if (!want("PF-01")) { app.open(t.url(file)); t.waitFrames(2, 10000); t.wait(1000); }
	const story0 = story;
	story = (id, title, fn) => { if (want(id.replace("-" + profile, ""))) story0(id, title, fn); };

	story("PF-01-" + profile, "Open and save the " + profile + " project", () => {
		check("fixture exists (run qa/tools/gen_project.py)", t.exists(file), file);
		const m0 = t.proc();
		const times = [];
		for (let k = 0; k < 3; ++k) {
			app.newDocument(); t.wait(300);
			const s = t.now();
			const ok = app.open(t.url(file));
			const callMs = t.now() - s;
			t.waitFrames(2, 10000);
			times.push(t.now() - s);
			if (k === 0) { check("opens", ok, app.toast); t.metric("openCallMs", Math.round(callMs)); }
		}
		const st = stats(times);
		t.metric("open", st);
		check("open → interactive ≤ " + B.open + " ms (median of 3)", st.p50 <= B.open, st);
		const d = t.doc();
		t.metric("counts", { scenes: d.sceneOrder.length, sources: d.sourceOrder.length, instances: Object.keys(d.instances).length });
		const saves = [];
		for (let k = 0; k < 5; ++k) saves.push(time(() => app.saveProject()));
		budgetCheck("saveProject", stats(saves), B.save);
		// Autosave: the timer fires 400 ms after an edit and writes on the UI thread. Measure the stall.
		const sid = d.sceneOrder[0];
		app.editScene(sid);
		const stalls = [];
		for (let k = 0; k < 5; ++k) {
			app.renameScene(sid, "Scene 1 r" + k);
			let worst = 0, last = t.now();
			const end = last + 900;
			while (t.now() < end) { t.wait(5); const n = t.now(); worst = Math.max(worst, n - last - 5); last = n; }
			stalls.push(worst);
		}
		budgetCheck("autosaveStall (worst UI gap after an edit)", stats(stalls), B.hitch);
		t.metric("memoryAfterOpen", { before: m0, after: t.proc() });
	});

	story("PF-02-" + profile, "Scene switch latency (cut), " + switches + " switches", () => {
		app.setCurrentTransition("tr_cut");
		const ids = t.doc().sceneOrder;
		const edit = [], show = [];
		for (let k = 0; k < switches; ++k) {
			const id = ids[(k * 37) % ids.length];
			edit.push(lat(() => app.editScene(id)));
			const s = t.now();
			app.take();
			t.waitUntil(() => t.programScene() === id, 3000, 1);
			if (t.waitFrames(1, 1000) < 0) ++frameTimeouts;
			show.push(t.now() - s);
			progress("PF-02 switch " + k);
		}
		t.metric("frameTimeouts", frameTimeouts);
		budgetCheck("selectSceneForEdit", stats(edit), B.ui, B.ui * 2);
		budgetCheck("showScene (cut, to program + frame)", stats(show), B.ui, B.ui * 2);
		app.setCurrentTransition("tr_fade");
	});

	story("PF-03-" + profile, "Linked edit across every camera scene", () => {
		const cams = camInstances();
		t.metric("linkedInstances", cams.length);
		const xs = [];
		const lag = lagDelta(() => {
			for (let k = 0; k < 30; ++k) {
				sel(cams[k % cams.length]);
				xs.push(lat(() => app.setTransform("x", 1300 + k)));
			}
		});
		budgetCheck("linkedEdit", stats(xs), B.ui, B.ui * 2);
		check("every linked copy got the value", cams.every(i => eff(i, "transform").x === 1329), cams.slice(0, 5).map(i => eff(i, "transform").x));
		t.metric("renderLagDuringEdits", lag);
	});

	story("PF-04-" + profile, "Frame time while dragging and scrubbing", () => {
		const cams = camInstances();
		sel(cams[0]);
		app.setLiveUnlock(true);
		const tr = eff(cams[0], "transform");
		t.takeFrameTimes();
		const lag = lagDelta(() => {
			// One move per rendered frame, like a mouse (LOBS-024: a 16 ms timer left frames without a move).
			const r = t.canvasDrag(tr.x + 50, tr.y + 50, tr.x - 400, tr.y - 300, 120, "", -1);
			t.metric("dragMoveCallMs", stats(r.moves));
		});
		const ft = t.takeFrameTimes();
		const iv = [];
		for (let k = 1; k < ft.length; ++k) iv.push(ft[k] - ft[k - 1]);
		// Budgets are in screen refreshes (frame95: 16.7 = one at 60 Hz), so a 59 Hz screen gets
		// 16.95 ms, plus 1 ms for timestamp jitter. A dropped frame adds a whole refresh, far more.
		const hz = t.screenHz(), refresh = 1000 / hz, inRefreshes = ms => Math.round((Math.round(ms / 16.7) * refresh + 1) * 100) / 100;
		t.metric("screenHz", hz);
		budgetCheck("uiFrameIntervalDuringDrag", stats(iv), inRefreshes(B.frame95), inRefreshes(B.frame99));
		t.metric("renderLagDuringDrag", lag);
		// Inspector scrub: 120 steps, one undo step at the end.
		const h0 = app.history.length;
		const sc = [];
		for (let k = 0; k <= 120; ++k) sc.push(lat(() => app.scrubTransform("w", 300 + k, k === 120, true)));
		budgetCheck("scrubStep", stats(sc), B.frame95 * 2);
		eq("scrub is one undo step", app.history.length - h0, 1);
	});

	story("PF-05-" + profile, "Selection → inspector, and layer list with many layers", () => {
		const d = t.doc();
		const sid = d.sceneOrder.slice().sort((a, b) => d.scenes[b].layers.length - d.scenes[a].layers.length)[0];
		app.editScene(sid);
		const ids = app.layers.map(l => l.id);
		const ss = [];
		for (let k = 0; k < 60; ++k) ss.push(lat(() => app.selectLayer(ids[k % ids.length], false)));
		budgetCheck("selectLayer", stats(ss), B.ui, B.ui * 2);
		// Sources search: type into the field (live filter on every keystroke).
		const field = t.items("placeholderText=Search");
		t.metric("searchFields", field.length);
	});

	story("PF-06-" + profile, edits + " edits, then " + edits + " undos", () => {
		const cams = camInstances();
		sel(cams[0]);
		const m0 = t.proc();
		const ed = [], un = [];
		for (let k = 0; k < edits; ++k) { const s = t.now(); app.setTransform("y", 100 + (k % 500)); ed.push(t.now() - s); }
		const m1 = t.proc();
		for (let k = 0; k < edits; ++k) { const s = t.now(); app.undo(); un.push(t.now() - s); }
		t.waitFrames(1);
		const m2 = t.proc();
		budgetCheck("editCall", stats(ed), B.ui);
		budgetCheck("undoCall", stats(un), B.ui);
		t.metric("memory", { before: m0, after1000Edits: m1, afterUndo: m2,
			growthMB: Math.round(m1.privateMB - m0.privateMB), perEditKB: Math.round((m1.privateMB - m0.privateMB) * 1024 / edits) });
		t.metric("historyLength", app.history.length);
		check("memory per edit stays small (< 512 KB)", (m1.privateMB - m0.privateMB) * 1024 / edits < 512,
		      Math.round((m1.privateMB - m0.privateMB) * 1024 / edits) + " KB per edit");
	});

	story("PF-07-" + profile, "Render: lag at 1080p60 with the project on program", () => {
		app.editScene(t.doc().sceneOrder[0]); app.take();
		t.wait(2000);
		const idle = lagDelta(() => t.wait(20000));
		t.metric("idle20s", idle);
		t.metric("renderStats", t.engineStats().live);
		check("0 lagged frames over 20 s on program", idle.lagged === 0, idle);
		t.setAllScenesShowing(true);
		t.wait(3000);
		const all = lagDelta(() => t.wait(10000));
		t.metric("allScenesShowing10s", all);
		t.metric("renderStatsAllShowing", t.engineStats().live);
		t.setAllScenesShowing(false);
		t.note("all scenes active (scene grid thumbnails): lagged " + all.lagged + "/" + all.rendered);
	});
	story = story0;
}
