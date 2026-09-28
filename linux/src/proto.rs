//! How the window talks to the service: one line of text over a TCP
//! connection to 127.0.0.1 (never reachable from the network), one answer,
//! then the connection closes.
//!
//!   STATE                               -> key=value lines, then "end"
//!   SET mode=4 level=5 target=45 floor=5 -> "ok" or "error <why>"   (every field optional)
//!   CALIBRATE                           -> "ok" or "error <why>"
//!
//! Any local user may steer the fan, as with the Windows version (whose
//! window talks to its service through shared memory open to all users).
//! Every value is checked; the worst a local user can do is pick a mode or
//! level, and the ManModeExit and Gorilla overrides still apply.

use std::io::{BufRead, BufReader, Read, Write};
use std::net::TcpStream;
use std::time::Duration;

pub const ADDR: &str = "127.0.0.1:47811";

#[derive(Clone, Debug, Default, PartialEq)]
pub struct State {
    pub version: String,
    pub backend: String,
    pub problem: String,
    pub mode: i32,
    pub level: i32,
    pub rpm: i32,
    pub temps: [Option<i32>; 12],
    pub max_temp: i32,
    pub manual_level: i32,
    pub gorilla_target: i32,
    pub gorilla_floor: i32,
    pub man_mode_exit: i32,
    pub level_rpm: [i32; 9],
    pub calibrating: String,
    pub log: Vec<String>,
}

/// A value with no line breaks, so one field stays one line.
fn clean(s: &str) -> String {
    s.replace(['\r', '\n'], " ")
}

impl State {
    pub fn encode(&self) -> String {
        let temps: Vec<String> = self.temps.iter().map(|t| t.map_or("-".to_string(), |v| v.to_string())).collect();
        let rpm: Vec<String> = self.level_rpm.iter().map(|r| r.to_string()).collect();
        let mut s = format!(
            "version={}\nbackend={}\nproblem={}\nmode={}\nlevel={}\nrpm={}\ntemps={}\nmax_temp={}\nmanual_level={}\ngorilla_target={}\ngorilla_floor={}\nman_mode_exit={}\nlevel_rpm={}\ncalibrating={}\n",
            clean(&self.version), clean(&self.backend), clean(&self.problem), self.mode, self.level, self.rpm, temps.join(","),
            self.max_temp, self.manual_level, self.gorilla_target, self.gorilla_floor, self.man_mode_exit, rpm.join(" "), clean(&self.calibrating)
        );
        for l in &self.log {
            s.push_str("log=");
            s.push_str(&clean(l));
            s.push('\n');
        }
        s.push_str("end\n");
        s
    }

    pub fn decode(text: &str) -> Option<State> {
        let mut st = State::default();
        let mut ended = false;
        for line in text.lines() {
            if line == "end" {
                ended = true;
                break;
            }
            let (k, v) = line.split_once('=')?;
            let n = || v.trim().parse::<i32>().unwrap_or(0);
            match k {
                "version" => st.version = v.into(),
                "backend" => st.backend = v.into(),
                "problem" => st.problem = v.into(),
                "mode" => st.mode = n(),
                "level" => st.level = n(),
                "rpm" => st.rpm = n(),
                "temps" => {
                    for (i, t) in v.split(',').take(12).enumerate() {
                        st.temps[i] = t.parse().ok();
                    }
                }
                "max_temp" => st.max_temp = n(),
                "manual_level" => st.manual_level = n(),
                "gorilla_target" => st.gorilla_target = n(),
                "gorilla_floor" => st.gorilla_floor = n(),
                "man_mode_exit" => st.man_mode_exit = n(),
                "level_rpm" => {
                    for (i, r) in v.split_whitespace().take(9).enumerate() {
                        st.level_rpm[i] = r.parse().unwrap_or(0);
                    }
                }
                "calibrating" => st.calibrating = v.into(),
                "log" => st.log.push(v.into()),
                _ => {}
            }
        }
        if ended { Some(st) } else { None }
    }
}

/// A SET request: only the fields present change.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Set {
    pub mode: Option<i32>,
    pub level: Option<i32>,
    pub target: Option<i32>,
    pub floor: Option<i32>,
}

impl Set {
    pub fn encode(&self) -> String {
        let mut s = String::from("SET");
        for (k, v) in [("mode", self.mode), ("level", self.level), ("target", self.target), ("floor", self.floor)] {
            if let Some(v) = v {
                s.push_str(&format!(" {}={}", k, v));
            }
        }
        s
    }

    /// Parse and check "SET k=v ...". Err names the first bad field.
    pub fn decode(line: &str) -> Result<Set, String> {
        let rest = line.strip_prefix("SET").ok_or("not a SET")?;
        let mut s = Set::default();
        for f in rest.split_whitespace() {
            let (k, v) = f.split_once('=').ok_or_else(|| format!("bad field {}", f))?;
            let v: i32 = crate::core::parse_level_token(v).ok_or_else(|| format!("bad number in {}", f))?;
            match k {
                "mode" if (1..=4).contains(&v) => s.mode = Some(v),
                "level" if crate::core::valid_level(v) && v != crate::core::BIOS => s.level = Some(v),
                "target" if (30..=70).contains(&v) => s.target = Some(v),
                "floor" if crate::core::valid_floor(v) => s.floor = Some(v),
                _ => return Err(format!("not allowed: {}", f)),
            }
        }
        Ok(s)
    }
}

/// Send one request, return the whole answer.
pub fn request(line: &str) -> Result<String, String> {
    let addr = ADDR.parse().map_err(|e| format!("{}", e))?;
    let mut s = TcpStream::connect_timeout(&addr, Duration::from_millis(600)).map_err(|e| e.to_string())?;
    s.set_read_timeout(Some(Duration::from_secs(3))).ok();
    s.set_write_timeout(Some(Duration::from_secs(3))).ok();
    s.write_all(line.as_bytes()).map_err(|e| e.to_string())?;
    s.write_all(b"\n").map_err(|e| e.to_string())?;
    let mut out = String::new();
    s.read_to_string(&mut out).map_err(|e| e.to_string())?;
    Ok(out)
}

/// Read the one request line of a connection (at most 256 bytes).
pub fn read_request(s: &TcpStream) -> Option<String> {
    s.set_read_timeout(Some(Duration::from_secs(2))).ok();
    let mut line = String::new();
    BufReader::new(s.take(256)).read_line(&mut line).ok()?;
    Some(line.trim().to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn state_round_trip() {
        let mut st = State { version: "0.1.0".into(), backend: "simulated".into(), mode: 4, level: 7, rpm: 3423, max_temp: 46, manual_level: 4, gorilla_target: 45, gorilla_floor: 5, man_mode_exit: 78, ..State::default() };
        st.temps[0] = Some(46);
        st.temps[10] = Some(66);
        st.level_rpm = [0, 1765, 2179, 2586, 2790, 2999, 3197, 3423, 3445];
        st.log = vec!["a line".into(), "a line\nwith a break".into()];
        let back = State::decode(&st.encode()).unwrap();
        assert_eq!(back.temps, st.temps);
        assert_eq!(back.level_rpm, st.level_rpm);
        assert_eq!(back.log[1], "a line with a break");
        assert_eq!((back.mode, back.level, back.gorilla_floor), (4, 7, 5));
        assert!(State::decode("mode=4\n").is_none()); // cut short
    }

    #[test]
    fn set_checks_every_value() {
        let s = Set { mode: Some(4), level: None, target: Some(45), floor: Some(64) };
        assert_eq!(Set::decode(&s.encode()), Ok(s));
        assert_eq!(Set::decode("SET level=0x40").unwrap().level, Some(64));
        assert!(Set::decode("SET mode=5").is_err());
        assert!(Set::decode("SET level=128").is_err()); // BIOS is a mode, not a manual level
        assert!(Set::decode("SET floor=0").is_err()); // a floor of 0 would let the fan stop
        assert!(Set::decode("SET target=90").is_err());
        assert!(Set::decode("SET rm=-rf").is_err());
    }
}
