# L15 live cooling

Open the tray window for the current owner, Intel package and EC temperatures, measured fan speed, RAPL package power and active limit. On the L15, the Windows service writes one JSON object per sample to the UTC day's file in ProgramData. No additional sensor process is needed. For an overnight build, reopen the file at UTC midnight. An observer must treat a missing or stale sample as unknown; compare UTC with the current time. A gap across a hang, shutdown or sleep is a gap in the evidence.

In PowerShell:

~~~powershell
$log = Join-Path $env:ProgramData ("TPFanControl\cooling-{0:yyyy-MM-dd}.jsonl" -f [datetime]::UtcNow)
Get-Content -LiteralPath $log -Tail 1 | ConvertFrom-Json
$engine = Join-Path $env:ProgramFiles 'Gorilla TPFanControl\TPFanControl.exe'
(Start-Process $engine -ArgumentList '--target compile 50' -PassThru -Wait).ExitCode
~~~

The target command also accepts gorilla. Targets from 40 through 70 C are temporary, per profile, until service restart. Exit 0 means the service observed the chosen profile's target. Check mode, target_c, fault, sample_ok and rpm in the subsequent live record to confirm actual control. Exit 2 is invalid input, 3 means service unavailable, and 4 means no confirmation. The command leaves profile selection to the existing mode controls.

thermal_active and power_limit_active come from the Intel package thermal-status register. thermal_history is a sticky hardware bit and may predate the job. core_limit_bits contains current low bits of MSR 0x64f on the sampled logical processor. windows_limit_flags and windows_limit_pct are Windows processor performance counters. reported_mhz is the Windows aggregate frequency counter. Watts are a package-energy difference over the interval. Null and -1 mean unavailable. BIOS mode reports no requested level or target. A zero RPM is a tachometer reading. An active Windows limit alone does not identify thermal throttling.

For a physical build, collect the compiler exit status, start/end UTC and System log events alongside the daily trace. Check sample continuity, active flags, temperature peaks, target versus measured RPM, power and any changes in clock speed. Records with missing feedback cannot establish sustained performance. Power and temperature may exceed cooling capacity even at full fan.
