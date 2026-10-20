@echo off
rem Builds dist\installer\LOBS-Setup-<version>.exe from dist\LOBS (run build.bat first).
setlocal
cd /d "%~dp0"
if not exist ..\dist\LOBS\engine\32.2.2\bin\64bit\obs.dll ( echo Run build.bat first & exit /b 1 )
"%~dp0..\deps\tools\InnoSetup\ISCC.exe" /Q lobs.iss || exit /b 1
echo INSTALLER OK
