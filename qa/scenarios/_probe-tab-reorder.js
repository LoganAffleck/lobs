// Probe: dragging a tab along its strip puts it at the gap it's dropped in (not at the end or
// the front), with two tabs and with three.
t.include("_lib.js");
function ui(text) { return t.items(text); }
function tab(title) { return ui(title).filter(i => i.text === title && i.cls === "QQuickText" && i.h < 30).sort((a, b) => a.y - b.y)[0]; }
function order(titles) { return titles.map(n => ({ n: n, t: tab(n) })).filter(o => o.t).sort((a, b) => a.t.x - b.t.x).map(o => o.n); }

story("TB-01", "Reorder tabs in a group by dragging", () => {
	skipSetup();
	fresh(); t.wait(500);
	const s = tab("Sources"), m = tab("Audio mixer");
	check("Sources and Audio mixer tabs show", !!s && !!m, JSON.stringify([s, m]));
	if (!s || !m) return;
	eq("Sources first", JSON.stringify(order(["Sources", "Audio mixer"])), JSON.stringify(["Sources", "Audio mixer"]));
	t.dragAt(m.x + m.w / 2, m.y + m.h / 2, s.x + 4, s.y + s.h / 2, 16); t.wait(800);
	t.grab("tb01-mixer-first.png");
	eq("dropped left of Sources, Audio mixer comes first", JSON.stringify(order(["Sources", "Audio mixer"])), JSON.stringify(["Audio mixer", "Sources"]));
	// A third tab: Assets joins the group, then goes between the two.
	const a = tab("Assets"), mx = tab("Audio mixer");
	t.dragAt(a.x + a.w / 2, a.y + a.h / 2, mx.x + 4, mx.y + mx.h / 2, 16); t.wait(800);
	const three = order(["Sources", "Audio mixer", "Assets"]);
	t.note("after adding Assets: " + JSON.stringify(three));
	eq("Assets went where it was dropped: first", JSON.stringify(three), JSON.stringify(["Assets", "Audio mixer", "Sources"]));
	const as = tab("Assets"), src = tab("Sources");
	t.dragAt(as.x + as.w / 2, as.y + as.h / 2, src.x + 4, src.y + src.h / 2, 16); t.wait(800);
	t.grab("tb01-three.png");
	eq("moved one gap right: between the other two", JSON.stringify(order(["Sources", "Audio mixer", "Assets"])), JSON.stringify(["Audio mixer", "Assets", "Sources"]));
});
