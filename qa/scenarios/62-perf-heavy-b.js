// PF-03..PF-07 on the heavy project, in a fresh process (so LOBS-022's switch leak can't skew them).
t.include("_lib.js");
t.include("_perf.js");
skipSetup();
runPerf("heavy", { open: 3000, save: 500, hitch: 50, ui: 100, frame95: 16.7, frame99: 33 }, { only: ["PF-03", "PF-04", "PF-05", "PF-06", "PF-07"], edits: 1000, timeoutS: 600 });
