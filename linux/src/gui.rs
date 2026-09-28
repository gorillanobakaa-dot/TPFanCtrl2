//! The window: the same layout as the Windows version (temperatures, the fan
//! and its modes, the log, a status line) and the same custom title bar, where
//! minimise lights up bright blue. It needs no rights: it only asks the
//! service (proto.rs) for the state and hands it the user's choices.

use crate::core::{self, BIOS, FULL, LEVEL_ORDER, SENSOR_NAMES};
use crate::proto::{self, Set, State};
use eframe::egui::{self, Color32, RichText, Sense, Vec2};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

const HOVER_BLUE: Color32 = Color32::from_rgb(0x00, 0xB4, 0xFF); // CaptionHoverColor on Windows
const HOVER_RED: Color32 = Color32::from_rgb(0xE8, 0x11, 0x23);
const TITLE_H: f32 = 30.0;

type Latest = Arc<Mutex<Result<State, String>>>;

pub fn run() -> i32 {
    let options = eframe::NativeOptions {
        viewport: egui::ViewportBuilder::default()
            .with_title("Gorilla TPFanControl")
            .with_app_id("gorilla-fan")
            .with_inner_size([1180.0, 560.0])
            .with_min_inner_size([1000.0, 520.0])
            .with_decorations(false),
        renderer: eframe::Renderer::Glow,
        ..Default::default()
    };
    let r = eframe::run_native(
        "Gorilla TPFanControl",
        options,
        Box::new(|cc| {
            cc.egui_ctx.set_visuals(egui::Visuals::light());
            cc.egui_ctx.style_mut(|s| {
                s.spacing.item_spacing = Vec2::new(8.0, 6.0);
                s.visuals.panel_fill = Color32::from_rgb(0xf0, 0xf0, 0xf0);
            });
            Ok(Box::new(Gui::new(cc.egui_ctx.clone())))
        }),
    );
    match r {
        Ok(()) => 0,
        Err(e) => {
            eprintln!("Could not open the window: {}", e);
            1
        }
    }
}

struct Gui {
    latest: Latest,
    // what the controls show; follows the service unless the user just changed it
    mode: i32,
    level: i32,
    target: i32,
    floor: i32,
    touched: Option<Instant>,
    note: String,
}

impl Gui {
    fn new(ctx: egui::Context) -> Gui {
        let latest: Latest = Arc::new(Mutex::new(Err("Connecting to the fan service...".into())));
        let l = latest.clone();
        std::thread::spawn(move || loop {
            let r = proto::request("STATE").and_then(|t| State::decode(&t).ok_or_else(|| "the service gave an unreadable answer".to_string()));
            *l.lock().unwrap() = r;
            ctx.request_repaint();
            std::thread::sleep(Duration::from_millis(1000));
        });
        Gui { latest, mode: 0, level: 4, target: 45, floor: 5, touched: None, note: String::new() }
    }

    fn send(&mut self, set: Set) {
        self.touched = Some(Instant::now());
        match proto::request(&set.encode()) {
            Ok(a) if a.trim() == "ok" => self.note.clear(),
            Ok(a) => self.note = a.trim().trim_start_matches("error ").to_string(),
            Err(e) => self.note = format!("The fan service did not answer: {}", e),
        }
    }

    fn title_bar(&mut self, ctx: &egui::Context, title: &str) {
        egui::TopBottomPanel::top("title").exact_height(TITLE_H).frame(egui::Frame::new().fill(Color32::WHITE)).show(ctx, |ui| {
            let full = ui.max_rect();
            // drag anywhere on the bar moves the window, as a normal title bar
            let bar = ui.interact(full, ui.id().with("drag"), Sense::click_and_drag());
            if bar.drag_started() {
                ctx.send_viewport_cmd(egui::ViewportCommand::StartDrag);
            }
            ui.painter().text(full.left_center() + Vec2::new(12.0, 0.0), egui::Align2::LEFT_CENTER, title, egui::FontId::proportional(14.0), Color32::from_gray(20));
            let w = 46.0;
            let close = egui::Rect::from_min_size(egui::pos2(full.right() - w, full.top()), Vec2::new(w, TITLE_H));
            let min = close.translate(Vec2::new(-w, 0.0));
            if caption_button(ui, min, "min", HOVER_BLUE) {
                ctx.send_viewport_cmd(egui::ViewportCommand::Minimized(true));
            }
            if caption_button(ui, close, "close", HOVER_RED) {
                ctx.send_viewport_cmd(egui::ViewportCommand::Close);
            }
        });
    }
}

/// A title-bar button: coloured fill and a white glyph on hover.
fn caption_button(ui: &mut egui::Ui, rect: egui::Rect, kind: &str, hover: Color32) -> bool {
    let r = ui.interact(rect, ui.id().with(kind), Sense::click());
    let hot = r.hovered();
    if hot {
        ui.painter().rect_filled(rect, 0.0, hover);
    }
    let ink = if hot { Color32::WHITE } else { Color32::from_gray(40) };
    let c = rect.center();
    let stroke = egui::Stroke::new(1.2_f32, ink);
    if kind == "min" {
        ui.painter().line_segment([c + Vec2::new(-5.0, 0.0), c + Vec2::new(5.0, 0.0)], stroke);
    } else {
        ui.painter().line_segment([c + Vec2::new(-5.0, -5.0), c + Vec2::new(5.0, 5.0)], stroke);
        ui.painter().line_segment([c + Vec2::new(-5.0, 5.0), c + Vec2::new(5.0, -5.0)], stroke);
    }
    r.on_hover_text(if kind == "min" { "Minimise" } else { "Close the window (the fan service keeps running)" }).clicked()
}

/// A titled box, like a Windows group box.
fn group<R>(ui: &mut egui::Ui, title: &str, width: f32, add: impl FnOnce(&mut egui::Ui) -> R) -> R {
    ui.vertical(|ui| {
        ui.set_width(width);
        ui.label(RichText::new(title).strong());
        egui::Frame::new()
            .fill(Color32::WHITE)
            .stroke(egui::Stroke::new(1.0_f32, Color32::from_gray(190)))
            .inner_margin(egui::Margin::same(8))
            .show(ui, |ui| {
                ui.set_width(width - 18.0);
                ui.set_min_height(400.0);
                add(ui)
            })
            .inner
    })
    .inner
}

fn level_combo(ui: &mut egui::Ui, id: &str, value: &mut i32, rpm: &[i32; 9], levels: &[i32], width: f32) -> bool {
    let before = *value;
    egui::ComboBox::from_id_salt(id)
        .width(width)
        .selected_text(RichText::new(core::level_label(rpm, *value)).monospace())
        .show_ui(ui, |ui| {
            for &l in levels {
                ui.selectable_value(value, l, RichText::new(core::level_label(rpm, l)).monospace());
            }
        });
    *value != before
}

impl eframe::App for Gui {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        let latest = self.latest.lock().unwrap().clone();
        let fresh = self.touched.is_none_or(|t| t.elapsed() > Duration::from_secs(3));
        if let (Ok(st), true) = (&latest, fresh) {
            self.mode = st.mode;
            self.level = st.manual_level;
            self.target = st.gorilla_target;
            self.floor = st.gorilla_floor;
        }
        let title = match &latest {
            Ok(st) if st.max_temp > 0 => format!("Gorilla TPFanControl  {} for Linux    {} C", st.version, st.max_temp),
            _ => format!("Gorilla TPFanControl  {} for Linux", env!("CARGO_PKG_VERSION")),
        };
        self.title_bar(ctx, &title);

        egui::TopBottomPanel::bottom("status").show(ctx, |ui| {
            ui.add_space(4.0);
            let text = match &latest {
                Err(e) => format!("The fan service is not reachable ({}). Start it with: sudo systemctl start gorilla-fan", e),
                Ok(st) if !st.problem.is_empty() => st.problem.clone(),
                Ok(st) => {
                    let t: Vec<String> = st.temps.iter().map(|t| t.map_or("-".into(), |v| v.to_string())).collect();
                    let sim = if st.backend == "simulated" { "   [SIMULATED ThinkPad - no real fan]" } else { "" };
                    format!("Fan: {} / Switch: {} C ({}){}", core::level_value_text(st.level), st.max_temp, t.join("; "), sim)
                }
            };
            ui.label(RichText::new(text).monospace());
            if !self.note.is_empty() {
                ui.colored_label(HOVER_RED, &self.note);
            }
            ui.add_space(2.0);
        });

        egui::CentralPanel::default().show(ctx, |ui| {
            let st = latest.clone().unwrap_or_default();
            let can = latest.is_ok() && st.problem.is_empty() && st.calibrating.is_empty();
            ui.horizontal_top(|ui| {
                group(ui, "Temperatures", 190.0, |ui| {
                    egui::Grid::new("temps").striped(true).num_columns(3).spacing([14.0, 4.0]).show(ui, |ui| {
                        ui.label(RichText::new("#").strong());
                        ui.label(RichText::new("Name").strong());
                        ui.label(RichText::new("Temp").strong());
                        ui.end_row();
                        for (i, t) in st.temps.iter().enumerate() {
                            if let Some(t) = t {
                                ui.label((i + 1).to_string());
                                ui.label(SENSOR_NAMES[i]);
                                ui.label(format!("{} C", t));
                                ui.end_row();
                            }
                        }
                    });
                });

                group(ui, "TPFanControl", 520.0, |ui| {
                    egui::Grid::new("state").num_columns(2).spacing([10.0, 6.0]).show(ui, |ui| {
                        ui.label("State");
                        let connected = latest.is_ok() && st.problem.is_empty();
                        let what = match st.level {
                            BIOS => "BIOS".to_string(),
                            FULL => "Full speed, Non Bios".to_string(),
                            l => format!("Fan Level {}, Non Bios", l),
                        };
                        let state = if connected { format!("{} ({})", core::level_value_text(st.level), what) } else { "unknown".to_string() };
                        ui.label(RichText::new(state).monospace());
                        ui.end_row();
                        ui.label("Speed");
                        let speed = if connected { format!("{} RPM      Switch {} C", st.rpm, st.max_temp) } else { "unknown".to_string() };
                        ui.label(RichText::new(speed).monospace());
                        ui.end_row();
                    });
                    ui.separator();
                    ui.add_enabled_ui(can, |ui| {
                        ui.label("Mode");
                        if ui.radio(self.mode == 1, "BIOS (embedded controller)").clicked() && self.mode != 1 {
                            self.mode = 1;
                            self.send(Set { mode: Some(1), ..Set::default() });
                        }
                        if ui.radio(self.mode == 2, "Smart (the curve in /etc/gorilla-fan.conf)").clicked() && self.mode != 2 {
                            self.mode = 2;
                            self.send(Set { mode: Some(2), ..Set::default() });
                        }
                        ui.horizontal(|ui| {
                            if ui.radio(self.mode == 3, "Manual").clicked() && self.mode != 3 {
                                self.mode = 3;
                                self.send(Set { mode: Some(3), level: Some(self.level), ..Set::default() });
                            }
                            if level_combo(ui, "manual", &mut self.level, &st.level_rpm, &LEVEL_ORDER, 300.0) {
                                // picking a level means Manual, as on Windows
                                self.mode = 3;
                                self.send(Set { mode: Some(3), level: Some(self.level), ..Set::default() });
                            }
                        });
                        if ui.radio(self.mode == 4, "Gorilla (cooling first)").clicked() && self.mode != 4 {
                            self.mode = 4;
                            self.send(Set { mode: Some(4), target: Some(self.target), floor: Some(self.floor), ..Set::default() });
                        }
                        ui.horizontal(|ui| {
                            ui.add_space(24.0);
                            ui.label("keep CPU at");
                            let before = self.target;
                            egui::ComboBox::from_id_salt("target").width(70.0).selected_text(format!("{} C", self.target)).show_ui(ui, |ui| {
                                for c in [40, 45, 50] {
                                    ui.selectable_value(&mut self.target, c, format!("{} C", c));
                                }
                            });
                            ui.label("never slower than");
                            let floors: Vec<i32> = LEVEL_ORDER.iter().copied().filter(|&l| l != 0).collect();
                            let fchanged = level_combo(ui, "floor", &mut self.floor, &st.level_rpm, &floors, 230.0);
                            if fchanged || self.target != before {
                                self.mode = 4;
                                self.send(Set { mode: Some(4), target: Some(self.target), floor: Some(self.floor), ..Set::default() });
                            }
                        });
                    });
                    ui.add_space(4.0);
                    ui.label("Gorilla: full speed at the CPU target, level 7 within 5 C of it, never below the minimum. Noise is not a factor.");
                    ui.add_space(6.0);
                    ui.horizontal(|ui| {
                        if !st.calibrating.is_empty() {
                            ui.spinner();
                            ui.label(format!("Measuring the fan speeds, {}", st.calibrating));
                        } else if ui.add_enabled(can, egui::Button::new("Measure fan speeds (about 3 minutes)")).on_hover_text(
                            "Runs each level for 20 seconds and saves the rpm, so the lists show real percentages. Stops by itself if the laptop gets hot.",
                        ).clicked() {
                            match proto::request("CALIBRATE") {
                                Ok(a) if a.trim() == "ok" => self.note.clear(),
                                Ok(a) => self.note = a.trim().trim_start_matches("error ").to_string(),
                                Err(e) => self.note = e,
                            }
                        }
                    });
                });

                group(ui, "Log (also: journalctl -u gorilla-fan)", ui.available_width(), |ui| {
                    egui::ScrollArea::both().stick_to_bottom(true).auto_shrink([false, false]).max_height(400.0).show(ui, |ui| {
                        for l in &st.log {
                            ui.label(RichText::new(l).monospace().size(11.0));
                        }
                    });
                });
            });
        });
    }
}
