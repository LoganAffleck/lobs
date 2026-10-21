// One-off: build a row of four panels under the Video panel by dragging tabs.
t.include("_lib.js");
skipSetup();
t.setTimeout(180);
function at(text, nth) { const r = t.items(text)[nth || 0]; return r ? { x: r.x + r.w / 2, y: r.y + r.h / 2 } : null; }
story("probe-dock2", "a row of four panels under the video", () => {
	t.wait(1200);
	t.click("Window"); t.wait(300); t.click("Captions"); t.wait(600);
	t.click("Window"); t.wait(300); t.click("Stats"); t.wait(600);
	const status = t.items("Engine running")[0];
	const video = at("Video");
	const bottomY = status.y - 40;
	// 1. Audio Mixer to the bottom of the Video panel.
	let m = at("Audio Mixer");
	t.dragAt(m.x, m.y, video.x + 300, bottomY, 30); t.wait(800);
	t.grab("row-1.png");
	// 2-4. Captions, Stats, History to the right of the last one in the row.
	["Captions", "Stats", "History"].forEach((name, k) => {
		const p = at(name), mix = at("Audio Mixer");
		if (!p || !mix) return;
		t.dragAt(p.x, p.y, mix.x + 140 + k * 180, mix.y + 120, 30); t.wait(800);
		t.grab("row-" + (k + 2) + ".png");
	});
	const prev = t.items("Fade · 300 ms")[0];
	t.note("after: video toolbar at " + JSON.stringify(prev));
	check("still running, and the video toolbar is still on screen", !!prev, prev);
});
