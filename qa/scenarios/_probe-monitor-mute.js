// Monitoring vs mute: "Mute monitoring when muted" silences a muted source in your headphones;
// without it you still hear it. Monitor only keeps a source out of every mix (OBS 33 has no
// monitor-only of its own).
t.include("_lib.js");
skipSetup();
t.setTimeout(300);

function obsOf(sid) { return t.obsInputs().find(i => i.name === sid) || {}; }

story("MM-01", "Mute monitoring when muted", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const k = addKind("mic", "Mic"), mic = k.source;
	app.setAudio(mic, "monitor", "monitor_and_output", true); t.wait(200);
	check("monitoring on", obsOf(mic).monitoring !== 0, obsOf(mic));
	app.setAudio(mic, "muted", true, true); t.wait(200);
	check("muted, option off: still monitored", obsOf(mic).monitoring !== 0, obsOf(mic));
	app.setAudio(mic, "monitorMutes", true, true); t.wait(200);
	check("option saved in the show", t.doc().sources[mic].audio.monitorMutes === true);
	check("muted, option on: not monitored", obsOf(mic).monitoring === 0, obsOf(mic));
	check("engine mute still on", t.inputMuted(mic));
	app.setAudio(mic, "muted", false, true); t.wait(200);
	check("unmuted: monitored again", obsOf(mic).monitoring !== 0, obsOf(mic));
	app.selectSource(mic); t.waitFrames(5, 2000); t.wait(300);
	check("the switch is in Audio settings", t.items("Mute monitoring when muted").length > 0);
	t.grab("mm01-audio.png");
	app.undo(); t.wait(200); app.undo(); t.wait(200);
	check("undo takes the option back", t.doc().sources[mic].audio.monitorMutes === false);
});

story("MM-02", "Monitor only stays out of every mix", () => {
	fresh();
	const A = t.doc().sceneOrder[0];
	app.editScene(A);
	const mic = addKind("mic", "Mic").source;
	app.setAudio(mic, "monitor", "monitor_only", true); t.wait(200);
	check("monitored", obsOf(mic).monitoring !== 0, obsOf(mic));
	check("in no mix", obsOf(mic).mixers === 0, obsOf(mic));
	app.setAudio(mic, "monitor", "off", true); t.wait(200);
	check("off: back in its mixes", obsOf(mic).mixers !== 0 && obsOf(mic).monitoring === 0, obsOf(mic));
});
