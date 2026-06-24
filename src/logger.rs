use std::collections::VecDeque;
use std::sync::Mutex;

use once_cell::sync::Lazy;

const MAX_LOG_LINES: usize = 128;
const MAX_DATA_LINES: usize = 32;

static LOG_LINES: Lazy<Mutex<VecDeque<String>>> =
    Lazy::new(|| Mutex::new(VecDeque::with_capacity(MAX_LOG_LINES)));

static DATA_LINES: Lazy<Mutex<VecDeque<String>>> =
    Lazy::new(|| Mutex::new(VecDeque::with_capacity(MAX_DATA_LINES)));

struct NimrsLogger;

impl log::Log for NimrsLogger {
    fn enabled(&self, _metadata: &log::Metadata) -> bool {
        true
    }

    fn log(&self, record: &log::Record) {
        let entry = format!("[{}] {}", millis(), record.args());
        let target = record.target();
        if target == "data" {
            if let Ok(mut lines) = DATA_LINES.lock() {
                lines.push_back(entry);
                if lines.len() > MAX_DATA_LINES {
                    lines.pop_front();
                }
            }
        } else if let Ok(mut lines) = LOG_LINES.lock() {
            lines.push_back(entry);
            if lines.len() > MAX_LOG_LINES {
                lines.pop_front();
            }
        }
    }

    fn flush(&self) {}
}

static LOGGER: NimrsLogger = NimrsLogger;

pub fn init() {
    log::set_logger(&LOGGER).ok();
    log::set_max_level(log::LevelFilter::Debug);
}

pub fn get_logs_json(filter: &str) -> String {
    let entries: Vec<String> = if filter == "[NIMRS_DATA]" {
        DATA_LINES.lock().unwrap().iter().cloned().collect()
    } else if !filter.is_empty() {
        LOG_LINES
            .lock()
            .unwrap()
            .iter()
            .filter(|l| l.contains(filter))
            .cloned()
            .collect()
    } else {
        LOG_LINES.lock().unwrap().iter().cloned().collect()
    };

    let mut json = String::from('[');
    for (i, entry) in entries.iter().enumerate() {
        if i > 0 {
            json.push(',');
        }
        json.push('"');
        json.push_str(&entry.replace('\\', "\\\\").replace('"', "\\\""));
        json.push('"');
    }
    json.push(']');
    json
}

pub fn clear() {
    if let Ok(mut lines) = LOG_LINES.lock() {
        lines.clear();
    }
    if let Ok(mut lines) = DATA_LINES.lock() {
        lines.clear();
    }
}

fn millis() -> u32 {
    unsafe { esp_idf_sys::esp_timer_get_time() as u32 / 1000 }
}
