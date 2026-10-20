// PF-03..PF-07 on the stress project, fresh process. Capped: 200 edits.
t.include("_lib.js");
t.include("_perf.js");
skipSetup();
runPerf("stress", { open: 10000, save: 2000, hitch: 2000, ui: 250, frame95: 33, frame99: 66 }, { only: ["PF-03", "PF-04", "PF-05", "PF-06", "PF-07"], edits: 200, timeoutS: 420 });
