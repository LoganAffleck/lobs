// PF-01, PF-02 on the stress project (must stay usable). Capped: 15 switches.
t.include("_lib.js");
t.include("_perf.js");
skipSetup();
runPerf("stress", { open: 10000, save: 2000, hitch: 2000, ui: 250, frame95: 33, frame99: 66 }, { only: ["PF-01", "PF-02"], switches: 15, timeoutS: 420 });
