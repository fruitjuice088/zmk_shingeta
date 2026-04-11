use std::fs::{self, OpenOptions};
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::sync::{LazyLock, Mutex};

use rdev::{listen, Event, EventType, Key};
use serde::Serialize;
use time::format_description::well_known::Rfc3339;
use time::macros::format_description;
use time::OffsetDateTime;

#[derive(Debug, Serialize)]
#[serde(rename_all = "snake_case")]
enum EventKind {
    Printable,
    Control,
}

#[derive(Debug, Serialize)]
struct CapturedEvent {
    ts: String,
    host: String,
    os: String,
    app: String,
    raw_key: String,
    mods: Vec<String>,
    resolved_text: Option<String>,
    kind: EventKind,
}

#[derive(Debug, Clone, Copy)]
enum ModifierKey {
    Shift,
    Ctrl,
    Alt,
    Meta,
}

#[derive(Debug, Default)]
struct ModifierState {
    shift: bool,
    ctrl: bool,
    alt: bool,
    meta: bool,
}

#[derive(Debug, Clone)]
struct CliOptions {
    verbose: bool,
    data_dir: PathBuf,
}

impl ModifierState {
    fn to_vec(&self) -> Vec<String> {
        let mut mods = Vec::new();

        if self.shift {
            mods.push("shift".to_string());
        }
        if self.ctrl {
            mods.push("ctrl".to_string());
        }
        if self.alt {
            mods.push("alt".to_string());
        }
        if self.meta {
            mods.push("meta".to_string());
        }

        mods
    }
}

static MODIFIER_STATE: LazyLock<Mutex<ModifierState>> =
    LazyLock::new(|| Mutex::new(ModifierState::default()));

fn now_local() -> OffsetDateTime {
    OffsetDateTime::now_local().unwrap_or_else(|_| OffsetDateTime::now_utc())
}

fn host_name() -> String {
    hostname::get()
        .ok()
        .and_then(|name| name.into_string().ok())
        .filter(|name| !name.is_empty())
        .unwrap_or_else(|| "unknown".to_string())
}

fn print_usage() {
    println!("usage: typing-collector [--verbose] [--data-dir PATH");
}

fn parse_cli() -> CliOptions {
    let mut verbose = false;
    let mut data_dir = PathBuf::from("data");

    let mut args = std::env::args().skip(1);

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--verbose" => verbose = true,
            "--data-dir" => {
                let Some(path) = args.next() else {
                    eprintln!("missing value for --data-dir");
                    print_usage();
                    std::process::exit(2);
                };
                data_dir = PathBuf::from(path);
            }
            "-h" | "--help" => {
                print_usage();
                std::process::exit(0);
            }
            _ => {
                eprintln!("unknown argument: {arg}");
                print_usage();
                std::process::exit(2);
            }
        }
    }

    CliOptions { verbose, data_dir }
}

fn modifier_key_for_event_type(event_type: &EventType) -> Option<ModifierKey> {
    match event_type {
        EventType::KeyPress(Key::ShiftLeft) | EventType::KeyRelease(Key::ShiftLeft) => {
            Some(ModifierKey::Shift)
        }
        EventType::KeyPress(Key::ShiftRight) | EventType::KeyRelease(Key::ShiftRight) => {
            Some(ModifierKey::Shift)
        }
        EventType::KeyPress(Key::ControlLeft) | EventType::KeyRelease(Key::ControlLeft) => {
            Some(ModifierKey::Ctrl)
        }
        EventType::KeyPress(Key::Unknown(62)) | EventType::KeyRelease(Key::Unknown(62)) => {
            Some(ModifierKey::Ctrl)
        }
        EventType::KeyPress(Key::Alt) | EventType::KeyRelease(Key::Alt) => {
            Some(ModifierKey::Alt)
        }
        EventType::KeyPress(Key::AltGr) | EventType::KeyRelease(Key::AltGr) => {
            Some(ModifierKey::Alt)
        }
        EventType::KeyPress(Key::MetaLeft) | EventType::KeyRelease(Key::MetaLeft) => {
            Some(ModifierKey::Meta)
        }
        EventType::KeyPress(Key::MetaRight) | EventType::KeyRelease(Key::MetaRight) => {
            Some(ModifierKey::Meta)
        }
        _ => None,
    }
}

fn is_modifier_key(key: Key) -> bool {
    matches!(
        key,
        Key::ShiftLeft
            | Key::ShiftRight
            | Key::ControlLeft
            | Key::Unknown(62)
            | Key::Alt
            | Key::AltGr
            | Key::MetaLeft
            | Key::MetaRight
    )
}

fn update_modifier_state(event: &Event) {
    let Some(modifier) = modifier_key_for_event_type(&event.event_type) else {
        return;
    };

    let pressed = matches!(event.event_type, EventType::KeyPress(_));

    let mut state = MODIFIER_STATE
        .lock()
        .expect("modifier state mutex should not be poisoned");

    match modifier {
        ModifierKey::Shift => state.shift = pressed,
        ModifierKey::Ctrl => state.ctrl = pressed,
        ModifierKey::Alt => state.alt = pressed,
        ModifierKey::Meta => state.meta = pressed,
    }
}

fn current_mods() -> Vec<String> {
    let state = MODIFIER_STATE
        .lock()
        .expect("modifier state mutex should not be poisoned");
    state.to_vec()
}

fn has_shortcut_modifiers(mods: &[String]) -> bool {
    mods.iter().any(|modifier| modifier == "ctrl" || modifier == "meta")
}

#[cfg(target_os = "macos")]
fn current_app() -> String {
    use objc2_app_kit::NSWorkspace;

    let workspace = NSWorkspace::sharedWorkspace();
    let Some(app) = workspace.frontmostApplication() else {
        return "unknown".to_string();
    };

    if let Some(bundle_id) = app.bundleIdentifier() {
        bundle_id.to_string()
    } else if let Some(name) = app.localizedName() {
        name.to_string()
    } else {
        "unknown".to_string()
    }
}

#[cfg(not(target_os = "macos"))]
fn current_app() -> String {
    "unknown".to_string()
}

fn daily_log_path(now: &OffsetDateTime, data_dir: &Path) -> PathBuf {
    let day = now
        .format(&format_description!("[year]-[month]-[day]"))
        .expect("failed to format date");
    data_dir.join(format!("events-{day}.jsonl"))
}

fn append_event(path: &Path, event: &CapturedEvent) -> io::Result<()> {
    let parent = path
        .parent()
        .expect("log path should have parent directory");

    fs::create_dir_all(parent)?;

    let mut file = OpenOptions::new().create(true).append(true).open(path)?;
    let line = serde_json::to_string(&event).expect("failed to serialize event");
    writeln!(file, "{line}")?;

    Ok(())
}

fn normalize_printable_name(name: Option<String>) -> Option<String> {
    let name = name?;

    if name.is_empty() {
        return None;
    }

    if name.chars().any(char::is_control) {
        return None;
    }

    Some(name)
}

fn captured_event_from_rdev(event: Event, now: &OffsetDateTime) -> Option<CapturedEvent> {
    let key = match event.event_type {
        EventType::KeyPress(key) => key,
        _ => return None,
    };

    if is_modifier_key(key) {
        return None;
    }

    let mods = current_mods();

    let kind = if has_shortcut_modifiers(&mods) {
        EventKind::Control
    } else {
        EventKind::Printable
    };

    let resolved_text = match kind {
        EventKind::Printable => Some(normalize_printable_name(event.name)?),
        EventKind::Control => None,
    };

    Some(CapturedEvent {
        ts: now
            .format(&Rfc3339)
            .expect("failed to format timestamp"),
        host: host_name(),
        os: std::env::consts::OS.to_string(),
        app: current_app(),
        raw_key: format!("{key:?}"),
        mods,
        resolved_text,
        kind,
    })
}

fn callback(event: Event, options: &CliOptions) {
    update_modifier_state(&event);

    let now = now_local();

    let Some(captured) = captured_event_from_rdev(event, &now) else {
        return;
    };

    let path = daily_log_path(&now, &options.data_dir);

    if let Err(error) = append_event(&path, &captured) {
        eprintln!("append error: {error}");
        return;
    }

    if options.verbose {
        println!(
            "logged kind={:?} raw_key={} mods={:?} resolved_text={:?}",
            captured.kind, captured.raw_key, captured.mods, captured.resolved_text
        );
    }
}

fn main() {
    let options = parse_cli();

    println!("starting listener");
    println!("logging to {}", options.data_dir.display());
    println!("press Ctrl-C to stop");

    if let Err(error) = listen(move |event| callback(event, &options)) {
        eprintln!("listen error: {error:?}");
    }
}
