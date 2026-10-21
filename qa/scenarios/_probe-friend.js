// One-off: import a stand-in for a friend's OBS collection and report what came over.
t.include("_lib.js");
skipSetup();
t.setTimeout(120);
story("probe-friend", "import the stand-in collection", () => {
	fresh();
	const ok = app.importObs(t.scriptDir() + "/../runs/fixtures/friend/Twitch.json");
	t.wait(6000);
	t.note("import ok=" + ok + ", toast: " + app.toast);
	const d = t.doc();
	const tree = id => { const i = d.instances[id]; const s = d.sources[i.sourceId] || {}; return { name: i.name, kind: i.kind === "group" ? "group" : s.kind, visible: eff(id, "visibility").visible, children: (i.children || []).map(tree) }; };
	t.metric("scenes", d.sceneOrder.map(s => ({ name: d.scenes[s].name, layers: d.scenes[s].layers.map(tree) })));
	t.metric("sources", d.sourceOrder.map(s => [d.sources[s].name, d.sources[s].kind, d.sources[s].hasAudio || false]));
	t.metric("mixer", app.mixer.map(m => [m.name, m.global]));
	t.metric("program", d.scenes[t.programScene()] ? d.scenes[t.programScene()].name : null);
	t.grab("friend-import.png");
	check("imported", ok);
});
