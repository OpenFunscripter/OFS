#define Configuration "Release"

#define Version GetEnv('appveyor_build_version')
#if Version == ""
#define Version ""
#endif

[Setup]
AppName=OpenFunscripter
AppVersion={#Version}
AppPublisher=gagax1234
AppPublisherURL=www.github.com/gagax1234/OpenFunscripter
AppId=gagax1234/OpenFunscripter

DefaultDirName={pf}\OpenFunscripter
DefaultGroupName=OpenFunscripter
UninstallDisplayIcon={app}\OpenFunscripter.exe
Compression=lzma2
SolidCompression=yes
OutputBaseFilename=openfunscripter-installer
OutputDir=.\installer

[Files]
Source: "bin\{#Configuration}\OpenFunscripter.exe"; DestDir: "{app}"
Source: "bin\{#Configuration}\*.dll"; DestDir: "{app}"
Source: "ffmpeg\ffmpeg.exe"; DestDir: "{app}"
Source: "data\*"; DestDir: "{userdata}\OFS\OFS_data"

[Code]
// Uninstall on install code taken from https://stackoverflow.com/a/2099805/4040754
////////////////////////////////////////////////////////////////////
function GetUninstallString(): String;
var
  sUnInstPath: String;
  sUnInstallString: String;
begin
  sUnInstPath := ExpandConstant('Software\Microsoft\Windows\CurrentVersion\Uninstall\{#emit SetupSetting("AppId")}_is1');
  sUnInstallString := '';
  if not RegQueryStringValue(HKLM, sUnInstPath, 'UninstallString', sUnInstallString) then
    RegQueryStringValue(HKCU, sUnInstPath, 'UninstallString', sUnInstallString);
  Result := sUnInstallString;
end;
/////////////////////////////////////////////////////////////////////
function IsUpgrade(): Boolean;
begin
  Result := (GetUninstallString() <> '');
end;
/////////////////////////////////////////////////////////////////////
function UnInstallOldVersion(): Integer;
var
  sUnInstallString: String;
  iResultCode: Integer;
begin
// Return Values:
// 1 - uninstall string is empty
// 2 - error executing the UnInstallString
// 3 - successfully executed the UnInstallString
  // default return value
  Result := 0;
  // get the uninstall string of the old app
  sUnInstallString := GetUninstallString();
  if sUnInstallString <> '' then begin
    sUnInstallString := RemoveQuotes(sUnInstallString);
    if Exec(sUnInstallString, '/SILENT /NORESTART /SUPPRESSMSGBOXES','', SW_HIDE, ewWaitUntilTerminated, iResultCode) then
      Result := 3
    else
      Result := 2;
  end else
    Result := 1;
end;
/////////////////////////////////////////////////////////////////////
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep=ssInstall) then
  begin
    if (IsUpgrade()) then
    begin
      UnInstallOldVersion();
    end;
  end;
end;