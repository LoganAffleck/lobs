// Video filters (OBS parity): FX-01 catalog, FX-02 what each key/correction filter draws,
// FX-03 inspector controls, FX-04 OBS import of every catalog filter.
t.include("_lib.js");
skipSetup();

function pixelAt(sceneId, name, x, y) { return t.pixel(t.snapshot(sceneId, name, 0), x, y); }
function isColor(px, r, g, b) { return px.ok && Math.abs(px.r - r) < 30 && Math.abs(px.g - g) < 30 && Math.abs(px.b - b) < 30; }
function filtersOf(id) { return eff(id, "filters"); }

story("FX-01", "Every filter in the Add filter menu is a real OBS filter on the layer", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const types = app.filterTypes().map(o => o.value);
	t.metric("types", types);
	["color", "chroma_key", "color_key", "luma_key", "mask", "sharpness", "scroll", "scale", "lut", "render_delay"].forEach(ty =>
		check("menu offers " + ty, types.indexOf(ty) >= 0, types));
	const r = placeColor("All filters", "#808080");
	t.waitFrames(2, 2000);
	const before = t.engineStats().layers.filters;
	types.forEach(ty => { sel(r); app.addFilter(ty); });
	t.waitFrames(3, 3000);
	const st = t.engineStats().layers;
	t.metric("layerStats", st);
	eq("the layer stores one filter per type", filtersOf(r).map(f => f.type), types);
	eq("the engine made one OBS filter per type", st.filters - before, types.length);
	eq("every OBS filter id is registered by a loaded module", st.unknownFilters, 0);
	// Each new filter starts from OBS's defaults.
	const ck = filtersOf(r).filter(f => f.type === "chroma_key")[0].settings;
	eq("Chroma Key defaults match OBS", [ck.key_color_type, ck.similarity, ck.smoothness, ck.spill, ck.opacity], ["green", 400, 80, 100, 1]);
	const sc = filtersOf(r).filter(f => f.type === "scroll")[0].settings;
	eq("Scroll defaults match OBS", [sc.speed_x, sc.limit_cx, sc.cx, sc.loop], [0, false, 100, true]);
	app.undo();
	eq("adding a filter is one undo step", filtersOf(r).length, types.length - 1);
});

story("FX-02", "Key and color filters change what the layer draws", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A); app.take();
	const under = placeColor("Under", "#ff0000");
	setT(under, "x", 0); setT(under, "y", 0); setT(under, "w", 800); setT(under, "h", 600);
	// Puts a fresh layer of `color` on top, adds one filter and applies `settings`; returns the pixel inside it.
	let n = 0;
	function filtered(color, type, settings) {
		const r = placeColor("Top " + (++n), color);
		setT(r, "x", 100); setT(r, "y", 100); setT(r, "w", 400); setT(r, "h", 300);
		sel(r); app.addFilter(type);
		Object.keys(settings || {}).forEach(k => { sel(r); app.setFilterValue(0, k, settings[k], true); });
		t.waitFrames(3, 3000);
		const px = pixelAt(A, "fx02-" + n + ".png", 300, 250);
		return { id: r, px: px };
	}
	const hide = id => { sel(id); app.toggleVisible(id); };
	let x = filtered("#00ff00", "color_key", {});
	check("Color Key (green preset) keys out a green layer", isColor(x.px, 255, 0, 0), x.px);
	sel(x.id); app.toggleFilter(0); t.waitFrames(3, 3000);
	const off = pixelAt(A, "fx02-off.png", 300, 250);
	check("turning the filter off brings the green back", isColor(off, 0, 255, 0), off);
	hide(x.id);
	x = filtered("#0000ff", "chroma_key", { key_color_type: "blue" });
	check("Chroma Key blue preset keys out a blue layer", isColor(x.px, 255, 0, 0), x.px);
	hide(x.id);
	x = filtered("#00ffff", "chroma_key", { key_color_type: "custom", key_color: "#00ffff" });
	check("Chroma Key custom color keys out that color", isColor(x.px, 255, 0, 0), x.px);
	hide(x.id);
	x = filtered("#ffffff", "luma_key", { luma_max: 0.5 });
	check("Luma Key removes pixels brighter than Luma max", isColor(x.px, 255, 0, 0), x.px);
	hide(x.id);
	x = filtered("#ffffff", "color", { color_multiply: "#0000ff" });
	check("Color Correction's color multiply tints the layer", isColor(x.px, 0, 0, 255), x.px);
	sel(x.id); app.setFilterValue(0, "opacity", 0, true); t.waitFrames(3, 3000);
	const clear = pixelAt(A, "fx02-opacity.png", 300, 250);
	check("Color Correction's opacity 0 shows what's beneath", isColor(clear, 255, 0, 0), clear);
	hide(x.id);
	x = filtered("#808080", "color", { color_add: "#ff0000" });
	check("Color Correction's color add brightens red", x.px.ok && x.px.r > 200 && x.px.g < 160, x.px);
});

story("FX-03", "Inspector shows each filter's settings, with dependent ones only when they apply", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const r = placeColor("Keyed", "#00ff00");
	sel(r); app.addFilter("chroma_key");
	const param = key => app.inspector.filters[0].params.filter(p => p.key === key)[0];
	eq("Key color is a choice", param("key_color_type").kind, "choice");
	check("the custom color is hidden for the green preset", !param("key_color").visible);
	sel(r); app.setFilterValue(0, "key_color_type", "custom", true);
	check("and shown for a custom color", param("key_color").visible);
	eq("opacity is shown as a percent", [param("opacity").scale, param("opacity").suffix], [100, "%"]);
	sel(r); app.addFilter("scale");
	const sp = key => app.inspector.filters[1].params.filter(p => p.key === key)[0];
	check("Undistort shows for bicubic", sp("undistort").visible);
	sel(r); app.setFilterValue(1, "sampling", "point", true);
	check("and hides for point sampling", !sp("undistort").visible);
	sel(r); app.setFilterValue(1, "resolution", "canvas", true);
	t.waitFrames(3, 3000);
	eq("Scaling to the canvas size still makes a registered filter", t.engineStats().layers.unknownFilters, 0);
});

story("FX-04", "OBS import brings every catalog filter over with its settings", () => {
	const filters = [
		{ id: "color_filter_v2", name: "CC", settings: { color_multiply: 0xFF0000FF, saturation: 0.5 } },
		{ id: "chroma_key_filter", name: "Old chroma", settings: { opacity: 50, similarity: 300 } },
		{ id: "color_key_filter_v2", name: "CK", settings: { key_color_type: "red", similarity: 200 } },
		{ id: "luma_key_filter_v2", name: "LK", settings: { luma_max: 0.7 } },
		{ id: "mask_filter_v2", name: "Mask", settings: { type: "blend_mul_filter.effect", image_path: "C:/x.png" } },
		{ id: "scroll_filter", name: "Scroll", settings: { speed_x: 50 } },
		{ id: "scale_filter", name: "Scale", settings: { resolution: "Base (Canvas) Resolution", sampling: "lanczos" } },
		{ id: "gpu_delay", name: "Delay", settings: { delay_ms: 100 } },
	];
	const coll = {
		name: "Filters", current_scene: "Scene", current_program_scene: "Scene", scene_order: [{ name: "Scene" }],
		sources: [
			{ id: "scene", name: "Scene", settings: { items: [{ name: "Box", visible: true, pos: { x: 0, y: 0 }, scale: { x: 1, y: 1 } }] } },
			{ id: "color_source_v3", name: "Box", settings: { color: 0xFF00FF00, width: 400, height: 300 }, filters: filters },
		],
	};
	const path = t.tempDir() + "/fx04-filters.json";
	check("fixture written", t.writeFile(path, JSON.stringify(coll)), path);
	fresh();
	check("import succeeds", app.importObs(path), app.toast);
	t.wait(1500);
	const d = t.doc();
	const box = Object.keys(d.instances).filter(i => d.instances[i].name === "Box")[0];
	check("the layer arrived", !!box);
	const fs = filtersOf(box);
	t.metric("imported", fs);
	eq("all eight filters, in order", fs.map(f => f.type), ["color", "chroma_key", "color_key", "luma_key", "mask", "scroll", "scale", "render_delay"]);
	const by = ty => fs.filter(f => f.type === ty)[0].settings;
	eq("colors become hex", by("color").color_multiply, "#ff0000");
	eq("unset OBS settings take OBS's defaults", [by("color").opacity, by("chroma_key").key_color_type, by("scroll").loop], [1, "green", true]);
	eq("a v1 filter's opacity 0..100 becomes 0..1", by("chroma_key").opacity, 0.5);
	eq("Color Key keeps its preset", [by("color_key").key_color_type, by("color_key").similarity], ["red", 200]);
	eq("Luma, mask, scroll and delay keep their values",
		[by("luma_key").luma_max, by("mask").type, by("mask").image_path, by("scroll").speed_x, by("render_delay").delay_ms],
		[0.7, "blend_mul_filter.effect", "C:/x.png", 50, 100]);
	eq("Scaling's localized canvas choice becomes 'canvas'", [by("scale").resolution, by("scale").sampling], ["canvas", "lanczos"]);
	check("none of them is reported as missing controls", !/has no LOBS controls/.test(app.toast), app.toast);
	t.removeFile(path);
});

// LOBS-039: filter cards and their controls are reused between layers, not rebuilt. A reused
// card must show the new layer's filter and settings, and nothing from the last one.
story("FX-05u", "Moving between layers shows each layer's own filter cards and settings", () => {
	fresh();
	app.editScene(t.doc().sceneOrder[0]);
	const two = placeColor("Two filters", "#ff0000");
	sel(two); app.addFilter("color"); app.addFilter("sharpness");
	const one = placeColor("One filter", "#00ff00");
	sel(one); app.addFilter("chroma_key");
	const none = placeColor("No filters", "#0000ff");
	const shows = label => t.items(label).some(i => i.text === label);
	const seen = () => ({ cc: shows("Color Correction"), sharpen: shows("Sharpen"), chroma: shows("Chroma Key"),
	                      gamma: shows("Gamma"), similarity: shows("Similarity"), multiply: shows("Color multiply") });
	sel(two); t.wait(200);
	eq("two filters: Color Correction and Sharpen", seen(), { cc: true, sharpen: true, chroma: false, gamma: true, similarity: false, multiply: true });
	sel(one); t.wait(200);
	eq("then one filter: only Chroma Key and its settings", seen(), { cc: false, sharpen: false, chroma: true, gamma: true, similarity: true, multiply: false });
	sel(none); t.wait(200);
	eq("then none: no cards", seen(), { cc: false, sharpen: false, chroma: false, gamma: false, similarity: false, multiply: false });
	sel(two); t.wait(200);
	eq("and back: both cards again", seen(), { cc: true, sharpen: true, chroma: false, gamma: true, similarity: false, multiply: true });
	app.setFilterValue(1, "amount", 0.5, true); t.wait(200);
	check("a reused card's slider shows the new value", t.items("0.50").length > 0, t.items("0.50").map(i => i.cls));
});

// FColorField builds its picker on first open (LOBS-039): it must still open, apply a color
// typed in its hex box and close, and open again later.
story("FX-06u", "A color field's picker opens, applies a typed color, and opens again", () => {
	fresh();
	app.editScene(t.doc().sceneOrder[0]);
	const r = placeColor("Picked", "#123456");
	sel(r); t.wait(200);
	const src = () => t.doc().sources[t.doc().instances[r].sourceId].settings.color;
	check("the color field shows the layer's color", t.items("#123456").length > 0, t.items("#123456").map(i => i.cls));
	t.click("#123456"); t.wait(300);
	check("clicking it opens the picker", t.items("Swatches").length > 0);
	const hex = t.items("#123456").filter(i => /Text(Field|Input)|FInput/.test(i.cls))[0];
	check("the picker has a hex box with the color", !!hex, t.items("#123456").map(i => i.cls));
	if (!hex) return;
	t.clickAt(hex.x + hex.w / 2, hex.y + hex.h / 2); t.key("Ctrl+A"); t.typeText("#abcdef"); t.key("Return"); t.wait(300);
	eq("Enter in the hex box applies the color", src(), "#abcdef");
	check("and closes the picker", t.items("Swatches").length === 0);
	sel(placeColor("Other", "#00ff00")); t.wait(200); sel(r); t.wait(200);
	t.click("#ABCDEF"); t.wait(300);
	check("it opens again on a later visit", t.items("Swatches").length > 0);
	t.key("Escape"); t.wait(200);
});
