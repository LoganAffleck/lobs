// Probe: LOBS exits cleanly with a browser source while it answers OBS's frontend API (the
// browser plugin listens to its events, EXIT included). run.ps1 reports a crash on exit.
t.include("_lib.js");
skipSetup();

story("EB-01", "A show with a browser source closes without a crash", () => {
	fresh();
	const b = addKind("browser", "Page", "https://example.com");
	check("browser source added", !!b && !!b.inst);
	t.wait(3000); // the page loads, so CEF is running
	app.toggleStreaming; // nothing: just keep the show as it is
});
