// PF-01, PF-02 on the realistic heavy project (must meet targets).
t.include("_lib.js");
t.include("_perf.js");
skipSetup();
runPerf("heavy", { open: 3000, save: 500, hitch: 50, ui: 100, frame95: 16.7, frame99: 33 }, { only: ["PF-01", "PF-02"], switches: 30, timeoutS: 600 });
