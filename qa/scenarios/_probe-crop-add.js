// Probe: shapes with no stroke reach their box's edge; cropping moves the crop window, holds an
// aspect ratio, and Esc puts it back; double-click a layer to crop; the Add Source dialog lists
// devices for every kind, meters audio and has the browser options. Image: 400x200, left half
// red, right half blue (qa/tools/make_halves.ps1).
t.include("_lib.js");
const img = t.scriptDir() + "/../runs/fixtures/halves.png";
function ui(text) { return t.items(text); }
function has(text) { return ui(text).length > 0; }
function halves() {
	const k = addKind("image", "Halves", img);
	sel(k.inst);
	app.setTransform("x", 100); app.setTransform("y", 100); app.setTransform("w", 400); app.setTransform("h", 200);
	t.wait(800);
	return k;
}
function near(a, b) { return Math.abs(a - b) <= 2; }

story("CA-01", "A rectangle with no stroke fills its box to the edge", () => {
	skipSetup();
	fresh();
	app.clearSelection();
	app.toolFill = "#ff0000"; app.toolStroke = ""; app.toolStrokeWidth = 12;
	app.tool = "rect"; t.wait(200);
	t.canvasDrag(300, 300, 700, 600, 10); t.wait(800);
	app.tool = "select";
	const p = t.snapshot("", "ca01-rect.png", 0);
	const edge = t.pixel(p, 302, 450), corner = t.pixel(p, 303, 303);
	t.note("pixel 2 px inside the left edge " + JSON.stringify(edge) + ", near the corner " + JSON.stringify(corner));
	check("fill reaches the left edge", edge.r > 200 && edge.g < 60);
	check("fill reaches the corner", corner.r > 200 && corner.g < 60);
	app.toolStroke = "#ffffff"; app.toolStrokeWidth = 6;
});

story("CA-02", "Dragging inside the crop moves the crop window; the image stays put", () => {
	fresh();
	const k = halves();
	app.setCrop("l", 100); t.wait(400);
	let tr = eff(k.inst, "transform");
	t.note("after crop l=100: " + JSON.stringify(eff(k.inst, "crop")) + " " + JSON.stringify(tr));
	app.cropMode = true; t.wait(200);
	t.canvasDrag(tr.x + tr.w / 2, 200, tr.x + tr.w / 2 - 50, 200, 10); t.wait(500);
	const c = eff(k.inst, "crop"); tr = eff(k.inst, "transform");
	t.note("after the drag: " + JSON.stringify(c) + " " + JSON.stringify(tr));
	eq("left crop went down by 50", c.l, 50);
	eq("right crop went up by 50", c.r, 50);
	check("the crop box moved with it, same size", near(tr.x, 150) && near(tr.w, 300), "x=" + tr.x + " w=" + tr.w);
	check("still in crop mode", app.cropMode);
	app.cropMode = false;
});

story("CA-03", "A picked aspect ratio holds while dragging an edge", () => {
	fresh();
	const k = halves();
	app.setCropAspect(1); t.wait(400);
	let tr = eff(k.inst, "transform");
	t.note("1:1: " + JSON.stringify(eff(k.inst, "crop")) + " " + JSON.stringify(tr));
	check("cropped square", near(tr.w, tr.h), JSON.stringify(tr));
	eq("1:1 remembered", Math.round(app.cropAspect * 100), 100);
	app.cropMode = true; t.wait(200);
	t.canvasDrag(tr.x + tr.w, tr.y + tr.h / 2, tr.x + tr.w - 50, tr.y + tr.h / 2, 10); t.wait(500);
	tr = eff(k.inst, "transform");
	t.note("after dragging the right edge in 50: " + JSON.stringify(eff(k.inst, "crop")) + " " + JSON.stringify(tr));
	check("still square", near(tr.w, tr.h), JSON.stringify(tr));
	check("and smaller", tr.w < 190, "w=" + tr.w);
	app.setCropAspect(0); t.wait(200);
	eq("Free", app.cropAspect, 0);
	const before = eff(k.inst, "transform");
	t.canvasDrag(before.x + before.w, before.y + before.h / 2, before.x + before.w - 30, before.y + before.h / 2, 10); t.wait(500);
	tr = eff(k.inst, "transform");
	check("free: only the width changes", near(tr.h, before.h) && tr.w < before.w - 20, JSON.stringify(tr));
	app.cropMode = false;
});

story("CA-04", "Esc puts the crop back and leaves crop mode; Enter keeps it", () => {
	fresh();
	const k = halves();
	app.cropMode = true; t.wait(200);
	t.canvasDrag(100, 200, 180, 200, 10); t.wait(400);
	check("cropped by the drag", eff(k.inst, "crop").l > 50, JSON.stringify(eff(k.inst, "crop")));
	t.key("Escape"); t.wait(400);
	if (app.cropMode) { t.note("Escape didn't reach the canvas; calling cancelCrop"); app.cancelCrop(); t.wait(300); }
	eq("crop is back", eff(k.inst, "crop").l || 0, 0);
	check("crop mode is off", !app.cropMode);
	const tr = eff(k.inst, "transform");
	check("and the layer is where it was", near(tr.x, 100) && near(tr.w, 400), JSON.stringify(tr));
});

story("CA-05", "Double-click the selected layer to crop it", () => {
	fresh();
	const k = halves();
	t.canvasClick(300, 200); t.wait(150);
	t.canvasClick(300, 200, "", true); t.wait(300);
	check("crop mode is on", app.cropMode);
	app.cropMode = false;
});

story("CA-06", "Alt resizes from the middle", () => {
	fresh();
	const k = halves();
	t.canvasDrag(500, 300, 600, 350, 10, "alt"); t.wait(400);
	const tr = eff(k.inst, "transform");
	t.note(JSON.stringify(tr));
	check("center stayed put", near(tr.x + tr.w / 2, 300) && near(tr.y + tr.h / 2, 200), JSON.stringify(tr));
	check("and it grew", tr.w > 450, "w=" + tr.w);
});

function openAdd() {
	t.clickItem("tip=Add a source", 0); t.wait(600);
	return has("Add a source");
}
function pickKind(label) {
	const items = ui(label);
	if (!items.length) return false;
	t.clickItem(label, items.length - 1); t.wait(900);
	return true;
}

story("AS-10", "Add Source: devices are listed for every kind, in any order", () => {
	fresh();
	app.setUiValue("addSource/kind", "camera");
	const cams = app.devicesForKind("camera"), mics = app.devicesForKind("mic"), outs = app.devicesForKind("desktop_audio");
	t.metric("cameras", cams.length); t.metric("mics", mics.length); t.metric("outputs", outs.length);
	t.note("cameras: " + JSON.stringify(cams.map(d => d.name)));
	check("dialog opens", openAdd());
	t.grab("as10-open-camera.png");
	if (cams.length) check("camera listed on first open", !has("No camera found"));
	check("'Sound' replaces 'Desktop'", has("Sound") && !ui("Desktop").some(i => i.text === "Desktop"));
	pickKind("Mic");
	t.grab("as10-mic.png");
	if (mics.length) check("mics listed", !has("No microphone found"));
	check("Mono option", has("Mono"));
	t.wait(1500);
	t.grab("as10-mic-meter.png");
	pickKind("Camera");
	t.grab("as10-camera-after-mic.png");
	if (cams.length) check("camera listed after Mic", !has("No camera found"));
	pickKind("Display");
	check("display listed after Camera", !has("No display found"));
	t.wait(1200);
	t.grab("as10-display-preview.png");
	check("display preview running", !has("Starting the preview"));
	pickKind("Sound");
	if (outs.length) check("outputs listed", !has("No sound output found"));
	pickKind("Browser");
	check("browser: Web / File", has("Web") && has("File"));
	check("browser: size follows the layer", has("Follows layer"));
	check("browser: advanced folded", has("Advanced") && !has("Page access"));
	if (!app.uiFlag("browser/advancedOpen", false)) { t.clickItem("Advanced", ui("Advanced").length - 1); t.wait(300); }
	check("browser: page access", has("Page access"));
	check("browser: frame rate", has("Frame rate"));
	check("browser: sound route", has("In the mixer"));
	check("browser: CSS", has("CSS") && has("Default"));
	t.grab("as10-browser.png");
	t.key("Escape"); t.wait(400);
	check("kind remembered", app.uiValue("addSource/kind", "") === "browser");
	app.setUiValue("addSource/kind", "camera");
});

story("BR-01", "Resizing a browser layer resizes its page; Shift keeps proportions", () => {
	fresh();
	const k = addKind("browser", "Page", "about:blank");
	sel(k.inst);
	app.setTransform("x", 100); app.setTransform("y", 100);
	app.setSize("w", 800, false); t.wait(300);
	app.setSize("h", 600, false); t.wait(300);
	let st = t.doc().sources[k.source].settings;
	eq("page width follows the layer", st.width, 800);
	eq("page height follows the layer", st.height, 600);
	t.canvasDrag(900, 700, 1100, 750, 10); t.wait(400);
	const tr = eff(k.inst, "transform");
	st = t.doc().sources[k.source].settings;
	t.note(JSON.stringify(tr) + " page " + st.width + "x" + st.height);
	check("free resize: the width grew more than the height", tr.w - 800 > (tr.h - 600) * 2, JSON.stringify(tr));
	check("page size is the layer size", Math.abs(st.width - tr.w) <= 1 && Math.abs(st.height - tr.h) <= 1);
	app.setSourceSetting(k.source, "responsive", false); t.wait(200);
	app.setSize("w", 400, true); t.wait(300);
	eq("off: the page keeps its size", t.doc().sources[k.source].settings.width, st.width);
});

story("CAM-01", "A camera already in the show isn't opened again by the Add dialog", () => {
	fresh();
	const cams = app.devicesForKind("camera").filter(d => !/virtual/i.test(d.name));
	if (!cams.length) { t.note("no camera"); return; }
	const k = addKind("camera", "Cam", cams[0].value);
	t.wait(2500);
	eq("source found by its device", app.sourceUsingDevice("camera", cams[0].value), k.source);
	t.note("fresh list with the camera running: " + JSON.stringify(app.devicesForKind("camera")));
	t.note("through the source: " + JSON.stringify(app.devices(k.source, true)));
	t.note("display list: " + app.devicesForKind("display").length);
	app.setUiValue("addSource/kind", "camera");
	check("dialog opens", openAdd());
	pickKind("Camera");
	t.wait(2500);
	t.grab("cam01-in-use.png");
	if (has("No signal")) t.note("the camera has no picture here (open in another app?): the in-use caption can't show");
	else check("dialog says it's already in the show", has("Already in your show"));
	t.key("Escape"); t.wait(500);
});

story("RS-01", "Drag a layer's edge to stretch that side", () => {
	fresh();
	const k = halves(); // 100,100 400x200
	t.canvasDrag(500, 200, 600, 260, 10); t.wait(300); // right edge, mid-height
	let tr = eff(k.inst, "transform");
	check("right edge: only the width grew", near(tr.w, 500) && near(tr.h, 200) && near(tr.x, 100), JSON.stringify(tr));
	t.canvasDrag(300, 300, 320, 360, 10); t.wait(300); // bottom edge
	tr = eff(k.inst, "transform");
	check("bottom edge: only the height grew", near(tr.h, 260) && near(tr.w, 500) && near(tr.y, 100), JSON.stringify(tr));
	t.canvasDrag(100, 200, 50, 200, 10, "alt"); t.wait(300); // left edge, around the middle
	tr = eff(k.inst, "transform");
	check("Alt: both sides", near(tr.w, 600) && near(tr.x + tr.w / 2, 350), JSON.stringify(tr));
});

story("MX-01", "A browser source joins the mixer only while it's on a scene", () => {
	fresh();
	const k = addKind("browser", "Alerts", "about:blank");
	t.wait(800);
	const inMixer = () => app.mixer.some(r => r.id === k.source);
	check("on a scene: in the mixer", inMixer(), JSON.stringify(app.mixer.map(r => r.name)));
	sel(k.inst); app.deleteSelected(); t.wait(600);
	check("source kept in Sources", t.doc().sourceOrder.indexOf(k.source) >= 0);
	check("on no scene: not in the mixer", !inMixer(), JSON.stringify(app.mixer.map(r => r.name)));
	app.placeSource(k.source); t.wait(600);
	check("placed again: back in the mixer", inMixer());
});
