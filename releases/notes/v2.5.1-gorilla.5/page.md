# Gorilla TPFanControl 2.5.1-gorilla.5 for Windows

## What is this?

Gorilla TPFanControl runs the fan service on supported Lenovo ThinkPads and shows its measurements in the Windows tray.

## Should you download it?

Choose `Gorilla-TPFanControl-2.5.1-gorilla.5-Setup.exe` for the installed Windows service. Choose `Gorilla-TPFanControl-2.5.1-gorilla.5-portable.zip` for the portable launchers. Both include the signed PawnIO driver installer. This release covers Windows on x64 ThinkPads.

## Why this matters

On an L15, you can see which temperature drives cooling and whether the fan responds. Scripts can read the same timestamped measurements as a standard user.

On one L15 under load, 12 separate Gorilla Sensors reads span three service publications. Four reads share the first timestamp, five the second, and three the third. Service minus reader ranges from −3 to +1 °C and −31 to +23 RPM. Sampling times differ.

The annotated source patch archive covers changes since `v2.5.1-gorilla.4`. [Windows service CI](https://github.com/gorillanobakaa-dot/TPFanCtrl2/actions/runs/45930373194) · [Merged source CI](https://github.com/gorillanobakaa-dot/TPFanCtrl2/actions/runs/45930610330).

---
# In plain language: everything in this release

This is the complete explanation, not a summary of one. Nothing below is behind a link.

<!-- plain-language track: in full, on this page -->

### Why This Release Exists

When the processor and a board sensor show different temperatures, the old display makes it hard to tell which reading drives cooling. You can now see the Intel processor reading, the named board sensors, and the fan speed measured by the laptop.

### What You Will Notice

**Temperature and fan display**
- Before: Short labels made nearby board readings look like processor temperatures.
- After:  The window identifies the source and keeps unknown firmware readings separate from temperatures.
- Affects: ThinkPad L15 users

**Cooling under load**
- Before: A requested fan level offered limited evidence that the fan responded.
- After:  The service checks fan feedback and attempts to return control to firmware when cooling fails. Compile mode reduces speed one level at a time.
- Affects: ThinkPad L15 users

### Deliberately Not Done

- **Factory maximum fan speed** — The highest observed speed depends on the individual fan. The manufacturer rating remains unknown.

### Privacy & Security

The Windows service writes timestamped temperature, fan and power readings to a daily file in ProgramData. A standard user can read the observations. The bundled PawnIO driver requires administrator approval at installation.

### How to Install

**Before you start:**
- Use a Lenovo ThinkPad with x64 Windows 10 version 1809 or later.
- Save your settings file and the previous installer before upgrading. Installation requires administrator access.

**Step 1:** Download the Windows installer and compare its SHA-256 with the release checksum.
```
Get-FileHash .\Gorilla-TPFanControl-2.5.1-gorilla.5-Setup.exe
```
✓ The two SHA-256 values match.

**Step 2:** Open the installer, approve the driver prompt if shown, then open the tray window.
✓ The window shows the active mode and fresh measurements.

**To go back:** Uninstall the current service using Windows Settings, run the previous installer, and restore your saved settings.

### If Something Goes Wrong

**A sensor has no temperature.**
The controller has no valid temperature for that channel.
What to do: Read the processor temperature in the CPU Package row.
Status: Unknown channels remain visible in diagnostics.

### Common Questions

**Q: How closely did the two readers agree?**
A: On one L15 under load, 12 separate Gorilla Sensors reads span three service publications. Four reads share the first timestamp, five the second, and three the third. Service minus reader ranges from −3 to +1 °C and −31 to +23 RPM. Sampling times differ.

**Q: Does the 5,084 RPM reading tell me how fast my fan can run?**
A: It is the highest full-speed reading recorded on the checked laptop. A different fan can reach a different speed.

### Bottom Line

Use the processor reading to follow heat during a build. Use measured RPM to see the fan response. The short physical check covers one L15. Long-term stability and absolute speed against a calibrated tachometer remain unmeasured.

### Claim Sources

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

*Auto-generated plain-language release notes.*

---

# For developers: how it works, and how to check it

Written for someone who will audit, fork or change the code. It covers the same release as the plain-language part above; neither is a summary of the other.

<!-- developer track: in full, on this page -->

### Summary

The engine prefers the signed ThinkPad EC module, verifies replies and fan actuation, and uses fresh Intel package telemetry on the L15. The tray preserves active EC rows and the service records sensor provenance.

### Known Alternatives Considered

Commit 7febe34 retains the expansion bus register in diagnostics without assigning Celsius.

### Architecture Impact

The privileged service owns the controller. The tray and read-only observers consume service state.

### Toolchain

```
MSBuild fancontrol/fancontrol.sln /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143
```

### Resource Deltas

Comparative RSS, startup time and binary size measurements are unavailable.

### Code Changes

| File | Change | Old Behavior | New Behavior |
|------|--------|--------------|--------------|
| `fancontrol/portaccess.cpp` | modified | L15 access depended on the generic EC path. | The engine can use the signed ThinkPad EC window. |
| `fancontrol/sharedstate.cpp` | modified | Observers lacked named per-register board measurements. | The service exposes board labels, values and units with measured fan capability. |

### Subsystem Changes

**COOLING:** The full command is 64. Ordinary fixed levels are zero through seven. A requested level and a measured RPM are separate fields.

**OBSERVATION:** Use package_c for die temperature. A null board unit denotes a diagnostic byte. Check utc and sample_ok before using observations.

### Test Coverage

- **Added:** CI exercises EC transactions and runs the production engine as a service with a simulated controller.
- **Removed:** No test removal identified in the release diff.
- **Notes:** On one L15 under load, 12 separate Gorilla Sensors reads span three service publications. Four reads share the first timestamp, five the second, and three the third. Service minus reader ranges from −3 to +1 °C and −31 to +23 RPM. Sampling times differ. An earlier 50-sample comparison checked decoding of the same registers. The physical comparison uses the laptop tachometer for both readers.

### Security Posture

The Windows service writes timestamped temperature, fan and power readings to a daily file in ProgramData. A standard user can read the observations. The bundled PawnIO driver requires administrator approval at installation.

### Deployment

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

### Known Issues

**[medium]** The latest JSON row predates the workload.
- Cause: The service writes at its own cadence and rotates files at UTC midnight.
- Remedy: Open the current UTC day's file and compare its timestamp with the current time. Treat gaps as unknown.

### Claim Sources

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
