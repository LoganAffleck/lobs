// Probe: the in-scene preview stays inside its box (above the graphic tools) at any window size.
// A red layer fills the canvas; nothing red may show below the box or behind the tools.
t.include("_lib.js");
function at(path, x, y) { const ws = t.windowSize(); return t.pixel(path, Math.round(x * ws.dpr), Math.round(y * ws.dpr)); }
function isRed(p) { return p.ok && p.r > 200 && p.g < 50 && p.b < 50; }

story("PF-01", "The canvas stays above the tools at several window sizes", () => {
	skipSetup();
	fresh();
	const k = placeColor("Red", "#ff0000");
	sel(k); app.fit("fill"); app.clearSelection();
	t.wait(800);
	for (const [w, h] of [[1600, 1000], [1280, 800], [1100, 900], [1400, 760], [1600, 1000]]) {
		t.resizeWindow(w, h); t.wait(800);
		const box = t.items("objectName=preview")[0], pv = t.items("objectName=previewItem")[0];
		const tip = t.items("tip=Pencil")[0];
		const shot = t.grab("pf01-" + w + "x" + h + ".png");
		const cx = box.x + box.w / 2, below = box.y + box.h + 2;
		t.note(w + "x" + h + " box " + JSON.stringify(box) + " item " + JSON.stringify(pv) + " tools y " + (tip ? tip.y : "?"));
		check(w + "x" + h + ": item matches its box", pv && Math.abs(pv.w - box.w) <= 1 && Math.abs(pv.h - box.h) <= 1,
			pv ? pv.w + "x" + pv.h + " vs " + box.w + "x" + box.h : "no previewItem");
		check(w + "x" + h + ": red inside the box", isRed(at(shot, cx, box.y + box.h / 2)));
		check(w + "x" + h + ": nothing red below the box", !isRed(at(shot, cx, below)) && !(tip && isRed(at(shot, cx, tip.y + tip.h / 2))));
	}
});
