//! Where the service reads temperatures and writes the fan.
//!
//! `ProcFan` is the real one: the kernel's thinkpad_acpi driver, through
//! /proc/acpi/ibm/fan and /proc/acpi/ibm/thermal. Writing the fan needs the
//! driver loaded with fan_control=1 (the package sets that in
//! /etc/modprobe.d/gorilla-fan.conf).
//!
//! `SimFan` is a pretend ThinkPad (an L15 Gen 3's measured fan speeds and a
//! simple heat model), so the service and the window can be run and tested on
//! any computer: `gorilla-fan daemon --simulate`.

use crate::core::{self, Reading, BIOS, FULL};
use std::path::PathBuf;

pub trait Fan: Send {
    fn read(&mut self) -> Result<Reading, String>;
    fn write(&mut self, level: i32) -> Result<(), String>;
    /// Ask the kernel to hand the fan back to the BIOS by itself if this
    /// service stops writing for `secs` seconds (a hang or a crash).
    fn arm_watchdog(&mut self, secs: u32) -> Result<(), String>;
    fn name(&self) -> &'static str;
}

pub struct ProcFan {
    pub root: PathBuf,
}

impl ProcFan {
    fn fan_path(&self) -> PathBuf {
        self.root.join("acpi/ibm/fan")
    }
    fn thermal_path(&self) -> PathBuf {
        self.root.join("acpi/ibm/thermal")
    }

    /// Why this computer cannot be driven, or None when it can.
    pub fn problem(&self, sys_root: &std::path::Path) -> Option<String> {
        if !self.fan_path().exists() {
            return Some(
                "No ThinkPad fan interface (/proc/acpi/ibm/fan): this is not a ThinkPad, or the thinkpad_acpi driver is not loaded. \
                 The service stays idle and changes nothing."
                    .into(),
            );
        }
        let p = sys_root.join("module/thinkpad_acpi/parameters/fan_control");
        if let Ok(v) = std::fs::read_to_string(&p) {
            if !v.trim().eq_ignore_ascii_case("y") && v.trim() != "1" {
                return Some(
                    "The thinkpad_acpi driver is loaded without fan_control=1, so the fan cannot be set yet. \
                     Restart the computer once (the package set the option), or run: sudo modprobe -r thinkpad_acpi && sudo modprobe thinkpad_acpi"
                        .into(),
                );
            }
        }
        None
    }
}

impl Fan for ProcFan {
    fn read(&mut self) -> Result<Reading, String> {
        let fan = std::fs::read_to_string(self.fan_path()).map_err(|e| format!("reading {}: {}", self.fan_path().display(), e))?;
        let (level, rpm) = core::parse_proc_fan(&fan).ok_or("the fan file has no level line")?;
        let temps = std::fs::read_to_string(self.thermal_path()).map(|t| core::parse_proc_thermal(&t)).unwrap_or([None; 12]);
        Ok(Reading { temps, level, rpm })
    }

    fn write(&mut self, level: i32) -> Result<(), String> {
        std::fs::write(self.fan_path(), core::proc_fan_command(level)).map_err(|e| format!("writing {}: {}", self.fan_path().display(), e))
    }

    fn arm_watchdog(&mut self, secs: u32) -> Result<(), String> {
        std::fs::write(self.fan_path(), format!("watchdog {}", secs)).map_err(|e| e.to_string())
    }

    fn name(&self) -> &'static str {
        "thinkpad_acpi"
    }
}

/// A pretend ThinkPad for testing without one.
pub struct SimFan {
    level: i32,
    rpm: f64,
    cpu: f64,
    tick: u64,
}

/// Measured on a ThinkPad L15 Gen 3 (2026-09-28): levels 0-7, then full speed.
const SIM_RPM: [f64; 9] = [0.0, 1765.0, 2179.0, 2586.0, 2790.0, 2999.0, 3197.0, 3423.0, 3445.0];

impl SimFan {
    pub fn new() -> SimFan {
        SimFan { level: BIOS, rpm: 1800.0, cpu: 52.0, tick: 0 }
    }
}

impl Default for SimFan {
    fn default() -> Self {
        Self::new()
    }
}

impl Fan for SimFan {
    fn read(&mut self) -> Result<Reading, String> {
        self.tick += 1;
        let target_rpm = match self.level {
            BIOS => 1800.0,
            FULL => SIM_RPM[8],
            l => SIM_RPM[l.clamp(0, 7) as usize],
        };
        self.rpm += (target_rpm - self.rpm) * 0.5;
        // heat in: a load that comes and goes; heat out: grows with airflow
        let load = if (self.tick / 60) % 2 == 0 { 14.0 } else { 8.0 };
        let equilibrium = 38.0 + load * (1.6 - self.rpm / 3445.0);
        self.cpu += (equilibrium - self.cpu) * 0.08;
        let c = self.cpu.round() as i32;
        let mut temps = [None; 12];
        for (i, t) in temps.iter_mut().enumerate().take(11) {
            *t = match i {
                0 => Some(c),
                1 | 3 | 7 | 8 => None,
                10 => Some(66), // the 'pwr' sensor that never moves, as on the real L15
                _ => Some(c - 1),
            };
        }
        Ok(Reading { temps, level: self.level, rpm: self.rpm.round() as i32 })
    }

    fn write(&mut self, level: i32) -> Result<(), String> {
        self.level = level;
        Ok(())
    }

    fn arm_watchdog(&mut self, _secs: u32) -> Result<(), String> {
        Ok(())
    }

    fn name(&self) -> &'static str {
        "simulated"
    }
}
