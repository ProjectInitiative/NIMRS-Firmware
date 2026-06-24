#[no_mangle]
pub extern "C" fn verifyRollbackLater() -> bool {
    true
}

#[no_mangle]
pub extern "C" fn verifyOta() -> bool {
    false
}
