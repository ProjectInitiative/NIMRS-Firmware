// Thin C-compatible shim around NmraDcc for Rust FFI
#include <NmraDcc.h>
#include <stdint.h>

static NmraDcc dcc;

extern "C" {
    void dcc_init(uint8_t pin, uint8_t mfr, uint8_t ver, uint8_t flags) {
        dcc.pin(pin, 1);
        dcc.init(mfr, ver, flags, 0);
    }

    uint8_t dcc_process(void) {
        return dcc.process();
    }

    uint8_t dcc_get_cv(uint16_t cv) {
        return dcc.getCV(cv);
    }

    uint8_t dcc_set_cv(uint16_t cv, uint8_t value) {
        return dcc.setCV(cv, value);
    }

    uint16_t dcc_get_addr(void) {
        return dcc.getAddr();
    }

    void dcc_set_supercap_enable(uint8_t en) {
        // SuperCap control happens on Rust side via GPIO
    }
}
