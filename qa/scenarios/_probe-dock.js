// One-off: reproduce a crash when dropping a panel into a row of panels (user's layout).
t.include("_lib.js");
skipSetup();
t.setTimeout(120);
story("probe-dock", "drop Captions beside the mixer", () => {
	t.wait(1500);
	t.grab("dock-before.png");
	t.note("Captions: " + JSON.stringify(t.items("Captions")) + " Mixer: " + JSON.stringify(t.items("Audio mixer")));
	const to = t.readFile(t.out("../../fixtures/layouts/drop.txt"));
	if (to) {
		const [x0, y0, x1, y1] = to.trim().split(",").map(Number);
		t.note("dragging " + [x0, y0, x1, y1]);
		t.dragAt(x0, y0, x1, y1, 30);
		t.wait(1500);
		t.grab("dock-after.png");
	}
	check("still running after the drop", true);
});
