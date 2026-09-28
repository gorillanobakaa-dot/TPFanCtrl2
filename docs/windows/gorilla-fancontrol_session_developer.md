# Gorilla TPFanControl 2.5.1-gorilla.1 and gorilla.2: window, persistence, Gorilla mode, level labels, Windows service installer

> Session record generated 2026-09-28

---

## Problem Being Solved

Upstream TPFanCtrl2 2.5.1 (mews-se) drives the ThinkPad fan correctly, but its dialog clips the temperature list (both scroll bars, about half of the sensors visible), has no working minimise button, loses a Manual level at restart, starts Manual at level 0 (fan off), and has no installer: it runs by hand as administrator. The owner of the test laptop also wants a cooling-first mode that keeps the CPU at 40-50 °C and ignores noise, and speed choices expressed as percentages.

## Approach Taken

Fork, do not rewrite: BIOS, Smart and Manual stay upstream code. Changes go into the dialog resources (`fancontrol/res/fancontrol.rc`), the dialog code (`fancontrol/fancontrol.cpp`), the settings reader and writer (`fancontrol/misc.cpp`), the control loop (`fancontrol/fanstuff.cpp`, new mode 4), the shared-memory block used between engine and client (`fancontrol/sharedstate.h`), and a new Inno Setup 6 installer (`installer/GorillaTPFanControl.iss`) that installs upstream's `TPFanControl` service. Percentages come from measured rpm per level (`FanLevelRpm=`) because the embedded controller accepts only levels 0-7, 0x40 and 0x80.

## Before

Upstream 2.5.1 at commit `4446932`: modes 1-3, a caption-bar dialog with a clipped list view, no write-back of user choices, `ManFanSpeed=0` in the default ini, no installer, no packaging.

## After

Tag `v2.5.1-gorilla.2` at commit `a6b449f`: a 719x208 DLU dialog with an owner-drawn title bar (minimise hover `CaptionHoverColor`, default `00B4FF`), write-back of mode, level and Gorilla settings through `RewriteIniKeys`, mode 4 (Gorilla) in the dialog, the tray menu (item 5006) and the engine, level combos filled from `FanLevelRpm=` with % and rpm labels, and an installer that registers the `TPFanControl` service (LocalSystem, automatic start) and a normal-rights client window in the common Startup folder. Both releases are on GitHub with SHA-256 values that match the tested files.

## Known Alternatives

Considered and not taken: a full rewrite of the Windows program (the owner asked to keep the working fan logic); a scheduled task at sign-in (used in `gorilla.1`, replaced by the service in `gorilla.2` so control starts at boot); free percentages (impossible: the controller takes only discrete levels); steering Gorilla mode by the hottest sensor (the L15's `pwr` sensor reads 66 °C or 0 whatever the fan does).

## Files Changed

| File | Change | What Changed | Why |
|------|--------|--------------|-----|
| `fancontrol/res/fancontrol.rc` | modified | Dialogs 9000/9002 relaid out to 719x208 DLU; controls 8120-8122 (title strip, minimise, close), 8303 (Gorilla radio), 8320/8321 (target and floor combos), 8322-8324 (labels); tray item 5006 | Readable list, working title bar, room for Gorilla mode |
| `fancontrol/fancontrol.cpp` | modified | Custom title bar (subclassed owner-drawn buttons, WS_CAPTION stripped at run time, WM_NCHITTEST -> HTCAPTION), level combos with item data, mode 4 in ModeToDialog/CurrentModeFromDialog, client SendCommand/PullSharedState carry Gorilla fields, CB_SETDROPPEDWIDTH on the floor list | The .rc CAPTION statement re-adds WS_CAPTION; item data avoids parsing labels longer than the 16-byte command field |
| `fancontrol/fanstuff.cpp` | modified | GorillaControl (mode 4), engine intake of cmdGorillaTarget/cmdGorillaFloor, publishing gorillaTarget/gorillaFloor, mode-change traces | Cooling-first mode with the same staircase rule as Smart (0x40 only above 4000 rpm) |
| `fancontrol/misc.cpp` | modified | ReadConfig parses GorillaTarget (30-70), GorillaFloor (1-7, 64), FanLevelRpm (9 ints), CaptionHoverColor; generic RewriteIniKeys with a `.saving` file and MoveFileEx; PersistUserMode and PersistGorilla | User choices survive a restart; no partial ini after a power cut |
| `fancontrol/sharedstate.h` | modified | FCSHARED gains gorillaTarget, gorillaFloor, cmdGorillaTarget, cmdGorillaFloor; mode comment lists 4 gorilla | The client window and the service engine exchange Gorilla settings |
| `fancontrol/TPFanControl.ini` | modified | ManFanSpeed=4; documented Active=4, GorillaTarget=45, GorillaFloor=5, FanLevelRpm example | Level 0 switched the fan off when Manual was clicked |
| `installer/GorillaTPFanControl.iss` | added | ThinkPad and Windows checks, PawnIO 2.2.0 bundled with a version check, service install (`-i -q`) or restart (`sc start`) decided once in PrepareToInstall, old task removal, uninstall with `-u -q` | One self-contained Setup.exe; control from boot |
| `installer/build-portable.ps1 and installer/portable/*` | added | Portable zip with the same files and two launchers | A no-install option |
| `README.md, installer/README-FIRST.txt, installer/portable/README-PORTABLE.txt` | modified | Fork section, service, Gorilla mode, percentages | Document the behaviour users see |

## Decisions Made

- 📄 **Keep upstream BIOS, Smart and Manual logic unchanged** — The owner asked to leave the fan logic alone (it works)
- 📄 **Label levels with measured rpm instead of offering percentages** — The EC takes only levels 0-7, 0x40 (disengaged, full) and 0x80 (BIOS), so there is no free percentage
- 📄 **Gorilla mode steers by sensor 0 (CPU)** — The L15's 'pwr' sensor reads 66 C (or 0) whatever the fan does
- 📄 **Do not save the automatic Manual-to-Smart revert at ManModeExit** — The revert at ManModeExit (78 C) is deliberately NOT saved
- 📄 **Run the engine as a Windows service with a normal-rights client window** — the fan is controlled from boot, before sign-in, without a prompt
- 📄 **No window on a silent install** — runasoriginaluser inherits the elevated shell, where normal tools cannot click it (UIPI)

## Tried and Abandoned

- **Regex-based relayout of `fancontrol.rc`** — It took control IDs for coordinates; replaced by a parser that follows the resource grammar and checks that every old control ID still exists
- **VarToStr and CompareVersion in Inno Setup Pascal** — Neither exists; replaced by VarIsNull checks, StrToVersion and ComparePackedVersion
- **Evaluating ServiceExists in the [Run] Check of each entry** — After `-i` created the service, the `sc start` entry also ran and failed with 1056; the check now reads a value stored in PrepareToInstall
- **Sign-in scheduled task (gorilla.1)** — Control started only at sign-in; replaced by the service in gorilla.2

## ⚠ Claimed But Not Verified

*Prior documents claimed these are done. No test evidence found in this diff:*

- Behaviour on ThinkPad models other than the L15 Gen 3 (dual-fan and AMD models)
- Windows 10 1809 and later (the installer allows it; only Windows 11 was used)
- The portable zip on a second computer

## Open Items

| Item | Priority | Blocks |
|------|----------|--------|
| Opening the client window sends its current values as a command, so the engine rewrites the same ini keys once | low | nothing currently |
| No in-program fan speed measurement on Windows (the Linux version has one) | medium | Percentages on other models need FanLevelRpm= typed by hand |
| Tests on other ThinkPad models and on Windows 10 | medium | Claims of support beyond the L15 Gen 3 |

## How to verify this work is correct

**Step 1:**
```bash
Build: `MSBuild fancontrol\fancontrol.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143`, then `ISCC installer\GorillaTPFanControl.iss`
```
  - **Pass:** `fancontrol\Release\TPFanControl.exe` and `installer\output\Gorilla-TPFanControl-2.5.1-gorilla.2-Setup.exe`, ISCC exit 0
  - **Fail:** A compile error, or ISCC reporting an unknown identifier in [Code]

**Step 2:**
```bash
Install on a ThinkPad, then `Get-CimInstance Win32_Service -Filter "Name='TPFanControl'"`
```
  - **Pass:** State Running, StartMode Auto, StartName LocalSystem, PathName ending in `TPFanControl.exe" -s`
  - **Fail:** No service, or a service stuck in Stopped

**Step 3:**
```bash
In the window, select Gorilla with 45 C and level 5; then `Select-String 'C:\Program Files\Gorilla TPFanControl\TPFanControl.ini' -Pattern '^Active='`
```
  - **Pass:** `Active=4`, and the log line `Change Mode from ...->Gorilla: keep the CPU at 45 C, never below level 5`
  - **Fail:** The radio returns to the previous mode after about 5 seconds, or Active= keeps its old value

**Step 4:**
```bash
Set `Log2File=1`, restart the service, then `Stop-Service TPFanControl` and read the end of `TPFanControl.log`
```
  - **Pass:** `On close: Set fan control to 0x80, Result: [i=0] OK`
  - **Fail:** No 0x80 line: the fan keeps the last level after the service stops

**Step 5:**
```bash
Uninstall with `unins000.exe /VERYSILENT` and query `sc.exe query TPFanControl`
```
  - **Pass:** Exit 1060 (no such service); `TPFanControl.ini` kept; PawnIO still in Installed apps
  - **Fail:** The service still listed, or the ini deleted


## Glossary

**EC** — The ThinkPad embedded controller; its fan register takes levels 0-7, 0x40 (disengaged, full speed) and 0x80 (BIOS control).

**Staircase rule** — Upstream's rule that sets level 7 first and engages 0x40 only once the fan passes 4000 rpm.

**FCSHARED** — The shared-memory block `Global\TPFanControl_State` through which the client window and the service engine exchange state and commands (cmdSeq/ackSeq).

**UIPI** — User Interface Privilege Isolation: Windows blocks input from normal-rights programs to windows running with administrator rights.

**DLU** — Dialog units, the font-relative coordinates of Win32 dialog resources.

**PawnIO** — A signed kernel driver (2.2.0, GPL-2.0) that runs the LpcACPIEC module to reach the EC ports 0x62/0x66.

## Technical Debt

🟡 **LOW** — Gorilla settings travel in every client command (cmdGorillaTarget/cmdGorillaFloor), not only when changed → Send -1 for unchanged fields, which the engine already treats as unchanged
🟡 **LOW** — Client start-up sends a command built from the dialog's defaults → Skip SendCommand until the first PullSharedState has filled the dialog

## Claim Sources

| Claim | Basis | Evidence |
|-------|-------|----------|
| The EC accepts only discrete levels, so percentages must be labels | 📄 stated in input | The EC takes only levels 0-7, 0x40 (disengaged, full) and 0x80 |
| Full speed on the L15 stays at level 7 in practice | 📄 stated in input | 0x40 is only engaged above 4000 rpm, so on the L15 "full speed" in practice stays at level 7 |
| Uninstall hands the fan to the BIOS | 📄 stated in input | stopping the service logs "On close: Set fan control to 0x80, Result: OK" |
| The start-up command is harmless | 🤖 model inference | *(none — model judgment)* |
| Other models are the main untested risk | 🤖 model inference | *(none — model judgment)* |
| Sending -1 for unchanged Gorilla fields would work with the current engine | 🤖 model inference | *(none — model judgment)* |


---
**How to verify this document:**
`📄 stated in input` — the model's phrasing of something your source text said.
Find the matching line in the original to verify.
`🤖 model inference` — the model's own judgment or synthesis. Treat as opinion,
not measurement. Re-run on the same input and check whether specific numbers
stay consistent between runs.

*Session record. Developer track. Covers work done, not current code state.*