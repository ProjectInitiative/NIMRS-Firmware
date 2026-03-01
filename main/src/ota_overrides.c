#include <esp_ota_ops.h>
#include <esp_log.h>
#include <stdbool.h>
#include <stdio.h>
#include <rom/ets_sys.h>

/**
 * NIMRS OTA STABILITY OVERRIDES
 * 
 * These functions override the weak implementations in the Arduino-ESP32 core.
 * Arduino's 'initArduino()' calls these to decide if it should automatically
 * mark the new firmware as VALID. 
 * 
 * By returning 'true' from verifyRollbackLater(), we stop the auto-validation
 * and stay in the 'PENDING_VERIFY' state until our own timer marks it good.
 */

bool verifyRollbackLater() {
    printf("\n[NIMRS] Rollback Guard: DEFERRING validation.\n");
    return true; 
}

bool verifyOta() {
    // If verifyRollbackLater was ignored and this is called, 
    // returning false prevents marking valid.
    printf("[NIMRS] Rollback Guard: REJECTING auto-verify.\n");
    return false;
}
