#ifndef Stage
  #define Stage "..\build\oracle-release-stage"
#endif
[Setup]
AppId={{07C15977-9C28-4EC5-995D-D7BA79BDE881}
AppName=Oracle Rack
AppVersion=0.1.0-alpha
AppVerName=Oracle Rack Alpha 0.1.0
AppPublisher=Karsei
VersionInfoCompany=Karsei
VersionInfoVersion=0.1.0.0
VersionInfoDescription=Oracle Rack VST3 Installer
DefaultDirName={localappdata}\Programs\Common\VST3\Oracle Rack.vst3
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible and not arm64
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
OutputDir=..\dist
OutputBaseFilename=Oracle-Rack-0.1.0-alpha-win64-Setup
SetupIconFile=..\assets\vamp.ico
UninstallDisplayName=Oracle Rack Alpha
WizardStyle=modern dark
WizardSizePercent=110
WizardImageFile=wizard.bmp
WizardSmallImageFile=wizard-small.bmp
WizardImageStretch=yes
LicenseFile=..\LICENSE.txt
InfoBeforeFile=oracle-info.txt
Compression=lzma2/normal
SolidCompression=yes
CloseApplications=no
RestartApplications=no
DisableDirPage=no
SetupLogging=yes
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
[Files]
Source: "{#Stage}\Oracle Rack.vst3\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Stage}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\Verification.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\manifest.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\Sources\*"; DestDir: "{app}\Sources"; Flags: ignoreversion
[Code]
procedure InitializeWizard;
begin
  WizardForm.FinishedLabel.Caption := 'Oracle Rack is installed. Rescan VST3 plugins in your DAW. Default folder: %LOCALAPPDATA%\Programs\Common\VST3. Use Scan plugins in Oracle to find your effects. User settings and host projects are preserved on uninstall.';
end;
