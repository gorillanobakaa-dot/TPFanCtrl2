# Gorilla TPFanControl for Linux

Fan control for Lenovo ThinkPads on Linux, with the same four modes and the
same window as the Windows version (`../README.md`):

- **BIOS**: the laptop decides, as if nothing were installed.
- **Smart**: a temperature curve (`Level=` lines in `/etc/gorilla-fan.conf`),
  steered by the hottest sensor.
- **Manual**: one fixed level, kept after a restart.
- **Gorilla (cooling first)**: keep the CPU at 40, 45 or 50 °C. Full speed
  at that temperature and above, level 7 within 5 °C below it, and never
  slower than the minimum you pick. Noise is not a factor.

It is a Rust program in two roles:

| Part | What it does | Rights |
|---|---|---|
| `gorilla-fan daemon` (`gorilla-fan.service`) | Reads the temperatures every second, decides every 5 seconds, writes the fan through the kernel's `thinkpad_acpi` driver (`/proc/acpi/ibm/fan`). Starts at boot. | root (systemd) |
| `gorilla-fan` (the window, "Gorilla TPFanControl" in the menu) | Shows the temperatures, the fan and the log; changes the mode. Talks to the service on `127.0.0.1:47811`. | none |

## Install

Debian, Ubuntu, Mint:

```bash
sudo apt install ./gorilla-tpfancontrol_0.1.0-1_amd64.deb
```

Fedora, openSUSE, RHEL:

```bash
sudo dnf install ./gorilla-tpfancontrol-0.1.0-1.x86_64.rpm
```

The package:

- installs `/etc/modprobe.d/gorilla-fan.conf` (`options thinkpad_acpi fan_control=1`), because the driver only lets programs set the fan with that option;
- reloads `thinkpad_acpi` with it (if the driver is busy, restart once);
- enables and starts `gorilla-fan.service`.

On first start the service creates `/etc/gorilla-fan.conf` from `/usr/share/gorilla-fan/gorilla-fan.conf.default`.

**Not a ThinkPad?** It still installs, but the service finds no
`/proc/acpi/ibm/fan`, says so in the window, and never writes anything.

## Safety

- Above `ManModeExit` (78 °C), Manual switches back to Smart and Gorilla runs full speed.
- The kernel watchdog (`watchdog 120`) hands the fan to the BIOS if the service hangs for two minutes.
- Whenever the service stops (stop, crash, uninstall), systemd runs `gorilla-fan release`, which hands the fan to the BIOS.

## Percentages

The fan chip takes only levels 0-7 and "full speed", so there is no free
percentage setting. The **Measure fan speeds** button runs each level for 20
seconds and saves the speeds as `FanLevelRpm=`. After that, the lists show each level
as a real percentage of full speed, e.g. `4  81 %  level 4, 2790 rpm`. The
measurement stops by itself if the laptop passes 78 °C.

## Useful commands

```bash
systemctl status gorilla-fan
```

```bash
journalctl -u gorilla-fan -f
```

```bash
cat /proc/acpi/ibm/fan
```

Remove it with `sudo apt remove gorilla-tpfancontrol` (keeps your settings), or `sudo apt purge gorilla-tpfancontrol` (also deletes them). On Fedora, use `sudo dnf remove gorilla-tpfancontrol`. Either way the fan goes back to the BIOS.

## Build

The packages are cross-built on Windows with cargo-zigbuild; see `build-linux.ps1`. Test on any computer with a pretend ThinkPad:

```bash
cargo test
```

```bash
cargo run -- daemon --simulate --config /tmp/gorilla-fan.conf
```

and in a second terminal:

```bash
cargo run
```
