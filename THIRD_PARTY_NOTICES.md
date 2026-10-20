# Third-party notices

LOBS is free software under the GNU General Public License, version 2 or later
(see `LICENSE`). It includes or links the following:

| Component | License | Source |
|---|---|---|
| OBS Studio (libobs and the bundled engine: obs.dll, plugins, obs-browser/CEF) | GPL-2.0-or-later | https://github.com/obsproject/obs-studio |
| Qt 6.11 | LGPL-3.0 | https://www.qt.io/download-open-source |
| KDDockWidgets 2.4.1 (with LOBS's QML style patches in `native/deps/kddw-patches`) | GPL-2.0 or GPL-3.0 | https://github.com/KDAB/KDDockWidgets |
| Chromium Embedded Framework (inside obs-browser) | BSD-3-Clause | https://bitbucket.org/chromiumembedded/cef |
| Inter typeface | SIL Open Font License 1.1 | https://rsms.me/inter/ |
| Lucide icons | ISC | https://lucide.dev |
| Microsoft Visual C++ Redistributable (installer only) | Microsoft license | https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist |

The bundled OBS engine is OBS Studio's official release build, unmodified. The Qt
libraries are dynamically linked and can be replaced with a compatible build.
