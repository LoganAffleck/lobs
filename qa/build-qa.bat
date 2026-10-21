@echo off
rem QA build: LOBS with the --scenariotest runner, in native\dist\LOBS-qa (never shipped).
rem Needs a normal build first (native\build.bat), whose engine folder it links to.
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0..\native"
set QTDIR=C:\Qt\6.11.1\msvc2022_64
set DIST=dist\LOBS-qa
if not exist dist\LOBS\engine ( echo Run native\build.bat first & exit /b 1 )
for /f "delims=" %%n in ('python -c "import ninja,os;print(os.path.join(ninja.BIN_DIR,'ninja.exe'))"') do set NINJA=%%n
cmake -S . -B build-qa -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QTDIR% -DLOBS_TESTS=ON || exit /b 1
cmake --build build-qa || exit /b 1
"%QTDIR%\bin\windeployqt.exe" --release --qmldir qml --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime %DIST%\LOBS.exe >nul || exit /b 1
copy /y deps\kddw\bin\kddockwidgets-qt62.dll %DIST%\ >nul
copy /y "%QTDIR%\bin\Qt6Widgets.dll" %DIST%\ >nul
copy /y engine-modules.txt %DIST%\ >nul
if not exist %DIST%\fonts mkdir %DIST%\fonts
copy /y fonts\* %DIST%\fonts\ >nul
if not exist %DIST%\icons mkdir %DIST%\icons
copy /y icons\* %DIST%\icons\ >nul
copy /y resources\icon\lobs-*.png %DIST%\icons\ >nul
rem Share the shipped build's engine instead of a second 400 MB copy.
if not exist %DIST%\engine mklink /J %DIST%\engine dist\LOBS\engine >nul
echo QA BUILD OK
