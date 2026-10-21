@echo off
rem Builds whisper.cpp 1.7.4 (speech to text for live captions) into deps\whisper.
rem Source: deps\whisper.cpp (github.com/ggerganov/whisper.cpp, tag v1.7.4, MIT).
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
for /f "delims=" %%n in ('python -c "import ninja,os;print(os.path.join(ninja.BIN_DIR,'ninja.exe'))"') do set NINJA=%%n
rem GGML_NATIVE=OFF: portable x64 build (AVX2 at most), not tuned to this PC's CPU.
cmake -S whisper.cpp -B whisper-build -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=%~dp0whisper -DBUILD_SHARED_LIBS=ON -DGGML_NATIVE=OFF -DGGML_AVX2=ON -DWHISPER_BUILD_EXAMPLES=OFF -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_SERVER=OFF || exit /b 1
cmake --build whisper-build || exit /b 1
cmake --install whisper-build || exit /b 1
echo WHISPER OK
