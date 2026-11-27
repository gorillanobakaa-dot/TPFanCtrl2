# L15 embedded-controller transactions and processor feedback

**Version:** 2.5.1-gorilla.5
**Date:** 2026-11-27

---

## Summary

The engine prefers the signed ThinkPad EC module, verifies replies and fan actuation, and uses fresh Intel package telemetry on the L15. The tray preserves active EC rows and the service records sensor provenance.

## Known Alternatives Considered

Commit 7febe34 retains the expansion bus register in diagnostics without assigning Celsius.

## Architecture Impact

The privileged service owns the controller. The tray and read-only observers consume service state.

## Toolchain

```
MSBuild fancontrol/fancontrol.sln /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143
```

## Resource Deltas

Comparative RSS, startup time and binary size measurements are unavailable.

## Code Changes

| File | Change | Old Behavior | New Behavior |
|------|--------|--------------|--------------|
| `fancontrol/portaccess.cpp` | modified | L15 access depended on the generic EC path. | The engine can use the signed ThinkPad EC window. |
| `fancontrol/sharedstate.cpp` | modified | Observers lacked named per-register board measurements. | The service exposes board labels, values and units with measured fan capability. |

## Subsystem Changes

**COOLING:** The full command is 64. Ordinary fixed levels are zero through seven. A requested level and a measured RPM are separate fields.

**OBSERVATION:** Use package_c for die temperature. A null board unit denotes a diagnostic byte. Check utc and sample_ok before using observations.

## Test Coverage

- **Added:** CI exercises EC transactions and runs the production engine as a service with a simulated controller.
- **Removed:** No test removal identified in the release diff.
- **Notes:** On one L15 under load, 12 separate Gorilla Sensors reads span three service publications. Four reads share the first timestamp, five the second, and three the third. Service minus reader ranges from −3 to +1 °C and −31 to +23 RPM. Sampling times differ. An earlier 50-sample comparison checked decoding of the same registers. The physical comparison uses the laptop tachometer for both readers.

## Security Posture

The Windows service writes timestamped temperature, fan and power readings to a daily file in ProgramData. A standard user can read the observations. The bundled PawnIO driver requires administrator approval at installation.

## Deployment

**Prerequisites:**
- Use a Lenovo ThinkPad with x64 Windows 10 version 1809 or later.
- Save your settings file and the previous installer before upgrading. Installation requires administrator access.

```bash
# verify
Get-FileHash .\Gorilla-TPFanControl-2.5.1-gorilla.5-Setup.exe
# Expected: SHA-256 matches the published checksum.
# install
.\Gorilla-TPFanControl-2.5.1-gorilla.5-Setup.exe
# Expected: The installer completes on supported hardware.
# verify_active
Get-Service TPFanControl
# Expected: Running
```

**Rollback:**
  1. Uninstall the current service using Windows Settings, run the previous installer, and restore your saved settings.

## Known Issues

**[medium]** The latest JSON row predates the workload.
- Cause: The service writes at its own cadence and rotates files at UTC midnight.
- Remedy: Open the current UTC day's file and compare its timestamp with the current time. Treat gaps as unknown.

## Claim Sources

| Claim | Basis | Evidence |
|-------|-------|----------|
| The readings track the same operating range during this short observation. | 🤖 model inference | *(none — model judgment)* |
| The comparison uses asynchronous samples. | 📄 stated in input | Twelve snapshots share three service publications. |


---
**How to verify this document:**
`📄 stated in input` — the model's phrasing of something your source text said.
Find the matching line in the original to verify.
`🤖 model inference` — the model's own judgment or synthesis. Treat as opinion,
not measurement. Re-run on the same input and check whether specific numbers
stay consistent between runs.

*Auto-generated DITA-structured technical release notes.*