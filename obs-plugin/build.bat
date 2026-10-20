@echo off
rem Builds lobs.dll against the installed OBS (import lib generated from obs.dll).
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
set OBSDLL=C:\Program Files\obs-studio\bin\64bit\obs.dll
dumpbin /nologo /exports "%OBSDLL%" > build\obs-exports.txt || exit /b 1
echo LIBRARY obs> build\obs.def
echo EXPORTS>> build\obs.def
for /f "skip=19 tokens=4" %%a in (build\obs-exports.txt) do echo %%a>> build\obs.def
lib /nologo /def:build\obs.def /machine:x64 /out:build\obs.lib >nul || exit /b 1
cl /nologo /O2 /LD /MD /W3 /D_CRT_SECURE_NO_WARNINGS /I deps\obs-studio\libobs src\lobs-plugin.c /Fo:build\ /Fe:build\lobs.dll /link build\obs.lib || exit /b 1
echo BUILD OK
