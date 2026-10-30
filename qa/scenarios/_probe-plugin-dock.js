// Probe: a plugin that adds a dock through OBS's interface API (P2-6), Downstream Keyer by
// Exeldro. Run twice with one plugins folder (LOBS_PLUGINS_DIR): the first run installs it,
// the second checks it loaded, added its panel, and survives saving and exit.
t.include("_lib.js");
skipSetup();

const zip = "C:/Users/Logan/AppData/Local/Temp/claude/C--Users-Logan-Documents-LOBS/5b050d8f-0130-4159-8125-3571b8c2aa38/scratchpad/dsk-dl/downstream-keyer-0.4.5-windows.zip";
const plugin = () => app.plugins().filter(p => p.name === "downstream-keyer")[0];
const installed = !!plugin();

story("PD-00", "Downstream Keyer is installed", () => {
	if (installed) return t.metric("plugin", plugin());
	const err = app.addPlugin(zip);
	check("adds the zip", err === "", err);
	t.note("Installed; run again (plugins load at start).");
});

story("PD-01", "It loads and its panel appears in Plugin panels", () => {
	if (!installed) return t.note("skipped: run again");
	const p = plugin();
	t.metric("plugin", p);
	check("loaded", p.status === "loaded", p.status + ": " + p.reason);
	t.wait(1500); // FINISHED_LOADING has gone out
	check("it added a panel", app.hasPluginPanels());
	t.metric("tools", app.pluginTools());
	app.showPluginPanels();
	t.wait(800);
	t.grab("pd01-panels.png");
	// Scene changes reach it: switch scenes while it listens.
	const B = mkScene("Second");
	app.editScene(B); app.take(); t.wait(500);
	check("still running after a scene change", app.hasPluginPanels());
});
