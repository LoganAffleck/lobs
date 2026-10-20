@echo off
rem Builds KDDockWidgets 2.4.1 (QtQuick frontend) into deps\kddw.
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0"
rem LOBS restyle patches (drop targets + landing preview), applied over the sources
copy /y kddw-patches\*.qml KDDockWidgets\src\qtquick\views\qml\ >nul || exit /b 1
for /f "delims=" %%n in ('python -c "import ninja,os;print(os.path.join(ninja.BIN_DIR,'ninja.exe'))"') do set NINJA=%%n
cmake -S KDDockWidgets -B kddw-build -G Ninja -DCMAKE_MAKE_PROGRAM="%NINJA%" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\msvc2022_64 -DCMAKE_INSTALL_PREFIX=%~dp0kddw -DKDDockWidgets_QT6=ON -DKDDockWidgets_FRONTENDS=qtquick -DKDDockWidgets_EXAMPLES=OFF -DKDDockWidgets_TESTS=OFF -DKDDockWidgets_DOCS=OFF || exit /b 1
cmake --build kddw-build || exit /b 1
cmake --install kddw-build || exit /b 1
echo KDDW OK
