use std::fs::{self, OpenOptions};
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::sync::{LazyLock, Mutex, OnceLock};

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
    windows_hook_debug: bool,
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
    println!("usage: typing-collector [--verbose] [--data-dir PATH] [--windows-hook-debug]");
}

fn parse_cli() -> CliOptions {
    let mut verbose = false;
    let mut data_dir = PathBuf::from("data");
    let mut windows_hook_debug = false;

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
            "--windows-hook-debug" => windows_hook_debug = true,
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

    CliOptions { verbose, data_dir, windows_hook_debug }
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
        EventType::KeyPress(Key::ControlRight) | EventType::KeyRelease(Key::ControlRight) => {
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
            | Key::ControlRight
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

#[cfg(target_os = "windows")]
mod windows_hook_debug {
    use std::ptr::null;

    use windows_sys::Win32::Foundation::{HINSTANCE, LPARAM, LRESULT, WPARAM};
    use windows_sys::Win32::System::LibraryLoader::GetModuleHandleW;
    use windows_sys::Win32::UI::Input::KeyboardAndMouse::{
        GetKeyboardLayout, GetKeyboardState, ToUnicodeEx, VK_CONTROL, VK_LCONTROL, VK_LMENU,
        VK_LSHIFT, VK_LWIN, VK_MENU, VK_RCONTROL, VK_RMENU, VK_RSHIFT, VK_RWIN, VK_SHIFT,
    };
    use windows_sys::Win32::UI::WindowsAndMessaging::{
        CallNextHookEx, DispatchMessageW, GetMessageW, KBDLLHOOKSTRUCT, MSG, SetWindowsHookExW,
        TranslateMessage, UnhookWindowsHookEx, HC_ACTION, HHOOK, LLKHF_INJECTED, WH_KEYBOARD_LL,
        WM_KEYDOWN, WM_KEYUP, WM_SYSKEYDOWN, WM_SYSKEYUP,
    };

    fn modifier_key_for_vk(vk_code: u32) -> Option<super::ModifierKey> {
        match vk_code {
            x if x == VK_SHIFT as u32 || x == VK_LSHIFT as u32 || x == VK_RSHIFT as u32 => {
                Some(super::ModifierKey::Shift)
            }
            x if x == VK_CONTROL as u32
                || x == VK_LCONTROL as u32
                || x == VK_RCONTROL as u32 =>
            {
                Some(super::ModifierKey::Ctrl)
            }
            x if x == VK_MENU as u32 || x == VK_LMENU as u32 || x == VK_RMENU as u32 => {
                Some(super::ModifierKey::Alt)
            }
            x if x == VK_LWIN as u32 || x == VK_RWIN as u32 => Some(super::ModifierKey::Meta),
            _ => None,
        }
    }

    fn update_modifier_state_from_vk(vk_code: u32, pressed: bool) {
        let Some(modifier) = modifier_key_for_vk(vk_code) else {
            return;
        };

        let mut state = super::MODIFIER_STATE
            .lock()
            .expect("modifier state mutex should not be poisoned");

        match modifier {
            super::ModifierKey::Shift => state.shift = pressed,
            super::ModifierKey::Ctrl => state.ctrl = pressed,
            super::ModifierKey::Alt => state.alt = pressed,
            super::ModifierKey::Meta => state.meta = pressed,
        }
    }

    fn keyboard_state_for_translation() -> Option<[u8; 256]> {
        let mut keyboard_state = [0u8; 256];
        unsafe {
            if GetKeyboardState(keyboard_state.as_mut_ptr()) == 0 {
                return None;
            }
        }

        let state = super::MODIFIER_STATE
            .lock()
            .expect("modifier state mutex should not be poisoned");

        if state.shift {
            keyboard_state[VK_SHIFT as usize] |= 0x80;
        }
        if state.ctrl {
            keyboard_state[VK_CONTROL as usize] |= 0x80;
        }
        if state.alt {
            keyboard_state[VK_MENU as usize] |= 0x80;
        }

        Some(keyboard_state)
    }

    fn resolve_text(vk_code: u32, scan_code: u32) -> Option<String> {
        let keyboard_state = keyboard_state_for_translation()?;
        let layout = unsafe { GetKeyboardLayout(0) };

        let mut buffer = [0u16; 8];
        let written = unsafe {
            ToUnicodeEx(
                vk_code,
                scan_code,
                keyboard_state.as_ptr(),
                buffer.as_mut_ptr(),
                buffer.len() as i32,
                1 << 2,
                layout,
            )
        };

        if written <= 0 {
            return None;
        }

        let text = String::from_utf16_lossy(&buffer[..written as usize]);
        super::normalize_printable_name(Some(text))
    }

    pub fn run() {
        unsafe {
            let instance: HINSTANCE = GetModuleHandleW(null());
            let hook = SetWindowsHookExW(WH_KEYBOARD_LL, Some(hook_proc), instance, 0);

            if hook.is_null() {
                eprintln!("failed to install WH_KEYBOARD_LL hook");
                std::process::exit(1);
            }

            println!("windows hook debug started");
            println!("press Ctrl-C to stop");

            let mut message = MSG::default();
            while GetMessageW(&mut message, std::ptr::null_mut(), 0, 0) > 0 {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            UnhookWindowsHookEx(hook);
        }
    }

    unsafe extern "system" fn hook_proc(code: i32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
        if code != HC_ACTION as i32 {
            return CallNextHookEx(std::ptr::null_mut(), code, wparam, lparam);
        }

        let message = wparam as u32;
        let keydown = matches!(message, WM_KEYDOWN | WM_SYSKEYDOWN);
        let keyup = matches!(message, WM_KEYUP | WM_SYSKEYUP);

        if !keydown && !keyup {
            return CallNextHookEx(std::ptr::null_mut(), code, wparam, lparam);
        }

        let keyboard = *(lparam as *const KBDLLHOOKSTRUCT);
        let injected = (keyboard.flags & LLKHF_INJECTED) != 0;

        if injected {
            return CallNextHookEx(std::ptr::null_mut(), code, wparam, lparam);
        }

        update_modifier_state_from_vk(keyboard.vkCode, keydown);

        if keyup || modifier_key_for_vk(keyboard.vkCode).is_some() {
            return CallNextHookEx(std::ptr::null_mut(), code, wparam, lparam);
        }

        let mods = super::current_mods();

        if super::has_shortcut_modifiers(&mods) {
            println!(
                "native kind=control vk_code={} scan_code={} mods={:?} resolved_text=None",
                keyboard.vkCode, keyboard.scanCode, mods
            );

            return CallNextHookEx(std::ptr::null_mut(), code, wparam, lparam);
        }

        let Some(text) = resolve_text(keyboard.vkCode, keyboard.scanCode) else {
            return CallNextHookEx(std::ptr::null_mut(), code, wparam, lparam);
        };

        println!(
            "native kind=printable vk_code={} scan_code={} mods={:?} resolved_text={:?}",
            keyboard.vkCode, keyboard.scanCode, mods, text
        );

        CallNextHookEx(std::ptr::null_mut(), code, wparam, lparam)
    }
}

#[cfg(not(target_os = "windows"))]
mod windows_hook_debug {
    pub fn run() {
        eprintln!("windows hook debug is only supported on Windows");
        std::process::exit(2);
    }
}

fn main() {
    let options = parse_cli();
    if options.windows_hook_debug {
        windows_hook_debug::run();
        return;
    }

    println!("starting listener");
    println!("logging to {}", options.data_dir.display());
    println!("press Ctrl-C to stop");

    if let Err(error) = listen(move |event| callback(event, &options)) {
        eprintln!("listen error: {error:?}");
    }
}
