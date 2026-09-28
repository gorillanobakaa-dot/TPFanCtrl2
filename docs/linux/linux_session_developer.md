# Gorilla TPFanControl for Linux 0.1.0: thinkpad_acpi service, egui window, cross-built .deb and .rpm

> Session record generated 2026-09-28

---

## Problem Being Solved

Provide the modes of Windows 2.5.1-gorilla.2 (BIOS, Smart, Manual, Gorilla) and the same window on Linux, packaged as `.deb` and `.rpm`, built from the Windows laptop. The Win32 code cannot be reused: it reaches the EC through PawnIO and draws a Win32 dialog.

## Approach Taken

A new Rust crate in `linux/` with one binary, `gorilla-fan`, in two roles: `gorilla-fan daemon` (root, systemd) reads `/proc/acpi/ibm/thermal` and `/proc/acpi/ibm/fan` and writes `level N|auto|disengaged` and `watchdog 120`; plain `gorilla-fan` is an eframe 0.32 (glow) window that talks to the daemon over one-line TCP requests on `127.0.0.1:47811`. The fan logic in `src/core.rs` is a port of `fanstuff.cpp` (SmartControl, GorillaControl, the Manual revert) with no OS dependencies, so `cargo test` runs it anywhere. A `SimFan` backend lets the whole daemon and window run on Windows.

## Before

Commit `a6b449f`: Windows only.

## After

Commit `8c6e93c`: `linux/` with `src/{main,core,fan,daemon,proto,gui}.rs`, packaging files, `build-linux.ps1`. 15 unit tests pass on Windows. The daemon and window ran end to end on Windows against `--simulate`. A Linux ELF (`x86_64`, DT_NEEDED libc/libm/libpthread/libdl only, highest symbol GLIBC_2.28) and the `.deb` and `.rpm` are built and inspected. Nothing has executed on Linux: this is built and statically inspected, not run.

## Known Alternatives

A Unix domain socket for window-to-service traffic (more usual on Linux; not taken so the same code runs on Windows for testing); porting the Win32 program with Wine (not considered viable for a root service writing kernel files); declaring the graphics libraries as Depends (not taken, so the service installs on machines without a desktop); making `/etc/gorilla-fan.conf` a dpkg conffile (not taken, because the service rewrites it and dpkg would prompt on every upgrade).

## Files Changed

| File | Change | What Changed | Why |
|------|--------|--------------|-----|
| `linux/src/core.rs` | added | Config parser and `rewrite_keys`, `level_label`, `Controller::step` with smart/manual/gorilla, `/proc` text parsers; 13 tests | The fan logic, identical to Windows and testable without Linux |
| `linux/src/fan.rs` | added | `Fan` trait; `ProcFan` (thinkpad_acpi) with a `problem()` check for a missing interface or `fan_control` not `Y`; `SimFan` with the L15 rpm table and a heat model | Real and simulated hardware behind one interface |
| `linux/src/daemon.rs` | added | 1 s read loop, decisions every `Cycle`, 30 s watchdog refresh, calibration state machine, TCP server, `release` subcommand, log ring of 200 lines, localtime via `localtime_r` | The root service |
| `linux/src/proto.rs` | added | STATE/SET/CALIBRATE text protocol with range checks; 2 tests | Window to service |
| `linux/src/gui.rs` | added | Undecorated eframe viewport with a drawn title bar (minimise hover `#00B4FF`, close `#E81123`), temperatures grid, mode radios, level combos, measure button, log | The same window as Windows |
| `linux/Cargo.toml` | added | eframe 0.32 (glow, x11, wayland); release profile with LTO; `package.metadata.deb` and `package.metadata.generate-rpm` | Build and packaging metadata |
| `linux/packaging/*` | added | `gorilla-fan.service` (ExecStopPost release), modprobe option, default settings, desktop entry, SVG icon, deb and rpm maintainer scripts | System integration |
| `linux/build-linux.ps1` | added | cargo test, cargo zigbuild --target x86_64-unknown-linux-gnu.2.28, cargo deb, cargo generate-rpm, SHA-256 list | Reproducible cross-build from Windows |

## Decisions Made

- 📄 **Port the logic, not the code** — Linux already has a kernel driver for the ThinkPad fan, thinkpad_acpi, with the same levels
- 📄 **TCP on loopback, not a Unix socket** — so the service and the window can run and be tested on Windows too, with a simulated ThinkPad
- 📄 **Any local user may change the mode** — as with the Windows version, whose shared memory is open to all users
- 📄 **Graphics libraries as Recommends** — so the service can be installed on a machine without a desktop
- 📄 **Link against glibc 2.28** — Debian 10 / RHEL 8 and newer
- 📄 **Skip level 0 during calibration** — Level 0 is recorded as 0 rpm and not run (it stops the fan)

## Tried and Abandoned

- **Hard-coded `target/x86_64-unknown-linux-gnu/release` asset paths** — cargo-deb refuses them; it rewrites `target/release` for `--target` itself
- **Formatting log messages from the guarded state inside `s.say(...)`** — E0502 (mutable and immutable borrow of the MutexGuard); messages are now formatted first
- **A 440 px tall window** — The Manual combo popup was clipped at the bottom edge; now 560 px
- **cargo-generate-rpm automatic requirements (ldd scan)** — Windows has no ldd for a Linux binary; auto-req is off and the requirements are declared: glibc >= 2.28, systemd, kmod

## ⚠ Claimed But Not Verified

*Prior documents claimed these are done. No test evidence found in this diff:*

- The binary runs on Linux (built and inspected only)
- The packages install, and the maintainer scripts behave (contents inspected only)
- thinkpad_acpi accepts the written commands on a real ThinkPad
- The kernel watchdog and ExecStopPost hand the fan back to the BIOS
- The window opens under X11 and Wayland with the dlopen-loaded libraries
- The thinkpad_acpi reload in postinst succeeds while the module is in use

## Open Items

| Item | Priority | Blocks |
|------|----------|--------|
| First install and run on the owner's Debian machine (not a ThinkPad): install, service active, window opens, purge | high | Any claim that the packages work |
| A run on a ThinkPad under Linux | high | Promoting 0.1.0 from pre-release |
| Tray icon, hotkeys, Fahrenheit, second Smart profile, sensor offsets, erratic-sensor guard | low | nothing currently |
| Restrict SET to a group (for example `gorilla-fan`) if multi-user machines matter | low | nothing currently |

## How to verify this work is correct

**Step 1:**
```bash
`cd linux && cargo test --release`
```
  - **Pass:** `test result: ok. 15 passed; 0 failed`
  - **Fail:** Any failed test names the mode or parser that differs from the Windows behaviour

**Step 2:**
```bash
`pwsh linux/build-linux.ps1` (Windows with zig and the three cargo tools)
```
  - **Pass:** `dist/` holds `gorilla-tpfancontrol_0.1.0-1_amd64.deb`, `gorilla-tpfancontrol-0.1.0-1.x86_64.rpm` and `gorilla-fan-linux-x86_64` with SHA-256 values
  - **Fail:** A zig link error, or cargo-deb rejecting the asset paths

**Step 3:**
```bash
On Debian: `sudo apt install ./gorilla-tpfancontrol_0.1.0-1_amd64.deb && systemctl status gorilla-fan && journalctl -u gorilla-fan -n 20`
```
  - **Pass:** Service active; the journal says `No ThinkPad fan interface (/proc/acpi/ibm/fan)` on a non-ThinkPad, or `Kernel watchdog armed` on a ThinkPad
  - **Fail:** `status=203/EXEC` (binary does not start) or `Cannot listen on 127.0.0.1:47811`

**Step 4:**
```bash
On a ThinkPad: pick Gorilla in the window, then `cat /proc/acpi/ibm/fan`
```
  - **Pass:** `level:` shows 7 or `disengaged` when the CPU is at or above the target
  - **Fail:** `level: auto` while the window shows Gorilla, or `Set fan control ... FAILED` in the journal

**Step 5:**
```bash
`sudo systemctl stop gorilla-fan && cat /proc/acpi/ibm/fan`
```
  - **Pass:** `level: auto` (ExecStopPost ran `gorilla-fan release`)
  - **Fail:** The last level stays set after the stop


## Glossary

**thinkpad_acpi** — The Linux kernel driver for ThinkPad hardware; `/proc/acpi/ibm/fan` accepts `level 0-7|auto|disengaged|full-speed` and `watchdog 0-120` when loaded with `fan_control=1`.

**Staircase rule** — Set level 7 first and use disengaged (0x40) only above 4000 rpm; kept from upstream in Smart, Manual and Gorilla.

**cargo-zigbuild** — A cargo wrapper that links Linux targets with zig and can pin the glibc version (`.2.28` suffix).

**dlopen** — Loading a shared library at run time, so it is not a link-time dependency (DT_NEEDED).

**SimFan** — The simulated ThinkPad backend (`--simulate`) used for tests without hardware.

## Technical Debt

🟡 **LOW** — The daemon holds one mutex across reading, deciding and writing each second → Acceptable at one request per second from the window; revisit if more clients appear
🟠 **MEDIUM** — No signal handling in the daemon; the fan hand-back relies on systemd ExecStopPost and the kernel watchdog → Add SIGTERM handling that writes `level auto` before exit, as a third layer
🟡 **LOW** — `now_text` declares `struct tm` by hand for glibc x86-64 → Keep the crate x86-64 glibc only, or add a date crate if other targets are added

## Claim Sources

| Claim | Basis | Evidence |
|-------|-------|----------|
| The binary hard-links only the C library | 📄 stated in input | DT_NEEDED = libm.so.6, libc.so.6, libpthread.so.0, libdl.so.2 |
| The logic matches the Windows program | 📄 stated in input | The fan logic is ported line by line from fanstuff.cpp |
| Nothing has run on Linux | 📄 stated in input | Nothing has run on Linux yet |
| The mutex design is adequate at current load | 🤖 model inference | *(none — model judgment)* |
| SIGTERM handling would add a useful third safety layer | 🤖 model inference | *(none — model judgment)* |
| The most likely first-run failures are start-up and port binding | 🤖 model inference | *(none — model judgment)* |


---
**How to verify this document:**
`📄 stated in input` — the model's phrasing of something your source text said.
Find the matching line in the original to verify.
`🤖 model inference` — the model's own judgment or synthesis. Treat as opinion,
not measurement. Re-run on the same input and check whether specific numbers
stay consistent between runs.

*Session record. Developer track. Covers work done, not current code state.*