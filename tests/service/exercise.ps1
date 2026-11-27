param([Parameter(Mandatory)][string]$Engine, [Parameter(Mandatory)][string]$Simulator, [Parameter(Mandatory)][string]$OutputDir)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted') {
    throw 'This service fixture requires a disposable GitHub-hosted Windows VM.'
}
if (Get-Service TPFanControl -ErrorAction SilentlyContinue) { throw 'Service name already in use' }
$start = Get-Date
$runtime = New-Item -ItemType Directory -Path (Join-Path $env:RUNNER_TEMP 'tp-service-runtime')
$out = New-Item -ItemType Directory -Force -Path $OutputDir
$app = Join-Path $runtime 'TPFanControl.exe'
Copy-Item $Engine $app
Copy-Item $Simulator (Join-Path $runtime 'TVicPort.dll')
$data = Join-Path $env:ProgramData TPFanControl
$statusFile = Join-Path $data status.txt
$marker = Join-Path $data active-control.lock
$guard = Join-Path $data guard-hold.txt
$case = 'setup'
$lastTs = ''
$tray = $null
$startedService = $false
"time,case,pid,cpu_seconds,working_set,status" | Set-Content "$out/samples.csv"
"tick,address,requested,observed,sim_cpu_c" | Set-Content "$runtime/register-writes.csv" -Encoding ascii
Get-FileHash $app,$Simulator | Format-List | Out-File "$out/hashes.txt"

function Scenario([int]$Cpu, [int]$Stall=0, [int]$RejectFixed=0, [int]$RejectBios=0) {
    $path = Join-Path $runtime 'scenario.txt'
    [IO.File]::WriteAllText("$path.next","$Cpu $Stall $RejectFixed $RejectBios")
    [IO.File]::Move("$path.next",$path,$true)
}
function Start-Mode([int]$Mode, [int]$Cpu, [int]$RequirePackage=0) {
    Scenario $Cpu
    @("RequireIntelPackage=$RequirePackage","PortBackend=tvicport","SingleFan=1","Active=$Mode","Cycle=1","Log2File=1","Log2csv=1",
      "StartMinimized=1","NoWaitMessage=1","ManFanSpeed=3","ManModeExit=78","GorillaTarget=50",
      "GorillaFloor=2","CompileTarget=70","Level=0 4 0 0","Level=70 7 0 0",
      "FanLevelRpm=0 1765 2183 2588 2788 2986 3182 3488 4518") |
        Set-Content "$runtime/TPFanControl.ini" -Encoding ascii
    if (Test-Path $statusFile) { $script:lastTs = ((Get-Content $statusFile -Raw) -split ';')[0].Substring(3) }
    Start-Service TPFanControl
}
function Expect([string]$Label,[scriptblock]$Condition,[int]$Timeout=35) {
    $script:case = $Label
    $until = [datetime]::UtcNow.AddSeconds($Timeout)
    do {
        $service = Get-CimInstance Win32_Service -Filter "Name='TPFanControl'"
        if ($service.State -ne 'Running') { throw "$Label service state $($service.State)" }
        $line = Get-Content $statusFile -Raw -ErrorAction SilentlyContinue
        if ($line) {
            $line = $line.Trim()
            $s = ConvertFrom-StringData ($line.Replace(';',"`n"))
            if ($s.ts -ne $script:lastTs) {
                $script:lastTs = $s.ts
                $p = Get-Process -Id $service.ProcessId
                ('{0:o},{1},{2},{3:F2},{4},"{5}"' -f [datetime]::UtcNow,$Label,$p.Id,$p.CPU,$p.WorkingSet64,$line) |
                    Add-Content "$out/samples.csv"
                if (& $Condition $s) {
                    "OK $Label $line" | Tee-Object -FilePath "$out/checks.txt" -Append
                    return
                }
            }
        }
        Start-Sleep -Milliseconds 300
    } while ([datetime]::UtcNow -lt $until)
    throw "Timeout in $Label. Last sample $line"
}
function Stop-Engine {
    Stop-Service TPFanControl
    if (Test-Path $marker) { throw 'Service stopped with active-control marker present' }
}

try {
    New-Service -Name TPFanControl -BinaryPathName ('"{0}" -s' -f $app) -StartupType Manual | Out-Null
    $startedService = $true
    Start-Mode 1 40
    Expect 'BIOS startup' {param($s) $s.mode -eq '1' -and $s.fan -eq '0x80'}
    $tray = Start-Process -FilePath $app -WorkingDirectory $runtime -WindowStyle Hidden -PassThru
    Expect 'tray attached while service owns fan' {param($s) !$tray.HasExited -and
        (Select-String -LiteralPath "$runtime/TPFanControl.log" -SimpleMatch -Pattern 'running as a client' -Quiet)}
    Stop-Process -Id $tray.Id
    Stop-Engine

    Start-Mode 2 60
    Expect 'Smart startup' {param($s) $s.mode -eq '2' -and $s.fan -eq '0x04' -and $s.rpm -eq '2788'}
    Scenario 72
    Expect 'Smart rises' {param($s) $s.fan -eq '0x07' -and $s.rpm -eq '3488'}
    Scenario 69
    Expect 'Smart drops below 70' {param($s) $s.fan -eq '0x04' -and $s.rpm -eq '2788'}
    Stop-Engine

    Start-Mode 3 40
    Expect 'Manual startup' {param($s) $s.mode -eq '3' -and $s.fan -eq '0x03' -and $s.rpm -eq '2588'}
    Scenario 82
    Expect 'Manual exits when hot' {param($s) $s.mode -eq '2' -and $s.fan -eq '0x07'}
    Stop-Engine

    Start-Mode 4 40
    Expect 'Gorilla floor' {param($s) $s.mode -eq '4' -and $s.fan -eq '0x02' -and $s.rpm -eq '2183'}
    Scenario 45
    Expect 'Gorilla near target' {param($s) $s.fan -eq '0x07' -and $s.rpm -eq '3488'}
    Scenario 50
    Expect 'Gorilla full' {param($s) $s.fan -eq '0x40' -and $s.rpm -eq '4518'}
    Scenario 49
    Expect 'Gorilla holds full' {param($s) $s.cpu -eq '49' -and $s.fan -eq '0x40'}
    Scenario 42
    Expect 'Gorilla steps down' {param($s) $s.fan -eq '0x02' -and $s.rpm -eq '2183'}
    Scenario 51 0 1 1
    Expect 'readback mismatch and refused BIOS' {param($s) $s.fault -eq '1' -and $s.bios_verified -eq '0'}
    Scenario 40
    Expect 'BIOS retry succeeds' {param($s) $s.fault -eq '1' -and $s.bios_verified -eq '1' -and $s.fan -eq '0x80'}
    Stop-Engine

    Start-Mode 5 48
    Expect 'Compile startup' {param($s) $s.mode -eq '5' -and $s.fault -eq '0' -and $s.fan -in '0x04','0x05','0x07'}
    $client = Start-Process $app -ArgumentList '--target compile 50' -PassThru -Wait
    if ($client.ExitCode -ne 0) { throw "Target command failed: $($client.ExitCode)" }
    Expect 'live Compile target' {param($s) $s.target_c -eq '50' -and $s.mode -eq '5'}
    $client = Start-Process $app -ArgumentList '--target compile 71' -PassThru -Wait
    if ($client.ExitCode -ne 2) { throw 'Out-of-range target accepted' }
    [IO.File]::WriteAllText($guard,'CI simulated workload')
    Expect 'Compile guard full' {param($s) $s.fan -eq '0x40' -and $s.rpm -eq '4518' -and $s.why -eq 'guard'}
    Remove-Item -LiteralPath $guard
    Scenario 48 1
    Expect 'stalled motor hands back' {param($s) $s.fault -eq '1' -and $s.bios_verified -eq '1'} 45
    Stop-Engine

    Start-Mode 4 40
    Expect 'engine owns manual level before process loss' {param($s) $s.fan -eq '0x02' -and (Test-Path $marker)}
    $ownPid = (Get-CimInstance Win32_Service -Filter "Name='TPFanControl'").ProcessId
    if ((Get-Process -Id $ownPid).Path -ne $app) { throw 'Unexpected service executable' }
    Stop-Process -Id $ownPid -Force
    (Get-Service TPFanControl).WaitForStatus('Stopped',[timespan]::FromSeconds(20))
    Start-Mode 4 40
    Expect 'restart after process loss' {param($s) $s.fault -eq '1' -and $s.bios_verified -eq '1' -and $s.mode -eq '1'}
    Stop-Engine
    Start-Mode 4 40 1
    Expect 'missing package hands back to firmware' {param($s) $s.package_c -eq '-1' -and
        $s.sample_ok -eq '0' -and $s.fault -eq '1' -and $s.bios_verified -eq '1' -and $s.fan -eq '0x80'}
    Stop-Engine
    Start-Mode 4 0
    Expect 'invalid CPU startup' {param($s) $s.cpu -eq '-1' -and $s.fault -eq '1' -and $s.fan -eq '0x80'}
    Stop-Engine
    if (Select-String -LiteralPath "$runtime/register-writes.csv" -SimpleMatch -Pattern ',0x31,' -Quiet) {throw 'Unexpected single-fan bank write'}
    'OK no single-fan bank writes' | Tee-Object -FilePath "$out/checks.txt" -Append
} finally {
    if ($tray -and !$tray.HasExited) { Stop-Process -Id $tray.Id -ErrorAction SilentlyContinue }
    if ($startedService) { Stop-Service TPFanControl -ErrorAction Continue }
    foreach ($name in 'TPFanControl.log','TPFanControl_csv.txt','TPFanControl.ini','register-writes.csv') {
        if (Test-Path "$runtime/$name") { Copy-Item "$runtime/$name" $out }
    }
    if (Test-Path $statusFile) { Copy-Item $statusFile $out }
    foreach ($log in 'System','Application') {
        Get-WinEvent -FilterHashtable @{LogName=$log;StartTime=$start} -ErrorAction SilentlyContinue |
            Where-Object {$_.Message -match 'TPFanControl' -or $_.ProviderName -in 'ACPI','Microsoft-Windows-Kernel-Power'} |
            Select-Object TimeCreated,Id,ProviderName,Message | Export-Csv "$out/$log.csv" -NoTypeInformation
    }
}
