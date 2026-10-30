// Probe: switching from OBS in one step (P1-5), against this PC's real OBS settings (read only:
// OBS's files are never written). QA's AppData starts empty, so nothing of yours changes.
t.include("_lib.js");
skipSetup();

const folder = "C:/Users/Logan/AppData/Roaming/obs-studio";

story("OS-01", "Finds OBS's profiles, scene collections and the ones used last", () => {
	const o = app.obsSetup(folder);
	t.metric("scan", { found: o.found, profiles: (o.profiles || []).map(p => p.name), collections: (o.collections || []).map(c => c.name),
		lastProfile: o.lastProfile, lastCollection: o.lastCollection, plugins: (o.plugins || []).map(p => p.name) });
	check("found", o.found === true);
	check("lists profiles", (o.profiles || []).length > 0);
	check("lists scene collections", (o.collections || []).length > 0);
	check("knows the profile used last", !!o.lastProfile, o.lastProfile);
	check("knows the collection used last", !!o.lastCollection, o.lastCollection);
	check("doesn't offer LOBS's own OBS plugin", !(o.plugins || []).some(p => p.name === "lobs"));
});

story("OS-02", "One import brings over the profile's settings and the scenes", () => {
	const o = app.obsSetup(folder);
	const scenesBefore = t.doc().sceneOrder.length;
	const r = app.importFromObs(o.lastProfile, o.lastCollection, []);
	t.metric("result", r);
	const st = app.settings;
	t.metric("settings", { service: st.service, hasKey: st.hasKey, w: st.outWidth, h: st.outHeight, fps: st.fps, bitrate: st.bitrate,
		audio: st.audioBitrate, encoder: st.encoder, enhanced: st.enhanced, recordFolder: st.recordFolder, recordFormat: st.recordFormat });
	check("reports what came over", (r.done || []).length >= 2, r.done);
	eq("service", st.service, "Twitch");
	check("stream key came over", st.hasKey === true);
	eq("canvas", [st.outWidth, st.outHeight, st.fps], [1920, 1080, 60]);
	eq("bitrate", st.bitrate, 6000);
	eq("encoder", st.encoder, "obs_nvenc_h264_tex");
	eq("recording format", st.recordFormat, "mp4");
	t.waitFrames(4, 3000);
	check("the scene collection replaced the show", t.doc().sceneOrder.length >= 1 && (t.doc().sceneOrder.length !== scenesBefore || r.done.some(d => d.indexOf("Scenes from") === 0)), [scenesBefore, t.doc().sceneOrder.length]);
});
