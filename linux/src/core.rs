//! The fan logic, free of any operating system: the settings file, the level
//! labels, and the four modes. Ported from the Windows program
//! (fancontrol/fanstuff.cpp: SmartControl, GorillaControl, HandleData) so both
//! behave the same. Tested with `cargo test` on any computer.

use std::fmt::Write as _;

/// Fan levels as the ThinkPad embedded controller knows them, and as the
/// kernel's thinkpad_acpi driver writes them: 0-7 regulated (0 stops the fan),
/// 64 (0x40) disengaged = full speed, 128 (0x80) = the BIOS decides.
pub const FULL: i32 = 64;
pub const BIOS: i32 = 128;

/// The order the level lists are shown in: fastest first.
pub const LEVEL_ORDER: [i32; 9] = [FULL, 7, 6, 5, 4, 3, 2, 1, 0];

/// Sensor names, in thinkpad_acpi's order (/proc/acpi/ibm/thermal: EC
/// registers 0x78-0x7F, then 0xC0-0xC3) - the same names the Windows version shows.
pub const SENSOR_NAMES: [&str; 12] = ["cpu", "aps", "crd", "gpu", "no5", "x7d", "bat", "x7f", "bus", "pci", "pwr", "xc3"];

pub const MODE_NAMES: [&str; 5] = ["", "BIOS", "Smart", "Manual", "Gorilla"];

/// A mode is 1 BIOS, 2 Smart, 3 Manual, 4 Gorilla (the ini's Active= values).
pub fn mode_name(mode: i32) -> &'static str {
    MODE_NAMES.get(mode as usize).copied().unwrap_or("")
}

pub fn valid_level(level: i32) -> bool {
    (0..=7).contains(&level) || level == FULL || level == BIOS
}

pub fn valid_floor(level: i32) -> bool {
    (1..=7).contains(&level) || level == FULL
}

// ---------------------------------------------------------------------------
//  settings (same key names as the Windows TPFanControl.ini)
// ---------------------------------------------------------------------------

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SmartLevel {
    pub temp: i32,
    pub fan: i32,
    pub hyst_up: i32,
    pub hyst_down: i32,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Config {
    pub active: i32,
    pub man_fan_speed: i32,
    pub man_mode_exit: i32,
    pub cycle: u64,
    pub ignore_sensors: Vec<String>,
    pub gorilla_target: i32,
    pub gorilla_floor: i32,
    pub fan_level_rpm: [i32; 9],
    pub levels: Vec<SmartLevel>,
}

impl Default for Config {
    fn default() -> Self {
        Config {
            active: 2,
            man_fan_speed: 4,
            man_mode_exit: 78,
            cycle: 5,
            ignore_sensors: Vec::new(),
            gorilla_target: 45,
            gorilla_floor: 5,
            fan_level_rpm: [0; 9],
            // upstream's default curve (TPFanControl.ini "Smart Mode 1")
            levels: vec![
                SmartLevel { temp: 50, fan: 0, hyst_up: 0, hyst_down: 0 },
                SmartLevel { temp: 60, fan: 1, hyst_up: 0, hyst_down: 0 },
                SmartLevel { temp: 70, fan: 2, hyst_up: 0, hyst_down: 0 },
                SmartLevel { temp: 80, fan: 4, hyst_up: 0, hyst_down: 0 },
                SmartLevel { temp: 90, fan: 7, hyst_up: 0, hyst_down: 0 },
            ],
        }
    }
}

/// The value part of a `Key=value // comment` line.
fn value_of<'a>(line: &'a str, key: &str) -> Option<&'a str> {
    let rest = line.strip_prefix(key)?.strip_prefix('=')?;
    let rest = match rest.find("//") {
        Some(i) => &rest[..i],
        None => rest,
    };
    Some(rest.trim())
}

fn ints(s: &str) -> Vec<i32> {
    s.split(|c: char| c.is_whitespace() || c == ',')
        .filter(|t| !t.is_empty())
        .map_while(|t| parse_level_token(t))
        .collect()
}

/// A number as the Windows program accepts it: decimal, or 0x.. hex.
pub fn parse_level_token(t: &str) -> Option<i32> {
    let t = t.trim();
    if let Some(h) = t.strip_prefix("0x").or_else(|| t.strip_prefix("0X")) {
        i32::from_str_radix(h, 16).ok()
    } else {
        t.parse().ok()
    }
}

impl Config {
    /// Read settings text. Unknown keys and bad values are ignored, and the
    /// default stays, as in the Windows program's ReadConfig.
    pub fn parse(text: &str) -> Config {
        let mut c = Config::default();
        let mut levels = Vec::new();
        for raw in text.lines() {
            let line = raw.trim();
            if line.starts_with("//") || line.starts_with('#') {
                continue;
            }
            let one = |k: &str| value_of(line, k).and_then(parse_level_token);
            if let Some(v) = one("Active") {
                if (0..=4).contains(&v) {
                    c.active = v;
                }
            } else if let Some(v) = one("ManFanSpeed") {
                if valid_level(v) && v != BIOS {
                    c.man_fan_speed = v;
                }
            } else if let Some(v) = one("ManModeExit") {
                if (40..=100).contains(&v) {
                    c.man_mode_exit = v;
                }
            } else if let Some(v) = one("Cycle") {
                if (1..=60).contains(&v) {
                    c.cycle = v as u64;
                }
            } else if let Some(v) = one("GorillaTarget") {
                if (30..=70).contains(&v) {
                    c.gorilla_target = v;
                }
            } else if let Some(v) = one("GorillaFloor") {
                if valid_floor(v) {
                    c.gorilla_floor = v;
                }
            } else if let Some(v) = value_of(line, "IgnoreSensors") {
                c.ignore_sensors = v.split(|ch: char| ch == ',' || ch.is_whitespace()).filter(|s| !s.is_empty()).map(|s| s.to_string()).collect();
            } else if let Some(v) = value_of(line, "FanLevelRpm") {
                let n = ints(v);
                if n.len() == 9 && n.iter().all(|&r| (0..20000).contains(&r)) {
                    c.fan_level_rpm.copy_from_slice(&n);
                }
            } else if let Some(v) = value_of(line, "Level") {
                let n = ints(v);
                if n.len() >= 2 && (-1..=130).contains(&n[0]) && valid_level(n[1]) {
                    levels.push(SmartLevel { temp: n[0], fan: n[1], hyst_up: *n.get(2).unwrap_or(&0), hyst_down: *n.get(3).unwrap_or(&0) });
                }
            }
        }
        if !levels.is_empty() {
            c.levels = levels;
        }
        if c.active < 1 {
            c.active = 1; // 0 = "only read" upstream; here the service then leaves the fan to the BIOS
        }
        c
    }

    pub fn start_mode(&self) -> i32 {
        self.active.clamp(1, 4)
    }
}

/// Write `keys` into settings text: an existing `Key=` line is replaced, a
/// missing key is appended; every other byte stays as it was (comments, the
/// curve, unknown keys). The Windows program's RewriteIniKeys does the same.
pub fn rewrite_keys(text: &str, pairs: &[(&str, String)]) -> String {
    let nl = if text.contains("\r\n") { "\r\n" } else { "\n" };
    let mut done = vec![false; pairs.len()];
    let mut out = String::with_capacity(text.len() + 128);
    for line in text.split_inclusive('\n') {
        let body = line.trim_end_matches(['\r', '\n']);
        let mut replaced = false;
        for (i, (k, v)) in pairs.iter().enumerate() {
            if !done[i] && body.trim_start().strip_prefix(k).is_some_and(|r| r.starts_with('=')) {
                let _ = write!(out, "{}={}{}", k, v, &line[body.len()..]);
                done[i] = true;
                replaced = true;
                break;
            }
        }
        if !replaced {
            out.push_str(line);
        }
    }
    if done.iter().any(|d| !d) {
        if !out.is_empty() && !out.ends_with('\n') {
            out.push_str(nl);
        }
        for (i, (k, v)) in pairs.iter().enumerate() {
            if !done[i] {
                let _ = write!(out, "{}={}{}", k, v, nl);
            }
        }
    }
    out
}

// ---------------------------------------------------------------------------
//  labels: "4     81 %  level 4, 2790 rpm" (same text as the Windows lists)
// ---------------------------------------------------------------------------

fn level_index(level: i32) -> usize {
    if level == FULL { 8 } else { level.clamp(0, 7) as usize }
}

pub fn level_value_text(level: i32) -> String {
    match level {
        FULL => "0x40".into(),
        BIOS => "0x80".into(),
        l => l.to_string(),
    }
}

pub fn level_label(rpm: &[i32; 9], level: i32) -> String {
    if level == BIOS {
        return "0x80  BIOS decides".into();
    }
    let val = level_value_text(level);
    let full = rpm.iter().copied().max().unwrap_or(0);
    let r = rpm[level_index(level)];
    let what = match level {
        FULL => "full speed".to_string(),
        0 => "fan OFF".to_string(),
        l => format!("level {}", l),
    };
    if full > 0 && (r > 0 || level == 0) {
        let pct = if level == 0 { 0 } else { (r * 100 + full / 2) / full };
        format!("{:<4}  {:>3} %  {}, {} rpm", val, pct, what, if level == 0 { 0 } else { r })
    } else {
        format!("{:<4}  {}", val, what)
    }
}

// ---------------------------------------------------------------------------
//  the controller: one decision per cycle
// ---------------------------------------------------------------------------

/// What the sensors and the fan read this cycle.
#[derive(Clone, Debug, Default)]
pub struct Reading {
    /// Temperatures in degrees C, None where thinkpad_acpi reports no sensor.
    pub temps: [Option<i32>; 12],
    /// The fan level now set (0-7, FULL, BIOS).
    pub level: i32,
    pub rpm: i32,
}

pub struct Controller {
    pub cfg: Config,
    pub mode: i32,
    pub previous_mode: i32,
    pub manual_level: i32,
    /// The Manual -> Smart revert at ManModeExit is not saved (as on Windows).
    pub reverted_from_manual: bool,
    pub traces: Vec<String>,
}

impl Controller {
    pub fn new(cfg: Config) -> Controller {
        let mode = cfg.start_mode();
        let manual_level = cfg.man_fan_speed;
        Controller { cfg, mode, previous_mode: 1, manual_level, reverted_from_manual: false, traces: Vec::new() }
    }

    fn trace(&mut self, s: String) {
        self.traces.push(s);
    }

    /// The hottest sensor that counts: no missing sensors, none on the
    /// IgnoreSensors list, nothing at or above 128.
    pub fn max_temp(&self, r: &Reading) -> i32 {
        let mut m = 0;
        for (i, t) in r.temps.iter().enumerate() {
            if let Some(t) = *t {
                if t > 0 && t < 128 && !self.cfg.ignore_sensors.iter().any(|n| n == SENSOR_NAMES[i]) {
                    m = m.max(t);
                }
            }
        }
        m
    }

    /// Decide this cycle. Returns the level to write, if it differs from now.
    pub fn step(&mut self, r: &Reading) -> Option<i32> {
        let max_temp = self.max_temp(r);
        let mut want: Option<i32> = None;

        if self.previous_mode != self.mode {
            let from = mode_name(self.previous_mode);
            let msg = match self.mode {
                1 => format!("Change Mode from {}->BIOS, setting fan speed", from),
                2 => format!("Change Mode from {}->Smart, recalculate fan speed", from),
                3 => format!("Change Mode from {}->Manual, setting fan speed", from),
                _ => format!("Change Mode from {}->Gorilla: keep the CPU at {} C, never below level {}", from, self.cfg.gorilla_target,
                    if self.cfg.gorilla_floor == FULL { "full".to_string() } else { self.cfg.gorilla_floor.to_string() }),
            };
            self.trace(msg);
        }

        match self.mode {
            1 => {
                if r.level != BIOS {
                    want = Some(BIOS);
                }
            }
            2 => want = self.smart(r, max_temp),
            3 => {
                let mut v = self.manual_level;
                if v == FULL && r.level != FULL && r.rpm < 4000 {
                    v = 7; // spin up regulated first, as Smart
                }
                if v != r.level {
                    want = Some(v);
                }
            }
            _ => want = self.gorilla(r, max_temp),
        }

        self.previous_mode = self.mode;
        if self.mode == 3 && max_temp > self.cfg.man_mode_exit {
            self.trace(format!("{} C is above ManModeExit ({} C): back to Smart", max_temp, self.cfg.man_mode_exit));
            self.mode = 2;
            self.reverted_from_manual = true;
        }
        want
    }

    fn smart(&mut self, r: &Reading, max_temp: i32) -> Option<i32> {
        let mut fanctrl = r.level;
        let mut newfan: i32 = -1;
        // after BIOS/Manual/Gorilla, or from 0x80, start from the bottom (upstream)
        if fanctrl > 7 || matches!(self.previous_mode, 1 | 3 | 4) {
            fanctrl = 0;
            newfan = 0;
        }
        for l in &self.cfg.levels {
            if max_temp >= l.temp + l.hyst_up && l.fan >= fanctrl {
                newfan = l.fan;
            }
        }
        if newfan == -1 {
            for l in &self.cfg.levels {
                if max_temp <= l.temp - l.hyst_down && l.fan < fanctrl {
                    newfan = l.fan;
                    break;
                }
            }
        }
        if newfan == FULL && fanctrl != FULL && r.rpm < 4000 {
            newfan = 7;
        }
        if newfan != -1 && newfan != r.level { Some(newfan) } else { None }
    }

    /// Gorilla mode, as fanstuff.cpp GorillaControl: steer by the CPU sensor.
    fn gorilla(&mut self, r: &Reading, max_temp: i32) -> Option<i32> {
        let t = match r.temps[0] {
            Some(c) if c > 0 && c < 128 => c,
            _ => max_temp,
        };
        let target = self.cfg.gorilla_target;
        let floor = self.cfg.gorilla_floor;
        let cur = r.level;
        let hot = max_temp > self.cfg.man_mode_exit;

        let mut want = if t >= target || hot {
            FULL
        } else if t >= target - 5 {
            7
        } else {
            floor
        };
        if !hot {
            if want != FULL && cur == FULL && t > target - 2 {
                want = FULL; // hold full speed until 2 C below the target
            }
            if want != FULL && want < 7 && (cur == 7 || cur == FULL) && t > target - 7 {
                want = 7; // hold level 7 until 2 C below its band
            }
        }
        if floor == FULL {
            want = FULL;
        } else if want != FULL && want < floor {
            want = floor;
        }
        if want == FULL && cur != FULL && r.rpm < 4000 {
            want = 7; // spin up regulated first (as Smart)
        }
        if want != cur { Some(want) } else { None }
    }

    /// The settings a user choice changes, for the settings file.
    pub fn persist_pairs(&self) -> Vec<(&'static str, String)> {
        let mut p = vec![("Active", self.mode.to_string())];
        if self.mode == 3 {
            p.push(("ManFanSpeed", level_value_text(self.manual_level)));
        }
        if self.mode == 4 {
            p.push(("GorillaTarget", self.cfg.gorilla_target.to_string()));
            p.push(("GorillaFloor", self.cfg.gorilla_floor.to_string()));
        }
        p
    }
}

// ---------------------------------------------------------------------------
//  thinkpad_acpi text (/proc/acpi/ibm/fan and /proc/acpi/ibm/thermal)
// ---------------------------------------------------------------------------

/// "status: enabled / speed: 2790 / level: 4" -> (level, rpm)
pub fn parse_proc_fan(text: &str) -> Option<(i32, i32)> {
    let mut level = None;
    let mut rpm = 0;
    for line in text.lines() {
        let (k, v) = match line.split_once(':') {
            Some(kv) => kv,
            None => continue,
        };
        let v = v.trim();
        match k.trim() {
            "speed" => rpm = v.parse().unwrap_or(0),
            "level" => {
                level = match v {
                    "auto" => Some(BIOS),
                    "disengaged" | "full-speed" => Some(FULL),
                    n => n.parse().ok().filter(|l| (0..=7).contains(l)),
                }
            }
            _ => {}
        }
    }
    level.map(|l| (l, rpm))
}

/// The command thinkpad_acpi takes for a level.
pub fn proc_fan_command(level: i32) -> String {
    match level {
        BIOS => "level auto".into(),
        FULL => "level disengaged".into(),
        l => format!("level {}", l.clamp(0, 7)),
    }
}

/// "temperatures:\t45 40 -128 ..." -> up to 12 sensors (-128 = none)
pub fn parse_proc_thermal(text: &str) -> [Option<i32>; 12] {
    let mut out = [None; 12];
    if let Some(rest) = text.lines().find_map(|l| l.strip_prefix("temperatures:")) {
        for (i, t) in rest.split_whitespace().filter_map(|t| t.parse::<i32>().ok()).take(12).enumerate() {
            out[i] = if t > 0 && t < 128 { Some(t) } else { None };
        }
    }
    out
}

// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    fn reading(cpu: i32, level: i32, rpm: i32) -> Reading {
        let mut temps = [None; 12];
        temps[0] = Some(cpu);
        Reading { temps, level, rpm }
    }

    fn gorilla(target: i32, floor: i32) -> Controller {
        let mut c = Controller::new(Config { active: 4, gorilla_target: target, gorilla_floor: floor, ..Config::default() });
        c.previous_mode = 4;
        c
    }

    #[test]
    fn parses_the_owners_windows_settings() {
        let ini = "// comment\r\nActive=4\r\nManFanSpeed=4\r\nManModeExit=78 //ManModeExit=172\r\nLevel=0 4\r\nLevel=70 7\r\nGorillaTarget=45\r\nGorillaFloor=5\r\nFanLevelRpm=0 1765 2179 2586 2790 2999 3197 3423 3445\r\nIgnoreSensors=no5\r\n";
        let c = Config::parse(ini);
        assert_eq!(c.active, 4);
        assert_eq!(c.man_mode_exit, 78);
        assert_eq!(c.levels.len(), 2);
        assert_eq!(c.levels[1], SmartLevel { temp: 70, fan: 7, hyst_up: 0, hyst_down: 0 });
        assert_eq!(c.fan_level_rpm[4], 2790);
        assert_eq!(c.ignore_sensors, vec!["no5"]);
    }

    #[test]
    fn bad_values_keep_the_defaults() {
        let c = Config::parse("GorillaTarget=99\nGorillaFloor=0\nFanLevelRpm=1 2 3\nActive=9\nManFanSpeed=0x80\n");
        let d = Config::default();
        assert_eq!((c.gorilla_target, c.gorilla_floor, c.fan_level_rpm, c.active, c.man_fan_speed), (d.gorilla_target, d.gorilla_floor, d.fan_level_rpm, d.active, d.man_fan_speed));
    }

    #[test]
    fn rewrite_keeps_everything_else() {
        let t = "// keep me\r\nActive=2\r\nLevel=0 4\r\nManFanSpeed=4 // note\r\n";
        let out = rewrite_keys(t, &[("Active", "4".into()), ("GorillaFloor", "6".into())]);
        assert_eq!(out, "// keep me\r\nActive=4\r\nLevel=0 4\r\nManFanSpeed=4 // note\r\nGorillaFloor=6\r\n");
        // a key inside a comment is not touched
        assert_eq!(rewrite_keys("// Active=1\nActive=2\n", &[("Active", "3".into())]), "// Active=1\nActive=3\n");
    }

    #[test]
    fn labels_match_the_windows_program() {
        let rpm = [0, 1765, 2179, 2586, 2790, 2999, 3197, 3423, 3445];
        assert_eq!(level_label(&rpm, 4), "4      81 %  level 4, 2790 rpm");
        assert_eq!(level_label(&rpm, FULL), "0x40  100 %  full speed, 3445 rpm");
        assert_eq!(level_label(&rpm, 0), "0       0 %  fan OFF, 0 rpm");
        assert_eq!(level_label(&[0; 9], 5), "5     level 5");
        assert_eq!(level_label(&rpm, BIOS), "0x80  BIOS decides");
    }

    #[test]
    fn gorilla_full_at_target_via_level_7_on_a_slow_fan() {
        let mut c = gorilla(45, 5);
        // 46 C, fan at 5 and below 4000 rpm: full speed is wanted, 7 comes first
        assert_eq!(c.step(&reading(46, 5, 2999)), Some(7));
        // spun up past 4000 rpm: now disengaged
        assert_eq!(c.step(&reading(46, 7, 4100)), Some(FULL));
    }

    #[test]
    fn gorilla_bands_and_floor() {
        let mut c = gorilla(45, 5);
        assert_eq!(c.step(&reading(41, 5, 3000)), Some(7)); // within 5 C below target
        let mut c = gorilla(45, 5);
        assert_eq!(c.step(&reading(35, 3, 2500)), Some(5)); // below the band: the floor
        let mut c = gorilla(45, 2);
        assert_eq!(c.step(&reading(35, 2, 2100)), None); // already at the floor
    }

    #[test]
    fn gorilla_hysteresis() {
        let mut c = gorilla(45, 3);
        assert_eq!(c.step(&reading(44, FULL, 5000)), None); // holds full until 2 C below
        assert_eq!(c.step(&reading(43, FULL, 5000)), Some(7));
        assert_eq!(c.step(&reading(39, 7, 3400)), None); // holds 7 until target-7
        assert_eq!(c.step(&reading(38, 7, 3400)), Some(3));
    }

    #[test]
    fn gorilla_full_floor_and_hot_override() {
        let mut c = gorilla(45, FULL);
        assert_eq!(c.step(&reading(30, FULL, 5000)), None);
        let mut c = gorilla(50, 1);
        let mut r = reading(30, 1, 1800);
        r.temps[3] = Some(80); // another sensor above ManModeExit
        assert_eq!(c.step(&r), Some(7)); // full wanted, spun up via 7
    }

    #[test]
    fn gorilla_uses_cpu_not_a_stuck_hot_sensor() {
        let mut c = gorilla(45, 4);
        let mut r = reading(35, 4, 2800);
        r.temps[10] = Some(66); // the 'pwr' sensor stuck at 66 C
        assert_eq!(c.step(&r), None); // CPU 35 C: stays at the floor
    }

    #[test]
    fn smart_follows_the_curve_with_hysteresis() {
        let cfg = Config { active: 2, levels: vec![
            SmartLevel { temp: 50, fan: 0, hyst_up: 0, hyst_down: 0 },
            SmartLevel { temp: 60, fan: 1, hyst_up: 0, hyst_down: 5 },
            SmartLevel { temp: 80, fan: 4, hyst_up: 5, hyst_down: 0 },
        ], ..Config::default() };
        let mut c = Controller::new(cfg);
        c.previous_mode = 2;
        assert_eq!(c.step(&reading(62, 0, 0)), Some(1));
        assert_eq!(c.step(&reading(82, 1, 1800)), None); // hystUp 5: not before 85
        assert_eq!(c.step(&reading(85, 1, 1800)), Some(4));
        assert_eq!(c.step(&reading(56, 4, 2800)), None); // not yet 5 C below the 60 C level
        assert_eq!(c.step(&reading(55, 4, 2800)), Some(1)); // first level at or below its temp - hystDown
    }

    #[test]
    fn smart_starts_from_the_bottom_after_another_mode() {
        let mut c = Controller::new(Config { active: 2, ..Config::default() });
        c.previous_mode = 3;
        assert_eq!(c.step(&reading(55, 7, 3400)), Some(0));
        assert_eq!(c.traces.last().unwrap(), "Change Mode from Manual->Smart, recalculate fan speed");
    }

    #[test]
    fn manual_reverts_to_smart_when_hot_and_bios_hands_back() {
        let mut c = Controller::new(Config { active: 3, man_fan_speed: 2, ..Config::default() });
        c.previous_mode = 3;
        let mut r = reading(50, 0, 0);
        assert_eq!(c.step(&r), Some(2));
        r.temps[0] = Some(79);
        c.step(&r);
        assert_eq!(c.mode, 2);
        assert!(c.reverted_from_manual);
        let mut b = Controller::new(Config { active: 1, ..Config::default() });
        assert_eq!(b.step(&reading(50, 4, 2800)), Some(BIOS));
    }

    #[test]
    fn proc_files() {
        let fan = "status:\t\tenabled\nspeed:\t\t2790\nlevel:\t\t4\ncommands:\tlevel <level> (<level> is 0-7, auto, disengaged, full-speed)\n";
        assert_eq!(parse_proc_fan(fan), Some((4, 2790)));
        assert_eq!(parse_proc_fan("speed: 3445\nlevel: disengaged\n"), Some((FULL, 3445)));
        assert_eq!(parse_proc_fan("speed: 1800\nlevel: auto\n"), Some((BIOS, 1800)));
        assert_eq!(proc_fan_command(FULL), "level disengaged");
        assert_eq!(proc_fan_command(BIOS), "level auto");
        let th = parse_proc_thermal("temperatures:\t46 0 45 -128 45 45 45 -128\n");
        assert_eq!(th[0], Some(46));
        assert_eq!(th[1], None);
        assert_eq!(th[3], None);
    }
}
