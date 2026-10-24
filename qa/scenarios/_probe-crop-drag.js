// Probe: crop by dragging a layer's edge on the canvas. The edge you drag is the one that moves,
// plain or mirrored. Image: 400x200, left half red, right half blue (make it with qa/tools/make_halves.ps1).
t.include("_lib.js");
const img = t.scriptDir() + "/../runs/fixtures/halves.png";
function red(p) { return p.r > 180 && p.b < 80; }
function blue(p) { return p.b > 180 && p.r < 80; }

function trial(label, flip) {
	fresh();
	const k = addKind("image", "Halves", img);
	sel(k.inst);
	app.setTransform("x", 100); app.setTransform("y", 100); app.setTransform("w", 400); app.setTransform("h", 200);
	if (flip) app.setTransform("flipH", 1);
	t.wait(800);
	app.cropMode = true;
	t.wait(200);
	t.canvasDrag(100, 200, 200, 200, 10); // left edge (on screen) inward by 100
	t.wait(800);
	const i = t.doc().instances[k.inst];
	const c = eff(k.inst, "crop"), tr = eff(k.inst, "transform");
	t.note(label + ": crop " + JSON.stringify(c) + " transform " + JSON.stringify(tr));
	check(label + ": the layer's left edge moved in", Math.abs(tr.x - 200) <= 2 && Math.abs(tr.w - 300) <= 2, "x=" + tr.x + " w=" + tr.w);
	const p = t.snapshot("", "crop-" + (flip ? "flip" : "plain") + ".png", 0);
	const at = x => t.pixel(p, x, 200);
	// What should still show: plain, red then blue; mirrored, blue then red.
	const near = at(215), far = at(485);
	t.note(label + ": pixel near the cropped edge " + JSON.stringify(near) + ", at the far edge " + JSON.stringify(far));
	check(label + ": the far edge still shows its half", flip ? red(far) : blue(far));
	// 100 px came off the half that was on the left on screen: plain, red is now 200..300 and blue
	// 300..500; mirrored, blue is 200..300 and red 300..500.
	const mid = at(320);
	t.note(label + ": pixel at 320 " + JSON.stringify(mid));
	check(label + ": the crop came off the edge that was dragged", flip ? red(mid) : blue(mid));
	check(label + ": nothing draws where the left edge was", !red(at(150)) && !blue(at(150)));
	app.cropMode = false;
}

story("CR-drag", "Crop by dragging the left edge on the canvas", () => {
	skipSetup();
	trial("plain", false);
	trial("mirrored", true);
});
