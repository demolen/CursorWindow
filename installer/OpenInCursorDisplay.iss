; Inno Setup script for OpenInCursorDisplay
; Builds a simple installer that installs to Program Files, adds Start Menu shortcuts,
; and (by default) configures Start with Windows via HKCU Run.

[Setup]
AppId={{A8D6E0A9-2B63-4C9A-9C13-9D410E91B8BE}}
AppName=ChaseTheCursor
AppVerName=ChaseTheCursor
AppVersion=1.0.0
AppPublisher=Nanane
AppPublisherURL=https://nanane.de
DefaultDirName={autopf}\ChaseTheCursor
DisableDirPage=no
DisableProgramGroupPage=yes
UsePreviousAppDir=no
CloseApplications=yes
RestartApplications=no
OutputDir=dist
OutputBaseFilename=ChaseTheCursor-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
SetupIconFile=..\\app.ico
UninstallDisplayIcon={app}\\ChaseTheCursor.exe

[Files]
; Install the executable. Normalize the installed name to keep registry entry stable.
Source: "..\\ChaseTheCursor.exe"; DestDir: "{app}"; DestName: "ChaseTheCursor.exe"; Flags: ignoreversion

[Tasks]
Name: "startup"; Description: "Start with Windows (current user)"; Flags: checkedonce

[Registry]
; Create/remove the Run key value to start on login for the current user.
Root: HKCU; Subkey: "Software\\Microsoft\\Windows\\CurrentVersion\\Run"; ValueType: string; ValueName: "ChaseTheCursor"; ValueData: """{app}\\ChaseTheCursor.exe"""; Flags: uninsdeletevalue; Tasks: startup
; Remove app settings key on uninstall (created by the app for prefs)
Root: HKCU; Subkey: "Software\\ChaseTheCursor"; Flags: uninsdeletekey

[Code]
function InitializeSetup(): Boolean;
var
  ResultCode: Integer;
begin
  { Try to stop a running instance before install/upgrade }
  Exec(ExpandConstant('{cmd}'), '/c taskkill /IM "ChaseTheCursor.exe" /T /F', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := True;
end;

function InitializeUninstall(): Boolean;
var
  ResultCode: Integer;
begin
  { Stop running instance to allow file removal }
  Exec(ExpandConstant('{cmd}'), '/c taskkill /IM "ChaseTheCursor.exe" /T /F', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := True;
end;

[Icons]
Name: "{autoprograms}\\ChaseTheCursor"; Filename: "{app}\\ChaseTheCursor.exe"; WorkingDir: "{app}"

[Run]
; Offer to launch after installation
Filename: "{app}\\ChaseTheCursor.exe"; Description: "Launch ChaseTheCursor"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
