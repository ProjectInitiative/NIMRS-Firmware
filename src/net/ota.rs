pub fn start_update() -> bool {
    // OTA update will be implemented when Arduino Update
    // equivalent is needed. For now this is a placeholder.
    log::info!("OTA: start_update not yet implemented");
    false
}

pub fn write_chunk(_data: &[u8]) -> bool {
    false
}

pub fn end_update(_success: bool) -> bool {
    false
}
