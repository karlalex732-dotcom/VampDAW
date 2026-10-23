#define AppVersion "0.4.19-alpha"
#define AppNumericVersion "0.4.19.0"
#ifndef Stage
  #define Stage "..\build\installer-stage"
#endif

[Setup]
AppId={{B03D437B-A588-4B11-9AD7-1BB136FEF240}
AppName=VampDAW
AppVersion={#AppVersion}
AppVerName=VampDAW Alpha 0.4.19
AppPublisher=Karsei
VersionInfoCompany=Karsei
VersionInfoVersion={#AppNumericVersion}
VersionInfoDescription=VampDAW Alpha Installer
DefaultDirName={localappdata}\Programs\VampDAW
DefaultGroupName=VampDAW
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible and not arm64
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
OutputDir=..\dist
OutputBaseFilename=VampDAW-0.4.19-alpha-win64-Setup
SetupIconFile=..\assets\vamp.ico
UninstallDisplayIcon={app}\VampDAW.exe
UninstallDisplayName=VampDAW Alpha
WizardStyle=modern dark
WizardSizePercent=110
WizardImageFile=wizard.bmp
WizardSmallImageFile=wizard-small.bmp
WizardImageStretch=yes
WizardImageBackColor=$14100E
LicenseFile=..\LICENSE.txt
InfoBeforeFile=alpha-info.txt
Compression=lzma2/normal
SolidCompression=yes
AppMutex=Local\VampDAW.Alpha.Installed
CloseApplications=no
RestartApplications=no
ChangesAssociations=yes
DisableWelcomePage=no
DisableDirPage=no
SetupLogging=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
russian.DesktopShortcut=Создать ярлык на рабочем столе
english.DesktopShortcut=Create a desktop shortcut
russian.ProjectAssociation=Добавить VampDAW в «Открыть с помощью» для проектов .vamp
english.ProjectAssociation=Add VampDAW to Open with for .vamp projects
russian.ReadmeShortcut=Инструкция для альфа-тестера
english.ReadmeShortcut=Alpha tester guide
russian.SourceShortcut=Исходники и лицензии
english.SourceShortcut=Sources and licenses
russian.UninstallShortcut=Удалить VampDAW
english.UninstallShortcut=Uninstall VampDAW
russian.DataPreserved=Проекты и настройки пользователя сохраняются при удалении программы.
english.DataPreserved=Uninstalling preserves user projects and settings.
russian.OlderInstaller=Установлена более новая версия VampDAW. Используйте установщик этой версии или более новой.
english.OlderInstaller=A newer version of VampDAW is installed. Use that version's installer or a newer one.
russian.FullInstall=VampDAW + локальный движок Blood Stems
english.FullInstall=VampDAW + local Stem Engine
russian.CustomInstall=Выборочная установка
english.CustomInstall=Custom installation
russian.StemEngine=Blood Stems / CPU-движок и модели на 4 и 6 стемов
english.StemEngine=Blood Stems / local CPU engine + 4 and 6 stem models

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopShortcut}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "fileassoc"; Description: "{cm:ProjectAssociation}"; Flags: checkedonce

[Types]
Name: "full"; Description: "{cm:FullInstall}"
Name: "custom"; Description: "{cm:CustomInstall}"; Flags: iscustom

[Components]
Name: "core"; Description: "VampDAW"; Types: full custom; Flags: fixed
Name: "stems"; Description: "{cm:StemEngine}"; Types: full

[Files]
Source: "{#Stage}\VampDAW.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\VampDAW.installed"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\README-ALPHA.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\Collect-DefenderReport.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\manifest.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\tools\ffmpeg\*"; DestDir: "{app}\tools\ffmpeg"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Stage}\tools\stems\*"; DestDir: "{app}\tools\stems"; Components: stems; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Stage}\tools\project-import\*"; DestDir: "{app}\tools\project-import"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Stage}\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Stage}\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#Stage}\Sources\*"; DestDir: "{app}\Sources"; Flags: ignoreversion

[Icons]
Name: "{group}\VampDAW"; Filename: "{app}\VampDAW.exe"; WorkingDir: "{app}"
Name: "{group}\{cm:ReadmeShortcut}"; Filename: "{app}\README-ALPHA.txt"
Name: "{group}\{cm:SourceShortcut}"; Filename: "{app}\Sources"
Name: "{group}\{cm:UninstallShortcut}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\VampDAW"; Filename: "{app}\VampDAW.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Classes\VampDAW.Project"; ValueType: string; ValueData: "VampDAW Project"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\VampDAW.Project\DefaultIcon"; ValueType: string; ValueData: """{app}\VampDAW.exe"",0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\VampDAW.Project\shell\open\command"; ValueType: string; ValueData: """{app}\VampDAW.exe"" ""%1"""; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\.vamp\OpenWithProgids"; ValueType: none; ValueName: "VampDAW.Project"; Flags: uninsdeletevalue uninsdeletekeyifempty; Tasks: fileassoc

[Run]
Filename: "{app}\VampDAW.exe"; Description: "{cm:LaunchProgram,VampDAW}"; Flags: nowait postinstall skipifsilent

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var MS, LS: Cardinal;
begin
  Result := '';
  if FileExists(ExpandConstant('{app}\build.ps1')) or DirExists(ExpandConstant('{app}\src')) then begin
    Result := 'Choose a separate installation folder, outside the development project.';
    exit;
  end;
  if GetVersionNumbers(ExpandConstant('{app}\VampDAW.exe'), MS, LS) then
    if (MS > 4) or ((MS = 4) and (LS > 851968)) then
      Result := ExpandConstant('{cm:OlderInstaller}');
end;

procedure InitializeUninstallProgressForm();
begin
  UninstallProgressForm.StatusLabel.Caption := ExpandConstant('{cm:DataPreserved}');
end;
