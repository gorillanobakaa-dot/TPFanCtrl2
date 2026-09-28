//! Gorilla TPFanControl for Linux.
//!
//!   gorilla-fan                      the window (no rights needed)
//!   gorilla-fan daemon [options]     the fan service (root; systemd runs it)
//!       --config FILE    settings file (default /etc/gorilla-fan.conf)
//!       --simulate       a pretend ThinkPad, for testing without one
//!       --proc DIR       where acpi/ibm/fan is (default /proc; for tests)
//!   gorilla-fan release              hand the fan back to the BIOS
//!   gorilla-fan --version

mod core;
mod daemon;
mod fan;
mod gui;
mod proto;

fn usage() -> i32 {
    eprintln!("Usage: gorilla-fan [daemon [--config FILE] [--simulate] [--proc DIR] | release | --version]");
    2
}

fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let code = match args.first().map(|s| s.as_str()) {
        None => gui::run(),
        Some("--version") | Some("-V") => {
            println!("gorilla-fan {}", env!("CARGO_PKG_VERSION"));
            0
        }
        Some("release") => daemon::release(std::path::Path::new("/proc")),
        Some("daemon") => {
            let mut opt = daemon::Options::default();
            let mut it = args[1..].iter();
            let mut ok = true;
            while let Some(a) = it.next() {
                match a.as_str() {
                    "--simulate" => opt.simulate = true,
                    "--config" => match it.next() {
                        Some(v) => opt.config = v.into(),
                        None => ok = false,
                    },
                    "--proc" => match it.next() {
                        Some(v) => opt.proc_root = v.into(),
                        None => ok = false,
                    },
                    _ => ok = false,
                }
            }
            if ok { daemon::run(opt) } else { usage() }
        }
        Some(_) => usage(),
    };
    std::process::exit(code);
}
