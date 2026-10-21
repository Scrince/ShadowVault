#define MyAppName "ShadowVault"
#define MyAppVersion "1.2.0"
#define MyAppExeName "ShadowVault.exe"
[Setup]
AppId={{3ADBB58D-4FE6-4A57-AE2E-49B5BA93A120}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=ShadowVault
DefaultDirName={localappdata}\Programs\ShadowVault
DefaultGroupName=ShadowVault
OutputDir=..\release\windows
OutputBaseFilename=ShadowVault-Setup-1.2.0
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
Uninstallable=yes

[Files]
Source: "..\dist\ShadowVault-1.2.0-portable.exe"; DestDir: "{app}"; DestName: "ShadowVault.exe"; Flags: ignoreversion
Source: "..\dist\shadowvault.ini"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\ShadowVault"; Filename: "{app}\ShadowVault.exe"

