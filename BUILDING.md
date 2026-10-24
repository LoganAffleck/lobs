# Building LOBS

## Windows

Requirements: Visual Studio 2026 (C++), CMake, Python with `ninja` (`pip install ninja`),
Qt 6.11.1 for MSVC 2022 x64 at `C:\Qt\6.11.1\msvc2022_64`, and Inno Setup 6 for the installer.

1. libobs headers:
   `git clone --depth 1 --branch 32.2.2 https://github.com/obsproject/obs-studio obs-plugin/deps/obs-studio`
2. KDDockWidgets:
   `git clone --depth 1 --branch v2.4.1 https://github.com/KDAB/KDDockWidgets native/deps/KDDockWidgets`
   then run `native\deps\build-kddw.bat`.
3. whisper.cpp (live captions):
   `git clone --depth 1 --branch v1.7.4 https://github.com/ggerganov/whisper.cpp native/deps/whisper.cpp`
   then run `native\deps\build-whisper.bat`. To run the captions test (CC-01) offline, download
   `ggml-base.bin` from https://huggingface.co/ggerganov/whisper.cpp, set `LOBS_WHISPER_MODEL` to its
   path, and make the speech clip with `qa\tools\make_speech.ps1`.
4. The OBS engine: download `OBS-Studio-32.2.2-Windows-x64.zip` from the official
   release into `native\deps\downloads\obs\`, and `vc_redist.x64.exe` into `native\deps\redist\`.
5. `native\build.bat` builds and deploys to `native\dist\LOBS`.
6. `native\installer\build-installer.bat` makes `native\dist\installer\LOBS-Setup-<version>.exe`.

## macOS

Apple Silicon, macOS 13 or later. Requirements: the Xcode Command Line Tools
(`xcode-select --install`), CMake and Ninja (`brew install cmake ninja`), and Qt 6.11.1 for macOS
at `~/Qt/6.11.1/macos` (the Qt online installer, or `aqt install-qt mac desktop 6.11.1 clang_64 -O ~/Qt`;
set `QTDIR` to use another location).

1. libobs headers, KDDockWidgets and whisper.cpp: clone them as in steps 1–3 above, plus SIMDe
   (libobs's SSE code goes through it on ARM):
   `git clone --depth 1 --branch v0.8.2 https://github.com/simd-everywhere/simde native/deps/simde`
2. `native/deps/build-kddw.sh` and `native/deps/build-whisper.sh`.
3. The OBS engine: download `OBS-Studio-32.2.2-macOS-Apple.dmg` from the official release into
   `native/deps/downloads/obs/`. `build.sh` stages it with `native/deps/stage-engine-mac.sh`
   (only the modules in `native/engine-modules-mac.txt`; OBS's Qt stays out, LOBS ships its own).
4. `native/build.sh` builds `native/dist/mac/LOBS.app`, with the engine inside it, and signs it
   ad hoc so it runs on this Mac.

The app writes `lobs.log` to `~/Library/Application Support/LOBS/LOBS`. Signed and notarized
builds, the updater and the virtual camera come later (see `PLAN-MACOS.md`).

## Code signing (Windows)

Unsigned installers get a SmartScreen warning. `build-installer.bat` signs LOBS's own files and the
installer when one of these is set (otherwise it builds unsigned and says so):

- **Azure Trusted Signing** (cheapest, about $10/month; SmartScreen trusts it quickly): set up an
  account and certificate profile in Azure, install the Trusted Signing client tools, sign in with
  `az login`, then set `LOBS_SIGN_AZURE_DLIB` to `Azure.CodeSigning.Dlib.dll` and
  `LOBS_SIGN_AZURE_METADATA` to a `metadata.json` naming your endpoint, account and profile.
- **A certificate in your Windows certificate store** (OV or EV, USB tokens included): set
  `LOBS_SIGN_THUMBPRINT` to its SHA-1 thumbprint.

LOBS's own account is already set up in `native/installer/signing-metadata.json`: with the Artifact
(Trusted) Signing client tools installed (`winget install Microsoft.Azure.ArtifactSigningClientTools`)
and `az login` done, both scripts sign with it, no variables needed.

`installer\sign.bat <files>` signs files by hand. It needs `signtool.exe` from the Windows SDK.

## Releasing an update

LOBS checks `https://api.github.com/repos/LoganAffleck/lobs/releases/latest` once a day and offers
the update in the app. It installs only an asset named `LOBS-Setup-<version>.exe` whose SHA-256
matches GitHub's and whose signature is from Logan Affleck, so every release must be signed.

1. Bump the version in `native/CMakeLists.txt` and `native/installer/lobs.iss`. To move to a new
   OBS engine, change `LOBS_ENGINE_VERSION` (CMakeLists), `OBS_VERSION` (build.bat) and
   `EngineVersion` (lobs.iss) together; the engine ships inside the installer and takes over on update.
2. `native\build.bat`, then `native\installer\build-installer.bat` (signed; see Code signing).
3. Tag `v<version>`, push, and publish a GitHub release (not a pre-release) with the installer attached.
   Everyone sees the update within a day.

To try the whole path without touching a real install: `ISCC /DUpdateTrial /DAppVersion=<v>` builds
an installer with its own identity; point `LOBS_UPDATE_FEED` at a release JSON on disk, with
`LOBS_UPDATE_PRETEND_VERSION`, `LOBS_UPDATE_DIR` and `LOBS_UPDATE_AUTO=1` (or `check`).
