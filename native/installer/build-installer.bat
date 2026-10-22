@echo off
rem Builds dist\installer\LOBS-Setup-<version>.exe from dist\LOBS (run build.bat first).
rem Signs LOBS's own files and the installer when a certificate is set up (see sign.bat).
setlocal
cd /d "%~dp0"
if not exist ..\dist\LOBS\engine\32.2.2\bin\64bit\obs.dll ( echo Run build.bat first & exit /b 1 )
set D=..\dist\LOBS
rem Qt's and OBS's files come signed by their makers; these are LOBS's.
call "%~dp0sign.bat" "%D%\LOBS.exe" "%D%\kddockwidgets-qt62.dll" "%D%\whisper.dll" "%D%\ggml.dll" "%D%\ggml-base.dll" "%D%\ggml-cpu.dll" || exit /b 1
if "%LOBS_SIGN_AZURE_METADATA%%LOBS_SIGN_THUMBPRINT%"=="" (
    "%~dp0..\deps\tools\InnoSetup\ISCC.exe" /Q lobs.iss || exit /b 1
) else (
    "%~dp0..\deps\tools\InnoSetup\ISCC.exe" /Q /DSign "/Slobs=cmd /c ""%~dp0sign.bat"" $f" lobs.iss || exit /b 1
)
echo INSTALLER OK
