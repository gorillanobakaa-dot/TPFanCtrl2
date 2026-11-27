# TPFanControl 2.5.1-gorilla.5 — See which temperature controls cooling on your L15

**Date:** 2026-11-27
**Previous version:** v2.5.1-gorilla.4

---

## Why This Release Exists

When the processor and a board sensor show different temperatures, the old display makes it hard to tell which reading drives cooling. You can now see the Intel processor reading, the named board sensors, and the fan speed measured by the laptop.

## What You Will Notice

**Temperature and fan display**
- Before: Short labels made nearby board readings look like processor temperatures.
- After:  The window identifies the source and keeps unknown firmware readings separate from temperatures.
- Affects: ThinkPad L15 users

**Cooling under load**
- Before: A requested fan level offered limited evidence that the fan responded.
- After:  The service checks fan feedback and attempts to return control to firmware when cooling fails. Compile mode reduces speed one level at a time.
- Affects: ThinkPad L15 users

## Deliberately Not Done

- **Factory maximum fan speed** — The highest observed speed depends on the individual fan. The manufacturer rating remains unknown.

## Privacy & Security

The Windows service writes timestamped temperature, fan and power readings to a daily file in ProgramData. A standard user can read the observations. The bundled PawnIO driver requires administrator approval at installation.

## How to Install

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

## If Something Goes Wrong

**A sensor has no temperature.**
The controller has no valid temperature for that channel.
What to do: Read the processor temperature in the CPU Package row.
Status: Unknown channels remain visible in diagnostics.

## Common Questions

**Q: How closely did the two readers agree?**
A: On one L15 under load, 12 separate Gorilla Sensors reads span three service publications. Four reads share the first timestamp, five the second, and three the third. Service minus reader ranges from −3 to +1 °C and −31 to +23 RPM. Sampling times differ.

**Q: Does the 5,084 RPM reading tell me how fast my fan can run?**
A: It is the highest full-speed reading recorded on the checked laptop. A different fan can reach a different speed.

## Bottom Line

Use the processor reading to follow heat during a build. Use measured RPM to see the fan response. The short physical check covers one L15. Long-term stability and absolute speed against a calibrated tachometer remain unmeasured.

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

*Auto-generated plain-language release notes.*