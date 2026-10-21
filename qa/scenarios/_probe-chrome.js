// Fix-agent check: the custom title bar. Keeps the window up so its real frame can be captured.
t.include("_lib.js");
skipSetup();
t.setTimeout(120);
story("probe-chrome", "custom title bar", () => {
	fresh(); placeColor("Bg", "#1f3b5a"); t.wait(500);
	t.grab("chrome-qml.png");
	t.writeFile(t.out("ready.txt"), "up");
	t.wait(12000);
	check("probe ran", true);
});
