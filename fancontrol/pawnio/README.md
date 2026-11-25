# PawnIO module

`LpcACPIEC.bin` is the signed ACPI EC module from
[namazso/PawnIO.Modules](https://github.com/namazso/PawnIO.Modules),
release [0.2.10](https://github.com/namazso/PawnIO.Modules/releases/tag/0.2.10),
unmodified. It executes inside the PawnIO driver and exposes byte reads
and writes on the classic ACPI EC ports 0x62/0x66 only. The module is
LGPL-2.1 — see `COPYING`; source in the repository above.

Deploy it next to `TPFanControl.exe`. The PawnIO driver itself is not
redistributed here; users install it from [pawnio.eu](https://pawnio.eu)
or `winget install namazso.PawnIO`.

sha256: `c38fd116e7aff4d1fdb0a494e296be0a6708e5a22fc72f14587442fb7f8f7906`

The signed LpcThinkpadEC.bin module from PawnIO.Modules 0.2.12 is tried first.
It checks for a Lenovo ThinkPad and a responding dedicated EC window. When
available, the session uses this module exclusively. It exposes ports 0x1600
through 0x161f. The existing ACPI module remains available on other machines.
IntelMSR.bin from the same release supplies the local CPU package temperature on the Intel L15 Gen 3. The engine reads the thermal target and package status on every sample. All modules execute locally inside PawnIO. Neither opens a network connection.

LpcThinkpadEC.bin SHA-256: 54bbf7f10d3511abfff167597485926ddf1587045f6ae849671cfaa85f97a1db
