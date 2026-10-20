@echo off
rem Configure, build and deploy LOBS into dist\LOBS (the folder the installer ships).
rem The OBS engine is installed from OBS's official release zip into dist\LOBS\engine\<version>.
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
set QTDIR=C:\Qt\6.11.1\msvc2022_64
set OBS_VERSION=32.2.2
set OBS_ZIP=deps\downloads\obs\OBS-Studio-%OBS_VERSION%-Windows-x64.zip
set DIST=dist\LOBS
for /f "delims=" %%n in ('python -c "import ninja,os;print(os.path.join(ninja.BIN_DIR,'ninja.exe'))"') do set NINJA=%%n
cmake -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QTDIR% || exit /b 1
cmake --build build || exit /b 1
"%QTDIR%\bin\windeployqt.exe" --release --qmldir qml --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime %DIST%\LOBS.exe >nul || exit /b 1
copy /y deps\kddw\bin\kddockwidgets-qt62.dll %DIST%\ >nul
rem obs-browser links Qt Widgets; ship ours so only one Qt build is loaded
copy /y "%QTDIR%\bin\Qt6Widgets.dll" %DIST%\ >nul
copy /y engine-modules.txt %DIST%\ >nul
rem License and notices ship with the app
copy /y ..\LICENSE %DIST%\LICENSE.txt >nul
copy /y ..\THIRD_PARTY_NOTICES.md %DIST%\ >nul
if not exist %DIST%\fonts mkdir %DIST%\fonts
copy /y fonts\* %DIST%\fonts\ >nul
if not exist %DIST%\icons mkdir %DIST%\icons
copy /y icons\* %DIST%\icons\ >nul
rem App icon sizes for the window and taskbar
copy /y resources\icon\lobs-*.png %DIST%\icons\ >nul
rem Bundled engine: OBS's own release build, only the modules in engine-modules.txt.
if not exist %DIST%\engine\%OBS_VERSION%\bin\64bit\obs.dll (
  if not exist %OBS_ZIP% ( echo Missing %OBS_ZIP% & exit /b 1 )
  rem LOBS.exe is a GUI app, so wait for it explicitly
  start "" /wait %DIST%\LOBS.exe --install-engine "%CD%\%OBS_ZIP%" %OBS_VERSION%
  if errorlevel 1 ( type %DIST%\engine\install.log & exit /b 1 )
)
rem Microsoft's VC++ runtime installer (signature checked when it was added to deps\redist),
rem which the setup runs on PCs that don't have it.
if not exist dist\redist mkdir dist\redist
copy /y deps\redist\vc_redist.x64.exe dist\redist\ >nul || exit /b 1
echo BUILD OK
