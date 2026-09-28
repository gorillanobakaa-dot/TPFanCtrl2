# Gorilla TPFanControl for Linux 0.1.0: the same fan control, built for Debian and Fedora — Plain Language

> Session record generated 2026-09-28

---

## What happened

You use Debian, and you want the same fan control on Linux that you now have on Windows: the same four modes, the same window, and the same Gorilla mode that keeps a ThinkPad at 40-50 °C. The Windows program cannot move over as it is. It reaches the fan through a Windows driver and draws a Windows-only window.

Linux already contains a driver for the ThinkPad fan, called `thinkpad_acpi`, which accepts the same fan levels. This work is a new program for Linux that repeats the Windows fan decisions step by step and uses that driver. It comes as a `.deb` package for Debian, Ubuntu and Mint, and as an `.rpm` package for Fedora and openSUSE.

It was built and tested on the Windows laptop with a pretend ThinkPad. It has not yet run on Linux or on a real ThinkPad fan. Your Debian computer is not a ThinkPad, so on it the program can show that it installs and opens, but it cannot drive a fan.

## Honest state of play

Finished and tested on Windows with a pretend ThinkPad: the fan decisions (15 of 15 automatic tests pass), the window, switching modes, saving your choices, and the Measure button. The Linux program file and both packages are built, and their contents were opened and checked line by line.

Not tested at all yet: running on Linux, installing the packages, the window under a Linux desktop, and driving a real ThinkPad fan. That is why this release is marked as a pre-release and version 0.1.0.

## Worst case if something is wrong

The worst case is on a real ThinkPad running Linux, where something in the untested path goes wrong. Example: the service starts but cannot write the fan, and the window shows Gorilla mode while the fan stays at the level the BIOS chose. The laptop would run at its normal BIOS temperature, not cooler. The kernel's watchdog also hands the fan back to the BIOS after two minutes without a command from the service, and stopping the service hands it back at once, so a stuck high or stopped fan is not expected.

## What changed for you

**Fan control on Linux**
- Before: Only the Windows version existed.
- After:  A Linux version with the same modes: BIOS, Smart, Manual and Gorilla.
- Affects: ThinkPad owners who use Linux

**Installing**
- Before: Nothing to install on Linux.
- After:  One package file (`.deb` or `.rpm`) installs the program, turns on the kernel's fan setting (`fan_control=1`), and starts the fan service at boot.
- Affects: everyone who installs it

**The window**
- Before: Only on Windows.
- After:  The same layout on Linux: temperatures, fan state, modes, log, and a title bar where minimise lights up bright blue. It opens from the applications menu without an administrator password.
- Affects: everyone who installs it

**Percentages**
- Before: On Windows you type nine measured speeds into the settings file by hand.
- After:  The Linux window has a Measure fan speeds button that runs each level for 20 seconds and saves the speeds itself.
- Affects: Linux users

## What you can do now

- Install the `.deb` on Debian, Ubuntu or Mint, or the `.rpm` on Fedora or openSUSE, with one command.
- Open Gorilla TPFanControl from the applications menu and see the temperatures and the fan, without an administrator password.
- Choose BIOS, Smart, Manual or Gorilla mode, and have the choice kept after a restart.
- Measure your own fan's speeds with one button, so that the lists show real percentages.
- Remove it with `apt remove` or `dnf remove` and have the fan handed back to the BIOS.

## What is still missing

- **A first run on Linux** — Until someone installs it on Linux, nobody knows for certain that the packages install without errors and that the window opens.
- **A test on a real ThinkPad under Linux** — Until then, the claim that it controls the fan rests on the tests with the pretend ThinkPad and on the kernel driver's documented commands.
- **A notification-area icon** — The window has no tray icon. Closing it is safe: the service keeps controlling the fan in the background.
- **Smaller extras from Windows (hotkeys, Fahrenheit, a second Smart curve)** — You cannot switch modes with keyboard shortcuts or show temperatures in Fahrenheit.

## How to check that what we say is done actually works

**Step 1:**
```bash
On your Debian computer, install the package: `sudo apt install ./gorilla-tpfancontrol_0.1.0-1_amd64.deb`
```
  - **Pass:** It installs without an error and prints that there is no ThinkPad fan interface, because that computer is not a ThinkPad.

**Step 2:**
```bash
Run `systemctl status gorilla-fan`
```
  - **Pass:** The service shows as active (running).

**Step 3:**
```bash
Open Gorilla TPFanControl from the applications menu.
```
  - **Pass:** The window opens, and the bottom line explains that there is no ThinkPad fan interface and the service changes nothing.

**Step 4:**
```bash
Point at the minimise button at the top right of the window.
```
  - **Pass:** It turns bright blue.

**Step 5:**
```bash
Remove it with `sudo apt purge gorilla-tpfancontrol`, then run `systemctl status gorilla-fan`
```
  - **Pass:** The package is gone and systemd reports that the unit could not be found.


## Should you be concerned?

Not on your Debian computer: without a ThinkPad fan the service never writes anything, and removing the package leaves nothing behind. On a real ThinkPad, treat 0.1.0 as a first test. The fan decisions match the Windows version, which is tested, but the Linux path from the program to the fan has not run yet. Watch the window for the first few minutes, and check that the Speed line follows the mode you pick.

## Glossary

**thinkpad_acpi** — The part of the Linux kernel that talks to ThinkPad hardware, including the fan.

**fan_control=1** — A setting that lets programs set the fan through `thinkpad_acpi`; the package turns it on.

**.deb and .rpm** — Package files: `.deb` for Debian-family systems, `.rpm` for Fedora-family systems.

**systemd service** — A program that Linux starts at boot and keeps running in the background.

**Watchdog** — A kernel timer that hands the fan back to the BIOS if the service goes quiet for two minutes.

**Pre-release** — A GitHub label that marks a release as not yet proven.

## Claim Sources

| Claim | Basis | Evidence |
|-------|-------|----------|
| The Windows program cannot be ported directly | 📄 stated in input | it reaches the embedded controller through a Windows driver (PawnIO) and is a Win32 dialog |
| The Linux fan driver accepts the same levels | 📄 stated in input | with the same levels (0-7, disengaged/full-speed = 0x40, auto = 0x80) |
| Nothing has run on Linux yet | 📄 stated in input | Nothing has run on Linux yet |
| The service never writes on a computer that is not a ThinkPad | 📄 stated in input | Without /proc/acpi/ibm/fan the service stays idle and never writes |
| A stuck high or stopped fan is not expected | 🤖 model inference | *(none — model judgment)* |
| Removing the package leaves nothing behind on the Debian computer | 🤖 model inference | *(none — model judgment)* |


---
**How to verify this document:**
`📄 stated in input` — the model's phrasing of something your source text said.
Find the matching line in the original to verify.
`🤖 model inference` — the model's own judgment or synthesis. Treat as opinion,
not measurement. Re-run on the same input and check whether specific numbers
stay consistent between runs.

*Session record. Plain-language track. Its developer twin covers the same session in technical detail.*