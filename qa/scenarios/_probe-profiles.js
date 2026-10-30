// Probe: profiles (P2-4), sets of stream, video and recording settings you switch between.
t.include("_lib.js");
skipSetup();

story("PR-01", "Each profile keeps its own stream, video and recording settings", () => {
	app.saveSettings({ service: "Twitch", bitrate: 6000, fps: 60, recordFormat: "mp4", liveLock: true });
	const first = app.settings.profile;
	eq("starts on one profile", app.profiles(), [first]);
	eq("new profile", app.newProfile("YouTube"), "");
	eq("switched to it", app.settings.profile, "YouTube");
	eq("a copy of the one it came from", app.settings.bitrate, 6000);
	app.saveSettings({ service: "YouTube - RTMPS", bitrate: 9000, fps: 30, recordFormat: "mkv", liveLock: false });
	t.waitFrames(2, 2000);
	eq("both listed", app.profiles().slice().sort(), [first, "YouTube"].sort());
	eq("switch back", app.switchProfile(first), "");
	eq("first profile's service", app.settings.service, "Twitch");
	eq("first profile's bitrate", app.settings.bitrate, 6000);
	eq("first profile's fps", app.settings.fps, 60);
	eq("first profile's recording format", app.settings.recordFormat, "mp4");
	eq("live lock is shared, not per profile", app.settings.liveLock, false);
	eq("switch to YouTube again", app.switchProfile("YouTube"), "");
	eq("YouTube kept its settings", [app.settings.service, app.settings.bitrate, app.settings.fps, app.settings.recordFormat], ["YouTube - RTMPS", 9000, 30, "mkv"]);
	eq("rename the current one", app.renameProfile("YouTube", "YouTube main"), "");
	eq("renamed", app.settings.profile, "YouTube main");
	check("the old name is gone", app.profiles().indexOf("YouTube") < 0, app.profiles());
	check("can't delete the one in use", app.deleteProfile("YouTube main") !== "");
	eq("delete the other", app.deleteProfile(first), "");
	eq("one left", app.profiles(), ["YouTube main"]);
	check("names must be unique", app.newProfile("youtube MAIN") !== "");
});
