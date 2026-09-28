//! `gorilla-fan daemon`: the root service. Reads the sensors every second,
//! decides every `Cycle` seconds (5 by default, as on Windows), writes the
//! fan, and answers the window on 127.0.0.1.
//!
//! Safety nets, from the inside out:
//!  - Gorilla and Manual run full speed above ManModeExit (78 C), as on Windows;
//!  - the kernel watchdog (thinkpad_acpi "watchdog 120") hands the fan to the
//!    BIOS if this service stops writing for two minutes (a hang);
//!  - systemd's ExecStopPost runs `gorilla-fan release`, which hands the fan
//!    to the BIOS whenever the service stops, however it stopped.

use crate::core::{self, Config, Controller, Reading, BIOS, FULL};
use crate::fan::{Fan, ProcFan, SimFan};
use crate::proto::{self, Set, State};
use std::collections::VecDeque;
use std::net::TcpListener;
use std::path::{Path, PathBuf};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

pub const DEFAULT_CONFIG: &str = "/etc/gorilla-fan.conf";
pub const SHIPPED_DEFAULT: &str = "/usr/share/gorilla-fan/gorilla-fan.conf.default";
const WATCHDOG_SECS: u32 = 120;
const REFRESH_EVERY: Duration = Duration::from_secs(30);
const CALIBRATE_SECS: u64 = 20;

pub struct Options {
    pub config: PathBuf,
    pub proc_root: PathBuf,
    pub sys_root: PathBuf,
    pub simulate: bool,
    pub listen: String,
}

impl Default for Options {
    fn default() -> Self {
        Options { config: DEFAULT_CONFIG.into(), proc_root: "/proc".into(), sys_root: "/sys".into(), simulate: false, listen: proto::ADDR.into() }
    }
}

struct Calibration {
    index: usize,
    since: Instant,
    samples: Vec<i32>,
    result: [i32; 9],
}

/// The levels measured, in the order of FanLevelRpm (0-7, full). Level 0
/// stops the fan: it is recorded as 0 rpm, not run.
const CALIBRATE_LEVELS: [i32; 8] = [1, 2, 3, 4, 5, 6, 7, FULL];

struct Shared {
    ctl: Controller,
    reading: Reading,
    backend: &'static str,
    problem: String,
    log: VecDeque<String>,
    calib: Option<Calibration>,
    config_path: PathBuf,
}

impl Shared {
    fn say(&mut self, s: &str) {
        let line = format!("[{}] {}", now_text(), s);
        println!("{}", s); // journald adds its own time
        if self.log.len() >= 200 {
            self.log.pop_front();
        }
        self.log.push_back(line);
    }

    fn drain_traces(&mut self) {
        let t: Vec<String> = self.ctl.traces.drain(..).collect();
        for s in t {
            self.say(&s);
        }
    }

    fn state(&self) -> State {
        let calibrating = match &self.calib {
            Some(c) => format!("measuring {} of {}: {}", c.index + 1, CALIBRATE_LEVELS.len(), core::level_value_text(CALIBRATE_LEVELS[c.index])),
            None => String::new(),
        };
        State {
            version: env!("CARGO_PKG_VERSION").into(),
            backend: self.backend.into(),
            problem: self.problem.clone(),
            mode: self.ctl.mode,
            level: self.reading.level,
            rpm: self.reading.rpm,
            temps: self.reading.temps,
            max_temp: self.ctl.max_temp(&self.reading),
            manual_level: self.ctl.manual_level,
            gorilla_target: self.ctl.cfg.gorilla_target,
            gorilla_floor: self.ctl.cfg.gorilla_floor,
            man_mode_exit: self.ctl.cfg.man_mode_exit,
            level_rpm: self.ctl.cfg.fan_level_rpm,
            calibrating,
            log: self.log.iter().rev().take(60).rev().cloned().collect(),
        }
    }

    fn persist(&mut self, pairs: &[(&str, String)]) {
        match save_keys(&self.config_path, pairs) {
            Ok(()) => {
                let what: Vec<String> = pairs.iter().map(|(k, v)| format!("{}={}", k, v)).collect();
                self.say(&format!("Saved to {}: {} (used again after a restart)", self.config_path.display(), what.join(" ")));
            }
            Err(e) => self.say(&format!("Could not save {}: {}", self.config_path.display(), e)),
        }
    }

    fn apply(&mut self, s: &Set) {
        if let Some(t) = s.target {
            self.ctl.cfg.gorilla_target = t;
        }
        if let Some(f) = s.floor {
            self.ctl.cfg.gorilla_floor = f;
        }
        if let Some(l) = s.level {
            self.ctl.manual_level = l;
        }
        if let Some(m) = s.mode {
            self.ctl.mode = m;
            self.ctl.reverted_from_manual = false;
        }
        let pairs = self.ctl.persist_pairs();
        self.persist(&pairs);
    }
}

/// Write keys into the settings file without touching anything else, via a
/// temporary file and a rename, so a power cut never leaves half a file.
pub fn save_keys(path: &Path, pairs: &[(&str, String)]) -> std::io::Result<()> {
    let old = std::fs::read_to_string(path).unwrap_or_default();
    let new = core::rewrite_keys(&old, pairs);
    let tmp = path.with_extension("conf.saving");
    std::fs::write(&tmp, new)?;
    std::fs::rename(&tmp, path)
}

/// The settings file, created from the shipped default the first time.
fn load_config(path: &Path) -> (Config, String) {
    if !path.exists() {
        let text = std::fs::read_to_string(SHIPPED_DEFAULT).unwrap_or_else(|_| include_str!("../packaging/gorilla-fan.conf.default").to_string());
        let note = match std::fs::write(path, &text) {
            Ok(()) => format!("Created {} from the defaults", path.display()),
            Err(e) => format!("Could not create {} ({}); running on the defaults", path.display(), e),
        };
        return (Config::parse(&text), note);
    }
    match std::fs::read_to_string(path) {
        Ok(t) => (Config::parse(&t), format!("Settings read from {}", path.display())),
        Err(e) => (Config::default(), format!("Could not read {} ({}); running on the defaults", path.display(), e)),
    }
}

pub fn run(opt: Options) -> i32 {
    let (cfg, note) = load_config(&opt.config);
    let mut fan: Box<dyn Fan> = if opt.simulate { Box::new(SimFan::new()) } else { Box::new(ProcFan { root: opt.proc_root.clone() }) };
    let problem = if opt.simulate { None } else { ProcFan { root: opt.proc_root.clone() }.problem(&opt.sys_root) };

    let shared = Arc::new(Mutex::new(Shared {
        ctl: Controller::new(cfg),
        reading: Reading { level: BIOS, ..Reading::default() },
        backend: if problem.is_some() { "none" } else { fan.name() },
        problem: problem.clone().unwrap_or_default(),
        log: VecDeque::new(),
        calib: None,
        config_path: opt.config.clone(),
    }));

    {
        let mut s = shared.lock().unwrap();
        s.say(&format!("Gorilla TPFanControl {} for Linux, fan backend: {}", env!("CARGO_PKG_VERSION"), fan.name()));
        s.say(&note);
        let c = s.ctl.cfg.clone();
        let msg = format!("Start mode: {}, Cycle={} s, ManModeExit={} C, GorillaTarget={} C, GorillaFloor={}, {} Smart levels",
            core::mode_name(s.ctl.mode), c.cycle, c.man_mode_exit, c.gorilla_target, core::level_value_text(c.gorilla_floor), c.levels.len()); s.say(&msg);
        if let Some(p) = &problem {
            s.say(p);
        }
    }

    let listener = match TcpListener::bind(&opt.listen) {
        Ok(l) => l,
        Err(e) => {
            eprintln!("Cannot listen on {}: {} (is another gorilla-fan daemon running?)", opt.listen, e);
            return 1;
        }
    };
    {
        let shared = shared.clone();
        std::thread::spawn(move || serve(listener, shared));
    }

    if problem.is_some() {
        // not a ThinkPad (or no fan_control): keep answering the window, never write
        loop {
            std::thread::sleep(Duration::from_secs(5));
            let mut s = shared.lock().unwrap();
            if let Ok(r) = fan.read() {
                s.reading = r;
            }
        }
    }

    if let Err(e) = fan.arm_watchdog(WATCHDOG_SECS) {
        shared.lock().unwrap().say(&format!("Kernel watchdog not armed: {}", e));
    } else {
        shared.lock().unwrap().say(&format!("Kernel watchdog armed: the BIOS takes the fan back if this service stops writing for {} s", WATCHDOG_SECS));
    }

    let mut last_decision = Instant::now() - Duration::from_secs(3600);
    let mut last_write = Instant::now();
    let mut last_error = String::new();
    loop {
        let reading = match fan.read() {
            Ok(r) => r,
            Err(e) => {
                let mut s = shared.lock().unwrap();
                if e != last_error {
                    s.say(&format!("Read failed: {}", e));
                    last_error = e;
                }
                drop(s);
                std::thread::sleep(Duration::from_secs(1));
                continue;
            }
        };

        let mut s = shared.lock().unwrap();
        s.reading = reading.clone();
        let cycle = Duration::from_secs(s.ctl.cfg.cycle);
        let mut want: Option<i32> = None;
        let mut who = "";

        if s.calib.is_some() {
            want = calibrate_step(&mut s, &reading);
            who = "Measure";
        } else if last_decision.elapsed() >= cycle || s.ctl.previous_mode != s.ctl.mode {
            last_decision = Instant::now();
            want = s.ctl.step(&reading);
            who = core::mode_name(if s.ctl.reverted_from_manual { 3 } else { s.ctl.previous_mode });
            s.drain_traces();
            if want.is_some() {
                let t: Vec<String> = reading.temps.iter().map(|t| t.map_or("-".into(), |v| v.to_string())).collect();
                let msg = format!("Fan: {} / {} rpm / Switch: {} C ({})", core::level_value_text(reading.level), reading.rpm, s.ctl.max_temp(&reading), t.join("; ")); s.say(&msg);
            }
        }

        // keep the kernel watchdog fed while this service holds the fan
        if want.is_none() && reading.level != BIOS && last_write.elapsed() >= REFRESH_EVERY {
            if fan.write(reading.level).is_ok() {
                last_write = Instant::now();
            }
        }
        if let Some(level) = want {
            match fan.write(level) {
                Ok(()) => {
                    last_write = Instant::now();
                    s.say(&format!("{}: Set fan control to {}, Result: OK", who, core::level_value_text(level)));
                }
                Err(e) => s.say(&format!("{}: Set fan control to {} FAILED: {}", who, core::level_value_text(level), e)),
            }
        }
        drop(s);
        std::thread::sleep(Duration::from_secs(1));
    }
}

/// One second of a fan speed measurement: CALIBRATE_SECS per level, the
/// average of the last five readings kept. Aborts to normal control if the
/// laptop gets hot.
fn calibrate_step(s: &mut Shared, r: &Reading) -> Option<i32> {
    let max = s.ctl.max_temp(r);
    let exit = s.ctl.cfg.man_mode_exit;
    let c = s.calib.as_mut().unwrap();
    if max > exit {
        s.calib = None;
        s.ctl.previous_mode = 1; // decide again from scratch, as after BIOS
        s.say(&format!("Measurement stopped: {} C is above ManModeExit ({} C). Nothing was saved.", max, exit));
        return None;
    }
    let level = CALIBRATE_LEVELS[c.index];
    if r.level != level {
        return Some(level);
    }
    let secs = c.since.elapsed().as_secs();
    if secs + 5 >= CALIBRATE_SECS {
        c.samples.push(r.rpm);
    }
    if secs < CALIBRATE_SECS {
        return None;
    }
    let avg = if c.samples.is_empty() { r.rpm } else { c.samples.iter().sum::<i32>() / c.samples.len() as i32 };
    let slot = if level == FULL { 8 } else { level as usize };
    c.result[slot] = avg;
    let msg = format!("Measured {}: {} rpm", core::level_value_text(level), avg);
    c.index += 1;
    c.since = Instant::now();
    c.samples.clear();
    let done = c.index >= CALIBRATE_LEVELS.len();
    let result = c.result;
    s.say(&msg);
    if done {
        s.calib = None;
        s.ctl.cfg.fan_level_rpm = result;
        s.ctl.previous_mode = 1;
        let v: Vec<String> = result.iter().map(|r| r.to_string()).collect();
        s.persist(&[("FanLevelRpm", v.join(" "))]);
        s.say("Measurement done: the level lists now show % and rpm");
        return None;
    }
    Some(CALIBRATE_LEVELS[s.calib.as_ref().unwrap().index])
}

fn serve(listener: TcpListener, shared: Arc<Mutex<Shared>>) {
    for conn in listener.incoming() {
        let mut conn = match conn {
            Ok(c) => c,
            Err(_) => continue,
        };
        // only this computer (the listener is on 127.0.0.1; checked again)
        if !conn.peer_addr().map(|a| a.ip().is_loopback()).unwrap_or(false) {
            continue;
        }
        let line = match proto::read_request(&conn) {
            Some(l) => l,
            None => continue,
        };
        let answer = {
            let mut s = shared.lock().unwrap();
            if line == "STATE" {
                s.state().encode()
            } else if line.starts_with("SET") {
                if !s.problem.is_empty() {
                    "error the fan cannot be controlled on this computer\n".to_string()
                } else if s.calib.is_some() {
                    "error a fan speed measurement is running\n".to_string()
                } else {
                    match Set::decode(&line) {
                        Ok(set) => {
                            s.apply(&set);
                            "ok\n".to_string()
                        }
                        Err(e) => format!("error {}\n", e),
                    }
                }
            } else if line == "CALIBRATE" {
                if !s.problem.is_empty() {
                    "error the fan cannot be controlled on this computer\n".to_string()
                } else if s.calib.is_some() {
                    "error already measuring\n".to_string()
                } else {
                    s.calib = Some(Calibration { index: 0, since: Instant::now(), samples: Vec::new(), result: [0; 9] });
                    let msg = format!("Measuring the fan: {} levels, {} s each, then back to {}", CALIBRATE_LEVELS.len(), CALIBRATE_SECS, core::mode_name(s.ctl.mode)); s.say(&msg);
                    "ok\n".to_string()
                }
            } else {
                "error unknown request\n".to_string()
            }
        };
        use std::io::Write;
        let _ = conn.write_all(answer.as_bytes());
    }
}

/// `gorilla-fan release`: hand the fan to the BIOS. systemd runs this after the
/// service stops, however it stopped.
pub fn release(proc_root: &Path) -> i32 {
    let mut f = ProcFan { root: proc_root.to_path_buf() };
    if !proc_root.join("acpi/ibm/fan").exists() {
        return 0;
    }
    match f.write(BIOS) {
        Ok(()) => {
            println!("Fan handed back to the BIOS (level auto)");
            0
        }
        Err(e) => {
            eprintln!("Could not hand the fan back to the BIOS: {}", e);
            1
        }
    }
}

// ---------------------------------------------------------------------------
//  local time for the log, without a date crate
// ---------------------------------------------------------------------------

#[cfg(unix)]
fn now_text() -> String {
    #[repr(C)]
    struct Tm {
        sec: i32,
        min: i32,
        hour: i32,
        mday: i32,
        mon: i32,
        year: i32,
        wday: i32,
        yday: i32,
        isdst: i32,
        gmtoff: i64,
        zone: *const i8,
    }
    extern "C" {
        fn time(t: *mut i64) -> i64;
        fn localtime_r(t: *const i64, tm: *mut Tm) -> *mut Tm;
    }
    unsafe {
        let t = time(std::ptr::null_mut());
        let mut tm: Tm = std::mem::zeroed();
        if localtime_r(&t, &mut tm).is_null() {
            return String::new();
        }
        format!("{:02}/{:02}/{} {:02}:{:02}:{:02}", tm.mday, tm.mon + 1, tm.year + 1900, tm.hour, tm.min, tm.sec)
    }
}

#[cfg(not(unix))]
fn now_text() -> String {
    let s = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
    format!("{:02}:{:02}:{:02} UTC", (s / 3600) % 24, (s / 60) % 60, s % 60)
}
