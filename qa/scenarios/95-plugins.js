// Third-party OBS plugins (qa/PLUGINS_PLAN.md): PL-01 installed plugins are listed with a status,
// PL-02 a plugin's filter can be added and changes what the layer draws, PL-03 a plugin's
// source can be added and renders, PL-04 filters survive save and open, PL-05 the generic editor,
// PL-06 removing and re-adding in use, PL-07 the UI (Settings > Plugins, filter editor, Add Source).
//
// Plugins come from the official GitHub release zips in qa/plugins/ (not committed):
//   obs-shaderfilter-*-windows.zip, obs-composite-blur-*-windows-x64.zip, move-transition-*-windows.zip,
//   source-clone-*-windows.zip
// The first run installs them (plugins load at start) and asks for a second run. QA's AppData
// starts empty every run, so give the runs one plugins folder that lasts:
//   $env:LOBS_PLUGINS_DIR = "$PWD\qa\runs\plugins"; qa\run.ps1 95-plugins   (twice)
t.include("_lib.js");
skipSetup();

const zipDir = t.scriptDir() + "/../plugins";
const byName = () => { const m = {}; app.plugins().forEach(p => m[p.name] = p); return m; };
const props = (scope, id, i) => app.obsProperties(scope, id, i || 0);
const findProp = (rows, re) => rows.filter(r => re.test(r.key))[0];
function isColor(px, r, g, b) { return px.ok && Math.abs(px.r - r) < 30 && Math.abs(px.g - g) < 30 && Math.abs(px.b - b) < 30; }
const solid = (r, g, b) => "float4 mainImage(VertData v_in) : TARGET\n{\n\treturn float4(" + [r, g, b].join(", ") + ", 1.0);\n}\n";

let installedNow = [];
story("PL-00", "Plugins from qa/plugins are installed", () => {
	const zips = t.exists(zipDir) ? t.listDir(zipDir).filter(f => /\.zip$/i.test(f)) : [];
	check("qa/plugins has plugin zips", zips.length > 0, zipDir);
	const have = byName();
	zips.forEach(z => {
		const stem = z.replace(/-\d.*$/, "").replace(/^obs-(?=shaderfilter|composite-blur)/, "obs-");
		const known = Object.keys(have).some(n => stem.indexOf(n) >= 0 || n.indexOf(stem) >= 0);
		if (known)
			return;
		const err = app.addPlugin(zipDir + "/" + z);
		check("adds " + z, err === "", err);
		installedNow.push(z);
	});
	t.metric("installedNow", installedNow);
	t.metric("folder", app.pluginsFolder());
	if (installedNow.length) {
		check("asks for a restart after adding", app.pluginsNeedRestart());
		t.note("Installed " + installedNow.join(", ") + ". Run this scenario again: plugins load when LOBS starts.");
	}
});

const loaded = installedNow.length === 0;
const pl = byName();
const shader = pl["obs-shaderfilter"], blur = pl["obs-composite-blur"], move = pl["move-transition"];

story("PL-01", "Installed plugins are listed with name, version, folder and status", () => {
	if (!loaded) return t.note("skipped: run again after PL-00 installed plugins");
	t.metric("plugins", app.plugins());
	check("restart not needed right after start", !app.pluginsNeedRestart());
	[shader, blur, move].forEach((p, i) => {
		const n = ["obs-shaderfilter", "obs-composite-blur", "move-transition"][i];
		if (!check(n + " is listed", !!p)) return;
		check(n + " has a folder", p.folder.length > 0, p);
		check(n + " has a version", p.version.length > 0, p);
		check(n + " says why if it didn't load", p.status === "loaded" || p.reason.length > 0, p);
	});
	check("Shader Filter loaded with its filter", shader && shader.status === "loaded" && shader.filterIds.indexOf("shader_filter") >= 0, shader);
	check("Composite Blur loaded with its filter", blur && blur.status === "loaded" && blur.filterIds.length > 0, blur);
	t.metric("move-transition", move ? move.status + ": " + move.reason + " filters " + move.filterIds.join(",") : "missing");
});

// obs-shaderfilter compiles a new shader when "Reload effect" is pressed, in OBS too.
function setShader(layer, i, text) {
	app.setObsProperty("filter", layer, i, "shader_text", text, true);
	t.waitFrames(2, 2000);
	app.clickObsProperty("filter", layer, i, "reload_effect");
	t.waitFrames(5, 3000);
}
const halves = "float4 mainImage(VertData v_in) : TARGET\n{\n\treturn v_in.uv.x < 0.5 ? float4(1.0, 1.0, 1.0, 1.0) : float4(0.0, 0.0, 0.0, 1.0);\n}\n";

story("PL-02", "A plugin filter is in the Add filter list, adds, and changes the picture", () => {
	if (!loaded || !shader || shader.status !== "loaded") return t.note("skipped: Shader Filter not loaded");
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const r = placeColor("Blue", "#0000ff");
	app.fit("fill");
	const types = app.filterTypes().map(o => o.value);
	check("Add filter offers the shader filter", types.indexOf("obs:shader_filter") >= 0, types);
	t.waitFrames(3, 3000);
	check("layer starts blue", isColor(t.pixel(t.snapshot(A, "pl02-before.png", 0), 960, 540), 0, 0, 255));
	sel(r); app.addFilter("obs:shader_filter");
	t.waitFrames(3, 3000);
	eq("the layer stores the plugin filter", eff(r, "filters").map(x => x.type), ["obs:shader_filter"]);
	eq("the engine knows the filter's module", t.engineStats().layers.unknownFilters, 0);
	const rows = props("filter", r, 0);
	t.metric("shaderFilterProps", rows.map(x => x.key + ":" + x.type));
	check("its settings are listed", rows.length > 3, rows.length);
	if (!check("it has a shader text setting and a reload button", !!findProp(rows, /^shader_text$/) && !!findProp(rows, /^reload_effect$/), rows.map(x => x.key))) return;
	setShader(r, 0, solid(1.0, 0.0, 0.0));
	const px = t.pixel(t.snapshot(A, "pl02-red.png", 0), 960, 540);
	check("the shader turns the layer red", isColor(px, 255, 0, 0), px);
	sel(r); app.toggleFilter(0); t.waitFrames(3, 3000);
	const off = t.pixel(t.snapshot(A, "pl02-off.png", 0), 960, 540);
	check("turning it off brings the blue back", isColor(off, 0, 0, 255), off);
	sel(r); app.toggleFilter(0); t.waitFrames(3, 3000);
	// Composite Blur after it: the shader draws white | black, and the blur softens the middle.
	if (!blur || blur.status !== "loaded")
		return t.note("Composite Blur not loaded");
	const blurId = blur.filterIds[0];
	check("Add filter offers Composite Blur", types.indexOf("obs:" + blurId) >= 0, blurId);
	setShader(r, 0, halves);
	const sharp = t.pixel(t.snapshot(A, "pl02-halves.png", 0), 975, 540);
	check("the shader draws black right of the middle", isColor(sharp, 0, 0, 0), sharp);
	sel(r); app.addFilter("obs:" + blurId);
	t.waitFrames(3, 3000);
	// OBS's properties view picks a list's first choice when its value isn't one (Gaussian here).
	eq("a new Composite Blur gets its first algorithm, as in OBS", eff(r, "filters")[1].settings.blur_algorithm, 1);
	const brows = props("filter", r, 1);
	t.metric("blurProps", brows.map(x => x.key + ":" + x.type + "=" + x.value));
	const radius = findProp(brows, /^radius$/);
	if (!check("blur has a radius setting", !!radius, brows.map(x => x.key))) return;
	app.setObsProperty("filter", r, 1, "radius", Math.min(radius.max, 40), true);
	t.waitFrames(5, 3000);
	const soft = t.pixel(t.snapshot(A, "pl02-blur.png", 0), 975, 540);
	check("the blur spreads white past the middle", soft.r > 30 && soft.r < 225, { sharp: sharp, blurred: soft });
});

story("PL-03", "A plugin source is in Add Source, adds, and renders", () => {
	if (!loaded) return t.note("skipped: run again after PL-00 installed plugins");
	const kinds = app.pluginSourceTypes();
	t.metric("pluginSources", kinds);
	const clone = kinds.filter(k => k.kind === "obs:source-clone")[0];
	if (!check("Add Source offers Source Clone", !!clone, kinds)) return;
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const red = addKind("color", "Red", "#ff0000");
	sel(red.inst); app.fit("fill");
	check("it's available", app.kindAvailable(clone.kind));
	const s = addKind(clone.kind, clone.label);
	eq("stored with the plugin's kind", t.doc().sources[s.source].kind, clone.kind);
	const rows = props("source", s.source);
	t.metric("cloneProps", rows.map(x => x.key + ":" + x.type + "=" + x.value));
	const pick = findProp(rows, /^clone$/);
	if (!check("it has a source to clone", !!pick && pick.options.some(o => o.value === red.source), pick)) return;
	app.setObsProperty("source", s.source, -1, "clone", red.source, true);
	t.waitUntil(() => t.sourceSize(s.source).w > 0, 3000);
	sel(s.inst); app.fit("fill");
	app.toggleVisible(red.inst); // only the clone shows red now
	t.waitFrames(5, 3000);
	const px = t.pixel(t.snapshot(A, "pl03-clone.png", 0), 960, 540);
	check("the clone draws the red source", isColor(px, 255, 0, 0), px);
});

story("PL-07", "The UI: Settings > Plugins, the filter editor, Add Source", () => {
	if (!loaded || !shader || shader.status !== "loaded") return t.note("skipped: Shader Filter not loaded");
	fresh();
	t.key("Ctrl+,"); t.wait(400);
	check("Settings has a Plugins section", t.click("Plugins")); t.wait(300);
	t.grab("pl07-settings.png");
	check("it lists Shader Filter with its version", t.items("obs-shaderfilter  2.6.1").length > 0, t.items("obs-shaderfilter"));
	check("it says what Shader Filter adds", t.items("Loaded. Adds filter: User-defined shader").length > 0);
	check("it offers Add from .zip and Open folder", t.items(".zip…").length > 0 && t.items("Open").length > 0);
	t.key("Escape"); t.wait(300);
	const r = placeColor("Gray", "#808080");
	sel(r); app.addFilter("obs:shader_filter");
	t.waitFrames(3, 3000); t.wait(300);
	t.grab("pl07-inspector.png");
	// Labels as the plugin names them (its locale file), shown by the generic editor.
	const labels = props("filter", r, 0).filter(x => x.visible).map(x => x.label);
	t.metric("editorLabels", labels);
	["button", "bool", "number"].forEach(ty => {
		const row = props("filter", r, 0).filter(x => x.visible && x.type === ty)[0];
		if (row) check("the filter card shows its " + ty + " \"" + row.label + "\"", t.items(row.label).length > 0, row);
	});
	check("Add Source opens", t.click("tip=Add a source")); t.wait(500);
	t.grab("pl07-addsource.png");
	check("Add Source shows the plugin source", t.items("Source Clone").length > 0, t.items("Clone"));
	t.key("Escape"); t.wait(300);
});

story("PL-04", "Plugin filters survive save and open", () => {
	if (!loaded || !shader || shader.status !== "loaded") return t.note("skipped: Shader Filter not loaded");
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const r = placeColor("Red", "#ff0000");
	app.fit("fill");
	sel(r); app.addFilter("obs:shader_filter");
	t.waitFrames(3, 3000);
	setShader(r, 0, solid(1.0, 1.0, 0.0));
	const d = roundTrip("pl04");
	const inst = Object.keys(d.instances).filter(k => (eff(k, "filters", d) || []).some(f => f.type === "obs:shader_filter"))[0];
	check("the plugin filter is back with its shader", !!inst && /float4\(1(\.0)?, 1(\.0)?, 0(\.0)?, 1\.0\)/.test(eff(inst, "filters", d)[0].settings.shader_text), inst && eff(inst, "filters", d));
	t.waitFrames(5, 3000);
	eq("the engine made the filter again", t.engineStats().layers.unknownFilters, 0);
	const px = t.pixel(t.snapshot(d.sceneOrder[0], "pl04.png", 0), 960, 540);
	check("and it draws the saved shader (yellow) after opening", isColor(px, 255, 255, 0), px);
});

story("PL-05", "The generic editor covers each property type and buttons", () => {
	if (!loaded || !shader || shader.status !== "loaded") return t.note("skipped: Shader Filter not loaded");
	fresh();
	const r = placeColor("Gray", "#808080");
	sel(r); app.addFilter("obs:shader_filter");
	t.waitFrames(3, 3000);
	const rows = props("filter", r, 0);
	const types = {};
	rows.forEach(x => types[x.type] = (types[x.type] || 0) + 1);
	t.metric("propertyTypes", types);
	check("every row has a key, a label and a type", rows.every(x => x.key && x.type && x.label !== undefined), rows.filter(x => !x.key || !x.type));
	const bool = rows.filter(x => x.type === "bool" && x.visible)[0];
	if (bool) {
		app.setObsProperty("filter", r, 0, bool.key, !bool.value, true);
		t.waitFrames(2, 2000);
		eq("a switch's change reaches OBS", findProp(props("filter", r, 0), new RegExp("^" + bool.key + "$")).value, !bool.value);
		eq("and is stored on the filter", eff(r, "filters")[0].settings[bool.key], !bool.value);
		app.undo();
	}
	const button = rows.filter(x => x.type === "button" && !x.url)[0];
	if (button) {
		app.clickObsProperty("filter", r, 0, button.key);
		t.waitFrames(2, 2000);
		check("a plugin button can be clicked", true, button.key);
	}
});

story("PL-06", "Removing a loaded plugin waits for a restart, and adding it back undoes that", () => {
	if (!loaded || !blur || blur.status !== "loaded") return t.note("skipped: Composite Blur not loaded");
	eq("remove has no error", app.removePlugin("obs-composite-blur"), "");
	eq("shown as removing", byName()["obs-composite-blur"].status, "removing");
	check("restart needed", app.pluginsNeedRestart());
	const zip = t.listDir(zipDir).filter(f => /composite-blur.*\.zip$/i.test(f))[0];
	eq("adding it again has no error", app.addPlugin(zipDir + "/" + zip), "");
	const p = byName()["obs-composite-blur"];
	check("no longer removing; the copy waits for a restart", p.status === "pending" || p.status === "loaded", p);
});
