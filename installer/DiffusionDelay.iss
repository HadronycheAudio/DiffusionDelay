; The Windows installer, built with Inno Setup. CI passes the version:
;   ISCC /DAppVersion=1.0.0 installer\DiffusionDelay.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
; Identifies the product for upgrades and uninstalling, so it must never change.
AppId={{97DE9045-6712-4B5A-A008-640543BC2562}
AppName=DiffusionDelay
AppVersion={#AppVersion}
AppVerName=DiffusionDelay {#AppVersion}
AppPublisher=Hadronyche
AppPublisherURL=https://github.com/HadronycheAudio/DiffusionDelay
AppCopyright=Copyright (c) 2026 Hadronyche
LicenseFile=..\LICENSE
MinVersion=10.0

; The standard location for VST 3 plug-ins, which hosts scan by default, so there's nothing to choose.
DefaultDirName={commoncf64}\VST3
DirExistsWarning=no
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Keeps the uninstaller out of the folder that hosts scan.
UninstallFilesDir={commonpf64}\Hadronyche\DiffusionDelay
UninstallDisplayName=DiffusionDelay

OutputDir=..\build\installer
OutputBaseFilename=DiffusionDelay-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Dirs]
; Makes Windows show the bundle's icon, which desktop.ini sets.
Name: "{app}\DiffusionDelay.vst3"; Attribs: system

[Files]
Source: "..\build\VST3\Release\DiffusionDelay.vst3\*"; DestDir: "{app}\DiffusionDelay.vst3"; Flags: recursesubdirs ignoreversion
Source: "..\LICENSE"; DestDir: "{app}\DiffusionDelay.vst3\Contents\Resources\Licenses"; DestName: "DiffusionDelay-LICENSE.txt"; Flags: ignoreversion
Source: "..\external\vst3sdk\LICENSE.txt"; DestDir: "{app}\DiffusionDelay.vst3\Contents\Resources\Licenses"; DestName: "VST3-SDK-LICENSE.txt"; Flags: ignoreversion
Source: "..\external\vst3sdk\vstgui4\LICENSE"; DestDir: "{app}\DiffusionDelay.vst3\Contents\Resources\Licenses"; DestName: "VSTGUI-LICENSE.txt"; Flags: ignoreversion
Source: "NOTICE.txt"; DestDir: "{app}\DiffusionDelay.vst3\Contents\Resources\Licenses"; Flags: ignoreversion
