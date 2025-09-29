; Inno Setup script for OpenInCursorDisplay
; Builds a simple installer that installs to Program Files, adds Start Menu shortcuts,
; and (by default) configures Start with Windows via HKCU Run.

[Setup]
AppId={{A8D6E0A9-2B63-4C9A-9C13-9D410E91B8BE}}
AppName=OpenInCursorDisplay
AppVersion=1.0.0
AppPublisher=OpenInCursorDisplay
AppPublisherURL=https://example.invalid
DefaultDirName={autopf}\OpenInCursorDisplay
DisableDirPage=no
DisableProgramGroupPage=yes
OutputDir=dist
OutputBaseFilename=OpenInCursorDisplay-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
SetupIconFile=..\app.ico

[Files]
; Install the executable. Normalize the installed name to keep registry entry stable.
Source: "C:\\Users\\User\\open-window-in-cursor-display\\open_in_cursor_display.exe"; DestDir: "{app}"; DestName: "open_in_cursor_display.exe"; Flags: ignoreversion

[Tasks]
Name: "startup"; Description: "Start with Windows (current user)"; Flags: checkedonce

[Registry]
; Create/remove the Run key value to start on login for the current user.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "OpenInCursorDisplay"; ValueData: """{app}\open_in_cursor_display.exe"""; Flags: uninsdeletevalue; Tasks: startup
; Remove app settings key on uninstall (created by the app for prefs)
Root: HKCU; Subkey: "Software\OpenInCursorDisplay"; Flags: uninsdeletekey

[Icons]
Name: "{autoprograms}\\OpenInCursorDisplay"; Filename: "{app}\\open_in_cursor_display.exe"; WorkingDir: "{app}"

[Run]
; Offer to launch after installation
Filename: "{app}\\open_in_cursor_display.exe"; Description: "Launch OpenInCursorDisplay"; Flags: nowait postinstall skipifsilent
