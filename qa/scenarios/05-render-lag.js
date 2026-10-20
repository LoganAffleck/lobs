// PF-07 pre-study: is render lag real, and when does it happen? Samples libobs's cumulative
// lagged/rendered counters as per-second deltas: at startup, idle, and while the harness snapshots.

function sample(label, seconds, during) {
	const rows = [];
	let prev = t.engineStats().live;
	for (let i = 0; i < seconds; ++i) {
		if (during)
			during();
		t.wait(1000);
		const cur = t.engineStats().live;
		const r = cur.rendered - prev.rendered, l = cur.lagged - prev.lagged;
		rows.push({ rendered: r, lagged: l, pct: r ? Math.round(1000 * l / r) / 10 : 0 });
		prev = cur;
	}
	const lagged = rows.reduce((s, x) => s + x.lagged, 0), rendered = rows.reduce((s, x) => s + x.rendered, 0);
	t.metric(label, { seconds: seconds, rendered: rendered, lagged: lagged, pct: rendered ? Math.round(1000 * lagged / rendered) / 10 : 0, perSecond: rows });
	return { lagged: lagged, rendered: rendered };
}

story("PF-07a", "render lag: startup vs idle vs snapshot load", () => {
	const startup = t.engineStats().live;
	t.metric("sinceStart", startup);
	t.note("cumulative at scenario start: " + startup.lagged + " lagged of " + startup.rendered + " (" + (startup.laggedPct || 0).toFixed(1) + "%)");
	fresh();
	app.addSource("color", "Bg", "#203040", true);
	const idle = sample("idle", 20);
	check("idle near-empty scene: <1% lagged frames", idle.rendered && idle.lagged / idle.rendered < 0.01, idle);
	check("idle renders ~60 fps", idle.rendered / 20 > 57, idle.rendered / 20);
	const snap = sample("whileSnapshotting", 10, () => t.snapshot("", "lag.png", 0));
	t.note("snapshot-load lag: " + snap.lagged + "/" + snap.rendered + " (harness side effect if high)");
});
