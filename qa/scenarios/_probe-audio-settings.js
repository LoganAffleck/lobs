// Probe: Settings > Audio, and the LOBS button toggling Settings.
t.include("_lib.js");
skipSetup();
function openAudio() {
	t.clickItem("LOBS", 0); t.waitFrames(3, 2000);
	const nav = t.items("Audio"), k = nav.findIndex(i => i.text === "Audio" && i.x < 100);
	t.clickItem("Audio", k); t.waitFrames(3, 2000);
}
story("AU-S1", "Settings > Audio lists the global sources, or offers to add them", () => {
	fresh();
	openAudio();
	check("with none: says so", t.items("No mic or desktop audio").length === 1);
	t.clickItem("Add mic", 0); t.waitFrames(3, 2000);
	check("Add mic adds one", app.mixer.filter(m => m.global).length === 1);
	t.metric("after", [t.items("No desktop audio").map(i=>i.text), t.items("Add desktop audio").map(i=>i.text), t.items("No mic").map(i=>i.text), app.mixer.map(m=>[m.kind,m.global])]);
	check("then desktop audio is still offered", t.items("No desktop audio").length === 1 && t.items("Add desktop audio").length >= 1);
	t.clickItem("Add desktop audio", 0); t.waitFrames(3, 2000);
	check("with both: nothing offered", app.mixer.filter(m => m.global).length === 2 && t.items("No desktop audio").length === 0 && t.items("No mic").length === 0);
	check("its row shows", t.items("Levels, monitoring").length === 1 && t.items("Mic").length > 0);
	t.metric("lobs", t.items("LOBS").map(i => [i.cls, i.text, i.x, i.y]));
	t.metric("clicked", t.clickItem("LOBS", 0)); t.waitFrames(40, 3000);
	check("clicking LOBS again closes Settings", t.items("Levels, monitoring").length === 0);
});
