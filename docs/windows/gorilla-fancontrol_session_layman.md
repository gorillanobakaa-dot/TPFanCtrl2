# Gorilla TPFanControl for Windows: a cooler ThinkPad, a usable window, and an installer — Plain Language

> Session record generated 2026-09-28

---

## What happened

You own a ThinkPad and you want it kept cool, between 40 and 50 °C, so that it lasts for many years. Fan noise does not matter to you. The program that controls the fan, TPFanControl (version 2.5.1 from mews-se), controls the fan correctly. The problems are around it: its window hides half of the temperatures, you cannot minimise it properly, it forgets your manual fan setting at every restart, and you have to start it by hand as an administrator.

This work keeps the fan logic that already works and changes everything around it. It adds a fourth mode, Gorilla mode, which puts cooling first. It also adds an installer that sets the program up as a Windows service, so the fan is under control from the moment the computer starts.

The result is two releases on GitHub: `v2.5.1-gorilla.1` (window, installer, portable zip) and `v2.5.1-gorilla.2` (Gorilla mode, levels shown as percentages, Windows service). Both were installed, used, uninstalled and reinstalled on one ThinkPad L15 Gen 3.

## Honest state of play

Done and tested on one ThinkPad L15 Gen 3 with Windows 11: the new window, saved manual settings, Gorilla mode, the level percentages, the service installer, upgrading from `gorilla.1`, the Exit command, and uninstalling and reinstalling (22 of 22 checks passed). Both downloads on GitHub match the files that were tested, byte for byte.

Not tested: any other ThinkPad model (two-fan models or AMD models), Windows 10, and the portable zip on a second computer.

Known and left as it is: opening the window writes the same settings back to the settings file once. This changes nothing and does no harm.

## Worst case if something is wrong

The worst case is on a ThinkPad model that was not tested, where the fan chip could behave differently. Example: you choose Gorilla mode on an untested model, and a level the program sets does not take effect, so the fan runs slower than the window says. The laptop would then run warmer than you expect. Two protections remain: the laptop's own firmware protects the processor from overheating, and uninstalling hands the fan back to the BIOS (the test log shows "On close: Set fan control to 0x80, Result: OK").

## What changed for you

**Temperature list**
- Before: The list was smaller than its own columns. It scrolled in both directions and showed about half of the sensors.
- After:  The list shows every sensor and all four columns, and it scales with Windows display scaling.
- Affects: everyone

**Title bar**
- Before: There was a close button but no minimise button, and the first minimise button did not light up when you pointed at it.
- After:  Minimise sends the window to the taskbar and lights up bright blue on hover. Close hides the window in the notification area; the fan control keeps running.
- Affects: everyone

**Manual setting**
- Before: A fan level you picked by hand was lost at the next restart. Clicking Manual started at level 0, which switches the fan off.
- After:  The mode and level you pick are saved in `TPFanControl.ini` and used again after a restart. Manual starts at level 4.
- Affects: everyone

**Cooling-first mode**
- Before: Only BIOS, Smart and Manual existed.
- After:  Gorilla mode keeps the processor at 40, 45 or 50 °C: full speed at that temperature and above, level 7 within 5 °C below it, and never slower than a minimum you choose.
- Affects: users who want the laptop cool and accept the noise

**Fan speed choices**
- Before: The levels were bare numbers from 0 to 7.
- After:  After you measure your fan once, each level shows its real share of full speed and its speed, for example `4  81 %  level 4, 2790 rpm`.
- Affects: everyone who records their fan speeds

**Starting the program**
- Before: You started it by hand and confirmed an administrator prompt each time.
- After:  The installer sets it up as a Windows service that starts with the computer, before anyone signs in, with no prompt. The window opens at every sign-in without administrator rights.
- Affects: everyone who uses the installer

**Copying the Status line (gorilla.3)**
- Before: You could not select or copy the line at the bottom of the window: it was rewritten every few seconds, which cleared your selection.
- After:  Click into the line and it stays still, so you can select and copy it. It starts updating again when you click elsewhere.
- Affects: everyone

## What you can do now

- Install everything, including the PawnIO driver, with one `Setup.exe` and no internet connection.
- Have the fan under control from the moment Windows starts, before you sign in, with no administrator prompt.
- Choose Gorilla mode and pick a processor temperature of 40, 45 or 50 °C and a minimum fan level.
- See each fan level as a real percentage of full speed with its rpm, once the speeds are recorded in `FanLevelRpm=`.
- Pick a manual level and find it still set after a restart.
- Minimise the window to the taskbar, or close it to the notification area while the fan control keeps running.
- Uninstall it from Settings > Apps and have the fan handed back to the BIOS.
- Copy the Status line at the bottom of the window: click into it, select, and copy.

## What is still missing

- **Tests on other ThinkPad models** — On a model other than the L15 Gen 3 you are the first tester. Watch the speed in the window after you change a mode.
- **A button that measures the fan speeds on Windows** — To see percentages on your own ThinkPad, you enter nine measured speeds in `FanLevelRpm=` by hand. The Linux version has a button for this; the Windows version does not yet.
- **A test on Windows 10** — The installer accepts Windows 10 version 1809 and later, but only Windows 11 was used.

## How to check that what we say is done actually works

**Step 1:**
```bash
Run `Gorilla-TPFanControl-2.5.1-gorilla.2-Setup.exe` on the ThinkPad and restart the computer.
```
  - **Pass:** Before you sign in, the fan already behaves as your chosen mode says. After you sign in, the fan icon appears in the notification area without an administrator prompt.

**Step 2:**
```bash
Open the window, choose Manual and pick level 4, then restart the computer and open the window again.
```
  - **Pass:** Manual is still selected with level 4, and the State line reads `0x04 (Fan Level 4, Non Bios)`.

**Step 3:**
```bash
Choose Gorilla (cooling first) with 45 °C and a minimum of level 5, then watch the Speed line for one minute.
```
  - **Pass:** The log shows `Change Mode from Manual->Gorilla: keep the CPU at 45 C, never below level 5`. When the processor is at 45 °C or above, the speed rises to the level 7 figure (about 3,400 rpm on an L15 Gen 3).

**Step 4:**
```bash
Point at the minimise button in the window's title bar.
```
  - **Pass:** The button turns bright blue.

**Step 5:**
```bash
Uninstall Gorilla TPFanControl from Settings > Apps > Installed apps.
```
  - **Pass:** A message says the fan is back under BIOS control, and the fan returns to its normal Windows behaviour.


## Should you be concerned?

Only if you run it on a ThinkPad model other than the L15 Gen 3. The fan logic for BIOS, Smart and Manual is unchanged upstream code that other people already use. Gorilla mode is new and was tested on one laptop. It only chooses levels the fan chip accepts, and it runs full speed above 78 °C whatever you set. Gorilla mode deliberately runs the fan fast and loud. That is its purpose, not a fault.

## Glossary

**Fan level** — One of the fixed speeds the ThinkPad fan chip accepts: 0 (off) to 7, plus full speed and 'BIOS decides'.

**BIOS** — The laptop's built-in firmware, which runs the fan by itself when no program controls it.

**Windows service** — A program that Windows starts by itself at boot and runs in the background without anyone signing in.

**PawnIO** — A signed driver that lets the program talk to the laptop's fan chip.

**rpm** — Revolutions per minute: how fast the fan turns.

**Hysteresis** — A small gap between the temperature where the fan speeds up and the one where it slows down again, so it does not keep switching back and forth.

## Claim Sources

| Claim | Basis | Evidence |
|-------|-------|----------|
| The fan logic already worked and was kept | 📄 stated in input | BIOS, Smart and Manual stay upstream code |
| Level 7 is about full speed on the L15 Gen 3 | 📄 stated in input | Level 7 is already about full speed |
| Gorilla mode steers by the processor sensor because another sensor is stuck | 📄 stated in input | the L15's 'pwr' sensor reads 66 C (or 0) whatever the fan does |
| Uninstalling hands the fan back to the BIOS | 📄 stated in input | stopping the service logs "On close: Set fan control to 0x80, Result: OK" |
| Other ThinkPad models are the main risk | 🤖 model inference | *(none — model judgment)* |
| The firmware's own overheating protection remains in place | 🤖 model inference | *(none — model judgment)* |
| Opening the window rewrites the settings once and does no harm | 📄 stated in input | Opening the window rewrites the same values into the ini once |
| The Status line can now be copied | 📄 stated in input | it stayed for three cycles, fully selected, and WM_COPY put it on the clipboard |


---
**How to verify this document:**
`📄 stated in input` — the model's phrasing of something your source text said.
Find the matching line in the original to verify.
`🤖 model inference` — the model's own judgment or synthesis. Treat as opinion,
not measurement. Re-run on the same input and check whether specific numbers
stay consistent between runs.

*Session record. Plain-language track. Its developer twin covers the same session in technical detail.*