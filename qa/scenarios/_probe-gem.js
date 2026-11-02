// The gem: the Setup welcome, and the top bar's gem rolling as Settings opens and closes.
t.include("_lib.js");
story("GM-01", "Welcome screen and Settings roll", () => {
	t.wait(1500);
	check("welcome shows", t.items("Welcome to LOBS").length > 0);
	t.grab("welcome.png");
	t.wait(1300); t.grab("welcome-later.png");
	t.click("Skip setup"); t.wait(500);
	check("LOBS opens Settings", t.click("LOBS")); t.wait(200); t.grab("settings-rolling.png");
	t.wait(800); t.grab("settings-open.png");
	t.click("LOBS"); t.wait(200); t.grab("settings-closing.png");
});
