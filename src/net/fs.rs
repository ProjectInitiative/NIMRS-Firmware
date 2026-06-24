pub struct FileManager;

impl FileManager {
    pub fn init() -> bool {
        log::info!("FS: LittleFS initialization placeholder");
        true
    }

    pub fn list_files() -> Vec<FileInfo> {
        Vec::new()
    }

    pub fn delete_file(_path: &str) -> bool {
        false
    }

    pub fn format() -> bool {
        false
    }

    pub fn read_file(_path: &str) -> Option<Vec<u8>> {
        None
    }

    pub fn write_file(_path: &str, _data: &[u8]) -> bool {
        false
    }

    pub fn total_bytes() -> u64 {
        0
    }

    pub fn used_bytes() -> u64 {
        0
    }
}

pub struct FileInfo {
    pub name: String,
    pub size: u32,
}
