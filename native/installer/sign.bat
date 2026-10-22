@echo off
rem Signs the files given as arguments. Set one of these first (see BUILDING.md, "Code signing"):
rem   LOBS_SIGN_AZURE_METADATA  metadata.json for Azure Trusted Signing, with LOBS_SIGN_AZURE_DLIB
rem                             pointing at Azure.CodeSigning.Dlib.dll
rem   LOBS_SIGN_THUMBPRINT      SHA-1 thumbprint of a code signing certificate in your Windows
rem                             certificate store (an OV/EV certificate, USB tokens included)
rem With neither set, nothing is signed (exit code 0) so local builds still work.
setlocal
if "%LOBS_SIGN_AZURE_METADATA%%LOBS_SIGN_THUMBPRINT%"=="" ( echo sign: no certificate set up, leaving unsigned & exit /b 0 )
set SIGNTOOL=
for /d %%v in ("%ProgramFiles(x86)%\Windows Kits\10\bin\10.*") do if exist "%%v\x64\signtool.exe" set "SIGNTOOL=%%v\x64\signtool.exe"
if not defined SIGNTOOL ( echo sign: signtool.exe not found, install the Windows SDK & exit /b 1 )
set TS=/fd SHA256 /tr http://timestamp.acs.microsoft.com /td SHA256
if not "%LOBS_SIGN_AZURE_METADATA%"=="" (
    "%SIGNTOOL%" sign /v %TS% /dlib "%LOBS_SIGN_AZURE_DLIB%" /dmdf "%LOBS_SIGN_AZURE_METADATA%" %* || exit /b 1
) else (
    "%SIGNTOOL%" sign /v /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 /sha1 %LOBS_SIGN_THUMBPRINT% %* || exit /b 1
)
