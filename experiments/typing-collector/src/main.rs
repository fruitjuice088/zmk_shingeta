use std::fs::{self, OpenOptions};
use std::io::{self, Write};
use std::path::{Path, PathBuf};

use rdev::{listen, Event, EventType};
use serde::Serialize;
use time::format_description::well_known::Rfc3339;
use time::macros::format_description;
use time::OffsetDateTime;

#[derive(Debug, Serialize)]
#[serde(rename_all = "snake_case")]
enum EventKind {
    Printable,
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

fn daily_log_path(now: &OffsetDateTime) -> PathBuf {
    let day = now
        .format(&format_description!("[year]-[month]-[day]"))
        .expect("failed to format date");
    PathBuf::from("data").join(format!("events-{day}.jsonl"))
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

    let resolved_text = normalize_printable_name(event.name)?;

    Some(CapturedEvent {
        ts: now
            .format(&Rfc3339)
            .expect("failed to format timestamp"),
        host: host_name(),
        os: std::env::consts::OS.to_string(),
        app: current_app(),
        raw_key: format!("{key:?}"),
        mods: Vec::new(),
        resolved_text: Some(resolved_text),
        kind: EventKind::Printable,
    })
}

fn callback(event: Event) {
    let now = now_local();

    let Some(captured) = captured_event_from_rdev(event, &now) else {
        return;
    };

    let path = daily_log_path(&now);

    if let Err(error) = append_event(&path, &captured) {
        eprintln!("append error: {error}");
        return;
    }

    println!(
        "logged raw_key={} resolved_text={:?}",
        captured.raw_key, captured.resolved_text
    );
}

fn main() {
    println!("starting printable-only listener");
    println!("press Ctrl-C to stop");

    if let Err(error) = listen(callback) {
        eprintln!("listen error: {error:?}");
    }
}
