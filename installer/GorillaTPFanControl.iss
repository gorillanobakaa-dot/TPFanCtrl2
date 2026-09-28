; ===========================================================================
;  Gorilla TPFanControl - installer (Inno Setup 6)
;
;  Builds one self-contained Setup.exe. Everything the program needs is
;  inside it; nothing is downloaded at install time:
;    - TPFanControl.exe      (static C runtime: needs only DLLs that are part
;                             of Windows itself - checked with dumpbin)
;    - LpcACPIEC.bin         (PawnIO module for the ThinkPad embedded
;                             controller, LGPL-2.1) + its licence
;    - PawnIO_setup.exe      (signed PawnIO 2.2.0 driver installer, GPL-2.0
;                             driver; SHA-256 checked against winget's record)
;    - TPFanControl.ini      (defaults; an existing one is never overwritten)
;
;  Refuses to install unless: 64-bit Intel/AMD Windows, Windows 10 1809
;  (build 17763) or newer, and a Lenovo ThinkPad. The fan is driven by
;  writing ThinkPad-specific embedded-controller registers; on other
;  laptops the same write could mean something else entirely.
;
;  Build:  ISCC.exe GorillaTPFanControl.iss   (from this folder)
; ===========================================================================

#define AppName      "Gorilla TPFanControl"
#define AppVersion   "2.5.1-gorilla.1"
#define AppPublisher "Gorilla Fan Control (fork of mews-se/TPFanCtrl2)"
#define AppUrl       "https://github.com/gorillanobakaa-dot/TPFanCtrl2"
#define TaskName     "Gorilla TPFanControl"
#define PawnIOVer    "2.2.0.0"

[Setup]
AppId={{F4ADED86-5B4A-4CD2-AF54-32918192428E}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; 64-bit Windows only: PawnIO is built for x64, and the program must run on
; x64 Windows to reach the driver.
ArchitecturesAllowed=x64compatible and not arm64
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0.17763
LicenseFile=..\LICENSE
InfoBeforeFile=README-FIRST.txt
OutputDir=output
OutputBaseFilename=Gorilla-TPFanControl-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\TPFanControl.exe
UninstallDisplayName={#AppName}
CloseApplications=no
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\fancontrol\Release\TPFanControl.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\fancontrol\pawnio\LpcACPIEC.bin";     DestDir: "{app}"; Flags: ignoreversion
Source: "..\fancontrol\pawnio\COPYING";           DestDir: "{app}\licences"; DestName: "LpcACPIEC-LGPL-2.1.txt"; Flags: ignoreversion
Source: "..\LICENSE";                             DestDir: "{app}\licences"; DestName: "TPFanControl-Unlicense.txt"; Flags: ignoreversion
Source: "THIRD-PARTY-NOTICES.txt";                DestDir: "{app}\licences"; Flags: ignoreversion
Source: "README-FIRST.txt";                       DestDir: "{app}"; Flags: ignoreversion
; settings: a user's own file is kept on reinstall and on upgrade
Source: "..\fancontrol\TPFanControl.ini";         DestDir: "{app}"; Flags: onlyifdoesntexist uninsneveruninstall
; PawnIO: unpacked to a temp folder only if it has to be installed
Source: "thirdparty\PawnIO\PawnIO_setup.exe";     DestDir: "{tmp}"; Flags: deleteafterinstall; Check: PawnIONeeded

[Icons]
; Starts it if it was exited. While it runs, its window opens from the tray icon
; (the task ignores a second start, so this cannot open the window itself).
Name: "{group}\Start Gorilla TPFanControl"; Filename: "{sys}\schtasks.exe"; Parameters: "/run /tn ""{#TaskName}"""; IconFilename: "{app}\TPFanControl.exe"
Name: "{group}\Settings file (TPFanControl.ini)"; Filename: "{app}\TPFanControl.ini"
Name: "{group}\Uninstall Gorilla TPFanControl"; Filename: "{uninstallexe}"

[Run]
; 1. the driver, silently (switches from winget's manifest for PawnIO 2.2.0)
Filename: "{tmp}\PawnIO_setup.exe"; Parameters: "-install -silent"; StatusMsg: "Installing the PawnIO driver..."; Flags: waituntilterminated; Check: PawnIONeeded; AfterInstall: CheckPawnIOResult
; 2. start at sign-in with admin rights and no prompt, via a scheduled task
Filename: "{sys}\schtasks.exe"; Parameters: "/create /tn ""{#TaskName}"" /xml ""{tmp}\task.xml"" /f"; StatusMsg: "Setting up start at sign-in..."; Flags: runhidden waituntilterminated; BeforeInstall: WriteTaskXml
; 3. start it now
Filename: "{sys}\schtasks.exe"; Parameters: "/run /tn ""{#TaskName}"""; Flags: runhidden waituntilterminated postinstall; Description: "Start Gorilla TPFanControl now"

[UninstallRun]
; close it properly first: on exit the program hands the fan back to the BIOS
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -Command ""try {{ [Threading.EventWaitHandle]::OpenExisting('Global\TPFanControl_Close').Set() | Out-Null }} catch {{}}; $p = Get-Process TPFanControl -ErrorAction SilentlyContinue; if ($p) {{ $null = $p.WaitForExit(15000) }}"""; Flags: runhidden waituntilterminated; RunOnceId: "CloseEngine"
Filename: "{sys}\schtasks.exe"; Parameters: "/end /tn ""{#TaskName}"""; Flags: runhidden waituntilterminated; RunOnceId: "EndTask"
Filename: "{sys}\schtasks.exe"; Parameters: "/delete /tn ""{#TaskName}"" /f"; Flags: runhidden waituntilterminated; RunOnceId: "DeleteTask"

[UninstallDelete]
Type: files; Name: "{app}\TPFanControl.log"
Type: files; Name: "{app}\TPFanControl_csv.txt"
Type: files; Name: "{app}\TPFanControl_last_csv.txt"
Type: files; Name: "{app}\TPFanControl.ini.saving"

[Code]
function WbemString(const Cls, Prop: String): String;
var Locator, Svc, Items, Item, V: Variant; I: Integer; S: String;
begin
  Result := '';
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Svc := Locator.ConnectServer('.', 'root\cimv2');
    Items := Svc.ExecQuery('SELECT ' + Prop + ' FROM ' + Cls);
    for I := 0 to Items.Count - 1 do begin
      Item := Items.ItemIndex(I);
      V := Item.Properties_.Item(Prop).Value;
      S := '';
      try
        if not VarIsNull(V) then S := V;      // an empty WMI value stays ''
      except
        S := '';
      end;
      Result := Result + ' ' + S;
    end;
  except
    Result := '';
  end;
end;

function IsThinkPad(var Found: String): Boolean;
var Maker, Family, Version, Model: String;
begin
  Maker   := WbemString('Win32_ComputerSystem', 'Manufacturer');
  Family  := WbemString('Win32_ComputerSystem', 'SystemFamily');
  Model   := WbemString('Win32_ComputerSystem', 'Model');
  Version := WbemString('Win32_ComputerSystemProduct', 'Version');
  Found := Trim(Maker + ' / ' + Trim(Version + ' ' + Family));
  Result := (Pos('LENOVO', Uppercase(Maker)) > 0) and
            (Pos('THINKPAD', Uppercase(Family + ' ' + Version + ' ' + Model)) > 0);
end;

function InitializeSetup: Boolean;
var Found: String;
begin
  Result := True;
  if not IsThinkPad(Found) then begin
    MsgBox('This fan control only works on Lenovo ThinkPad laptops, and this computer does not appear to be one:' + #13#10#13#10 +
           '    ' + Found + #13#10#13#10 +
           'It controls the fan by writing to registers of the ThinkPad embedded controller. ' +
           'On other laptops the same registers can mean something else, so installing here could leave the fan in a wrong state. ' +
           'Nothing has been changed.', mbCriticalError, MB_OK);
    Result := False;
  end;
end;

// ----- PawnIO --------------------------------------------------------------
function InstalledPawnIOVersion: String;
begin
  Result := '';
  if not RegQueryStringValue(HKLM64, 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PawnIO', 'DisplayVersion', Result) then
    Result := '';
end;

function PawnIONeeded: Boolean;
var Have: String; HaveV, WantV: Int64;
begin
  Have := InstalledPawnIOVersion;
  // missing, or a version string we cannot read: install (the safe side)
  if (Have = '') or not StrToVersion(Have, HaveV) or not StrToVersion('{#PawnIOVer}', WantV) then begin
    Result := True; exit;
  end;
  // otherwise only if the installed version is older than the bundled one
  Result := ComparePackedVersion(HaveV, WantV) < 0;
end;

procedure CheckPawnIOResult;
var Code: Integer;
begin
  // PawnIO 2.2.0 returns DOS error codes; 3010 = done, restart needed.
  if InstalledPawnIOVersion = '' then
    MsgBox('The PawnIO driver did not install. The fan control cannot reach the fan without it.' + #13#10 +
           'The program is installed; run Setup again, or install PawnIO from https://pawnio.eu, then restart.',
           mbError, MB_OK);
end;

// ----- scheduled task --------------------------------------------------------
procedure WriteTaskXml;
var Xml: String; User: String;
begin
  User := GetUserNameString;
  if Pos('\', User) = 0 then User := GetEnv('USERDOMAIN') + '\' + User;
  Xml :=
    '<?xml version="1.0" encoding="UTF-16"?>' + #13#10 +
    '<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">' + #13#10 +
    '  <RegistrationInfo><Description>Starts Gorilla TPFanControl at sign-in with administrator rights, without a prompt.</Description></RegistrationInfo>' + #13#10 +
    '  <Triggers><LogonTrigger><Enabled>true</Enabled><UserId>' + User + '</UserId><Delay>PT15S</Delay></LogonTrigger></Triggers>' + #13#10 +
    '  <Principals><Principal id="Author"><UserId>' + User + '</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>' + #13#10 +
    '  <Settings>' + #13#10 +
    '    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>' + #13#10 +
    '    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>' + #13#10 +
    '    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>' + #13#10 +
    '    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>' + #13#10 +
    '    <Priority>4</Priority>' + #13#10 +
    '    <RestartOnFailure><Interval>PT1M</Interval><Count>3</Count></RestartOnFailure>' + #13#10 +
    '  </Settings>' + #13#10 +
    '  <Actions Context="Author"><Exec><Command>' + ExpandConstant('{app}\TPFanControl.exe') + '</Command><WorkingDirectory>' + ExpandConstant('{app}') + '</WorkingDirectory></Exec></Actions>' + #13#10 +
    '</Task>';
  SaveStringToFile(ExpandConstant('{tmp}\task.xml'), Xml, False);
end;

// ----- before files are copied: close a running copy properly ---------------
function PrepareToInstall(var NeedsRestart: Boolean): String;
var Code: Integer;
begin
  Result := '';
  Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    '-NoProfile -ExecutionPolicy Bypass -Command "try { [Threading.EventWaitHandle]::OpenExisting(''Global\TPFanControl_Close'').Set() | Out-Null } catch {}; $p = Get-Process TPFanControl -ErrorAction SilentlyContinue; if ($p) { $null = $p.WaitForExit(15000) }"',
    '', SW_HIDE, ewWaitUntilTerminated, Code);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    MsgBox('Gorilla TPFanControl is removed and the fan is back under BIOS control.' + #13#10#13#10 +
           'Your settings file (TPFanControl.ini) was kept in the program folder.' + #13#10 +
           'The PawnIO driver was left installed, because other tools may use it. ' +
           'To remove it too: Settings > Apps > Installed apps > PawnIO > Uninstall.',
           mbInformation, MB_OK);
end;
