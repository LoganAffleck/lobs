; LOBS installer (Inno Setup 6). Build with installer\build-installer.bat after build.bat.
; Per-user install: no admin needed, and the in-app OBS engine updater can write to the
; engine folder. The OBS engine ships inside (dist\LOBS\engine\<version>), so OBS Studio
; doesn't need to be installed.

#define AppName "LOBS"
#ifndef AppVersion
#define AppVersion "0.6.1"
#endif
#define EngineVersion "32.2.2"
#define Dist "..\dist"

[Setup]
; ISCC /DUpdateTrial builds a copy with its own identity, so the update path can be tried
; end to end without touching a real install.
#ifdef UpdateTrial
AppId={{6C0B5A43-3E1D-4E4B-9C59-0B1D5A7A1C99}
#else
AppId={{6C0B5A43-3E1D-4E4B-9C59-0B1D5A7A1C21}
#endif
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion} (OBS engine {#EngineVersion})
AppPublisher=LOBS
#ifdef UpdateTrial
DefaultDirName={localappdata}\Programs\LOBS-UpdateTrial
#else
DefaultDirName={localappdata}\Programs\LOBS
#endif
DefaultGroupName=LOBS
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Dist}\installer
#ifdef UpdateTrial
OutputBaseFilename=LOBS-UpdateTrial-{#AppVersion}
#else
OutputBaseFilename=LOBS-Setup-{#AppVersion}
#endif
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\resources\lobs.ico
LicenseFile=..\..\LICENSE
UninstallDisplayIcon={app}\LOBS.exe
#ifdef Sign
; The installer and its uninstaller are signed with sign.bat (build-installer.bat passes it in).
SignTool=lobs
SignedUninstaller=yes
#endif
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
; The app, its Qt runtime and the bundled OBS engine. Test output and logs stay out.
Source: "{#Dist}\LOBS\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion; \
  Excludes: "\lobs.log,\autotest.json,\engine\install.log,\engine\*.partial,\obs-*.exe,\vc_redist.x64.exe"
Source: "{#Dist}\redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\LOBS"; Filename: "{app}\LOBS.exe"
Name: "{group}\Uninstall LOBS"; Filename: "{uninstallexe}"
Name: "{userdesktop}\LOBS"; Filename: "{app}\LOBS.exe"; Tasks: desktopicon

[Run]
; Microsoft's VC++ runtime, only if this PC doesn't have a recent enough one (asks for admin).
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Microsoft Visual C++ runtime..."; \
  Flags: waituntilterminated shellexec; Verb: runas; Check: NeedsVCRedist
Filename: "{app}\LOBS.exe"; Description: "Start LOBS"; Flags: nowait postinstall skipifsilent
; An update from inside LOBS runs this installer quietly; LOBS starts again when it's done.
Filename: "{app}\LOBS.exe"; Parameters: "--updated"; Flags: nowait; Check: IsLobsUpdate

[UninstallDelete]
; Engines downloaded by the in-app updater and the helpers LOBS mirrors from them.
Type: filesandordirs; Name: "{app}\engine"
Type: files; Name: "{app}\obs-*.exe"
Type: files; Name: "{app}\lobs.log"

[Code]
function IsLobsUpdate: Boolean;
begin
  Result := ExpandConstant('{param:LOBSUPDATE|0}') = '1';
end;

function NeedsVCRedist: Boolean;
var
  Installed, Major, Minor: Cardinal;
begin
  // LOBS and the OBS engine need the VC++ 2015-2022 runtime, 14.40 or newer.
  Result := True;
  if RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and (Installed = 1) and
     RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Major', Major) and
     RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Minor', Minor) then
    Result := (Major < 14) or ((Major = 14) and (Minor < 40));
end;
