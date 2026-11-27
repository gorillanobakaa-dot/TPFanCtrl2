# Gorilla TPFanControl 2.5.1-gorilla.5 for Windows

## What is this?

Gorilla TPFanControl runs the fan service on supported Lenovo ThinkPads and shows its measurements in the Windows tray.

## Should you download it?

Choose `Gorilla-TPFanControl-2.5.1-gorilla.5-Setup.exe` for the installed Windows service. Choose `Gorilla-TPFanControl-2.5.1-gorilla.5-portable.zip` for the portable launchers. Both include the signed PawnIO driver installer. This release covers Windows on x64 ThinkPads.

## Why this matters

On an L15, you can see which temperature drives cooling and whether the fan responds. Scripts can read the same timestamped measurements as a standard user.

On one L15 under load, 12 separate Gorilla Sensors reads span three service publications. Four reads share the first timestamp, five the second, and three the third. Service minus reader ranges from −3 to +1 °C and −31 to +23 RPM. Sampling times differ.

The annotated source patch archive covers changes since `v2.5.1-gorilla.4`. [Windows service CI](https://github.com/gorillanobakaa-dot/TPFanCtrl2/actions/runs/45930373194) · [Merged source CI](https://github.com/gorillanobakaa-dot/TPFanCtrl2/actions/runs/45930610330).
