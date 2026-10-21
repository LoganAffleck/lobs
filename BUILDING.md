# Building LOBS (Windows)

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
