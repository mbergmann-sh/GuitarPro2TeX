; ---------------------------------------------------------------------------
;  GuitarPROtoTeX-Convert - Inno Setup script
;
;  1. Build the Release configuration (Qt Creator or qmake + mingw32-make).
;     The post-link step of GuitarPROtoTeX-Convert.pro runs windeployqt, copies
;     the QScintilla DLL and stages everything into installer\install_src.
;  2. Open this script in Inno Setup and compile it (or: ISCC.exe
;     GuitarPROtoTeX-Convert.iss). The setup is written to installer\Output.
;     Requires Inno Setup 6.3 or newer (also works as a script for Inno Setup 7).
;
;  The version number is read from the staged .exe (VERSION in the .pro file),
;  so it only has to be changed in one place.
; ---------------------------------------------------------------------------

#define AppName "GuitarPROtoTeX-Convert"
#define AppExe  "gPro8toTeX.exe"

; All paths are absolute, built from the folder of this script: once SourceDir is set,
; Inno Setup resolves every relative path in [Setup] against SourceDir (install_src),
; not against the script.
#define ScriptDir  AddBackslash(SourcePath)
#define ProjectDir ScriptDir + "..\"
#define ExeFile    ScriptDir + "install_src\" + AppExe

#if !FileExists(ExeFile)
  #error "install_src\gPro8toTeX.exe not found - build the Release configuration first."
#endif

; "1.0.2.0" (Windows file version) -> "1.0.2"
#define FullVersion GetVersionNumbersString(ExeFile)
#define AppVersion  Copy(FullVersion, 1, RPos(".", FullVersion) - 1)

[Setup]
AppId={{6F3D2A91-4C7B-4E58-9B1A-2D7E5C8F0A43}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Michael Bergmann
AppPublisherURL=https://github.com/mbergmann-sh/GuitarPROtoTeX-Convert
AppSupportURL=https://github.com/mbergmann-sh/GuitarPROtoTeX-Convert/issues
VersionInfoVersion={#FullVersion}

; installation directory: asked on its own wizard page
DefaultDirName={autopf}\{#AppName}
DisableDirPage=no
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequiredOverridesAllowed=dialog

; welcome page with picture, license page (accept / decline)
DisableWelcomePage=no
WizardStyle=modern
WizardImageFile={#ScriptDir}wizard_image.bmp
WizardSmallImageFile={#ScriptDir}wizard_small.bmp
LicenseFile={#ProjectDir}LICENSE
ShowLanguageDialog=yes

SourceDir={#ScriptDir}install_src
OutputDir={#ScriptDir}Output
OutputBaseFilename={#AppName}_{#AppVersion}_Setup
SetupIconFile={#ProjectDir}images\app_icon.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName} {#AppVersion}

Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "german";  MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
german.SamplesTask=Beispiele installieren (MusicXML-Dateien, LaTeX-Snippets, Rahmendokument)
english.SamplesTask=Install examples (MusicXML files, LaTeX snippets, frame document)
german.SamplesGroup=Beispiele:
english.SamplesGroup=Examples:

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "samples";     Description: "{cm:SamplesTask}";       GroupDescription: "{cm:SamplesGroup}"

[Files]
; program, Qt/QScintilla DLLs, Qt plugin folders, Qt translations, LICENSE, README
Source: "*";         DestDir: "{app}";         Excludes: "samples\*"; Flags: ignoreversion recursesubdirs createallsubdirs
; optional examples
Source: "samples\*"; DestDir: "{app}\samples"; Tasks: samples;        Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}";  Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
