// Shared helpers for scenarios (t.include("_lib.js")). Files starting with "_" aren't run.

var GROUPS = ["transform", "crop", "filters", "visibility"];

// Effective value of a property group, following links like the app does.
function eff(id, group, d) {
	d = d || t.doc();
	const i = d.instances[id];
	const lid = i.links ? i.links[group] : undefined;
	if (lid && d.links[lid])
		return d.links[lid].value;
	return i.props[group];
}
function linkId(id, group, d) { d = d || t.doc(); const l = d.instances[id].links; return l ? l[group] : undefined; }
function linkedTogether(a, b, group, d) { d = d || t.doc(); const la = linkId(a, group, d); return !!la && la === linkId(b, group, d); }

// New scene named `name`; it becomes the edited scene. Returns its id.
function mkScene(name) {
	const before = t.doc().sceneOrder.slice();
	app.addScene();
	const id = lastAdded(before, t.doc().sceneOrder)[0];
	if (name)
		app.renameScene(id, name);
	return id;
}
// Adds a color source placed in the edited scene. Returns the new instance id (selected).
function placeColor(name, color) {
	app.addSource("color", name, color || "#ff0000", true);
	return app.inspector.id;
}
// Adds a source of any kind; returns {source, inst}.
function addKind(kind, name, extra, place) {
	const before = t.doc().sourceOrder.slice();
	app.addSource(kind, name, extra || "", place !== false);
	const sid = lastAdded(before, t.doc().sourceOrder)[0];
	return { source: sid, inst: place !== false ? app.inspector.id : undefined };
}
// Copies one instance to a scene (linked or not). Returns the new instance id.
function copyTo(id, sceneId, linked) {
	const keepScene = app.editSceneId;
	const d0 = t.doc();
	const sceneOf = d0.instances[id].sceneId;
	if (app.editSceneId !== sceneOf)
		app.editScene(sceneOf);
	app.selectLayer(id, false);
	const before = Object.keys(d0.instances);
	app.copySelectedTo(sceneId, linked);
	const after = Object.keys(t.doc().instances);
	const made = lastAdded(before, after).filter(x => t.doc().scenes[sceneId].layers.indexOf(x) >= 0);
	if (keepScene && keepScene !== app.editSceneId)
		app.editScene(keepScene);
	return made[0];
}
// Selects an instance, switching to its scene first.
function sel(id) {
	const s = t.doc().instances[id].sceneId;
	if (app.editSceneId !== s)
		app.editScene(s);
	app.selectLayer(id, false);
}
function setT(id, field, v) { sel(id); app.setTransform(field, v); }
function sameJson(a, b) { return JSON.stringify(a) === JSON.stringify(b); }
// Link ids referenced by instances vs defined: finds dangling or orphan links.
function linkIntegrity(d) {
	d = d || t.doc();
	const used = {}, problems = [];
	Object.keys(d.instances).forEach(iid => {
		const l = d.instances[iid].links || {};
		Object.keys(l).forEach(g => {
			if (!d.links[l[g]])
				problems.push("dangling " + iid + "." + g + " -> " + l[g]);
			used[l[g]] = (used[l[g]] || 0) + 1;
		});
	});
	Object.keys(d.links).forEach(lid => { if ((used[lid] || 0) < 2) problems.push("link " + lid + " has " + (used[lid] || 0) + " members"); });
	// Scene layer lists and group children point at real instances.
	Object.keys(d.scenes).forEach(sid => d.scenes[sid].layers.forEach(l => { if (!d.instances[l]) problems.push("scene " + sid + " lists missing " + l); }));
	Object.keys(d.instances).forEach(iid => (d.instances[iid].children || []).forEach(c => { if (!d.instances[c]) problems.push("group " + iid + " lists missing " + c); }));
	Object.keys(d.instances).forEach(iid => { const s = d.instances[iid].sourceId; if (s && !d.sources[s]) problems.push("instance " + iid + " -> missing source " + s); });
	return problems;
}
function integrity(label) { const p = linkIntegrity(); check(label || "document integrity", p.length === 0, p.slice(0, 10)); }
// Save as a project file and reopen it. Returns the reopened doc.
function roundTrip(name) {
	const f = t.out(name + ".lobs.json");
	check("saveAs " + name, app.saveAs(t.url(f)));
	app.newDocument();
	check("open " + name, app.open(t.url(f)));
	return t.doc();
}
// Closes the first-run setup assistant (a modal that swallows keys and clicks) if it's open.
function skipSetup() {
	if (t.items("Skip setup").length) { t.click("Skip setup"); t.wait(300); }
	return t.items("Welcome to LOBS").length === 0;
}
// Private MB once memory stops moving: under 5 MB of change across 4 s, giving up at 15 s.
// Freed memory goes back to Windows late and in steps: it holds about 3 s after hidden scenes
// park, then drops a few hundred MB about 1 s at a time (LOBS-038). GC doesn't hurry it.
function settledMB() {
	t.gc(); const s = [];
	for (let k = 0; k < 30; ++k) {
		t.wait(500); s.push(t.proc().privateMB);
		const w = s.slice(-9);
		if (w.length === 9 && Math.max.apply(null, w) - Math.min.apply(null, w) < 5) return w[8];
	}
	return s[s.length - 1];
}
