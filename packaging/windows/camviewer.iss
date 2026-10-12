; Instalador do CamViewer para Windows (Inno Setup 6).
;
;   ISCC /DAppVersion=1.2.3 /DSourceDir=C:\caminho\dist camviewer.iss
;
; SourceDir é a pasta já pronta para rodar: CamViewer.exe, as DLLs do Qt
; (windeployqt) e ffmpeg\ffmpeg.exe.

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist"
#endif

[Setup]
AppId={{6F0B6C0E-6C55-4D5B-9B7A-3B0C3A1E7C42}
AppName=CamViewer
AppVersion={#AppVersion}
AppPublisher=Glerme
AppPublisherURL=https://github.com/Glerme/CamViewer
AppSupportURL=https://github.com/Glerme/CamViewer/issues
DefaultDirName={autopf}\CamViewer
DefaultGroupName=CamViewer
DisableProgramGroupPage=yes
; Instala só para o usuário, sem pedir administrador; quem quiser instalar
; para todos pode escolher isso na primeira tela.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\..\out
OutputBaseFilename=CamViewer-{#AppVersion}-windows-x64-setup
SetupIconFile=camviewer.ico
UninstallDisplayIcon={app}\CamViewer.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{autoprograms}\CamViewer"; Filename: "{app}\CamViewer.exe"
Name: "{autodesktop}\CamViewer"; Filename: "{app}\CamViewer.exe"; Tasks: desktopicon

[Registry]
; "Iniciar ao fazer login" é ligado dentro do app; aqui só garantimos que a
; entrada some na desinstalação.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "CamViewer"; Flags: dontcreatekey uninsdeletevalue

[Run]
Filename: "{app}\CamViewer.exe"; Description: "{cm:LaunchProgram,CamViewer}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM CamViewer.exe"; Flags: runhidden; RunOnceId: "StopCamViewer"

[Code]
// O app fica na bandeja mesmo com a janela fechada; se estiver rodando, os
// arquivos ficam travados e a atualização falha.
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM CamViewer.exe', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := '';
end;
