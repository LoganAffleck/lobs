# Phase 0 Baseline

Date: 2026-10-20 · LOBS v0.1.1 (commit a51310e) · Engine OBS 32.2.2

## Machine

| | |
|---|---|
| CPU | Intel Core i9-13900KF (24 cores / 32 threads) |
| GPU | NVIDIA GeForce RTX 3080 Ti, driver 32.0.16.1714 |
| RAM | 64 GB |
| Display | 1 × LG HDR 4K, 3840×2160 @ 59 Hz (single monitor) |
| OS | Windows 11 Home 10.0.26300 |
| Camera | EOS Webcam Utility (Canon virtual webcam driver) |
| Mic | Scarlett Solo USB (also: Realtek USB2.0, Steam Streaming, Virtual Audio Cable) |

Notes for later phases:
- The display runs at 59 Hz, so the 16.7 ms frame budget is effectively ~16.9 ms. Targets stay as written.
- The only camera is a virtual driver (EOS Webcam Utility). "One device = one capture" results may differ on a physical UVC webcam; flag this in coverage gaps.
- The machine is high-end. Passing here does not mean passing on mid-range hardware; flag this in coverage gaps.

## Build

`native\build.bat` → `BUILD OK` (incremental; no compile errors).

## Built-in autotest

`LOBS.exe --autotest`: exit 0 in 12 s. `autotest.json`:

| Field | Value |
|---|---|
| ok | true |
| scenes / sources / instances | 2 / 4 / 5 |
| canUndo | true |
| stats | 60 fps · 0.28 ms render |
| recording | 5.000 s, H.264 1920×1080 @ 60/1 + AAC, 124 KB |

**Baseline: PASS.** Caveat: the autotest only checks that recording succeeded and reports counts. It doesn't assert expected values (for example, it adds 2 sources but reports 4, presumably default audio devices). Phase 2 assertions must pin exact expectations.

## Findings during Phase 0
See [BUGS.md](BUGS.md): LOBS-001, LOBS-002, LOBS-003.
