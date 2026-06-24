// Bindings header for dcc_shim
#include <stdint.h>

void dcc_init(uint8_t pin, uint8_t mfr, uint8_t ver, uint8_t flags);
uint8_t dcc_process(void);
uint8_t dcc_get_cv(uint16_t cv);
uint8_t dcc_set_cv(uint16_t cv, uint8_t value);
uint16_t dcc_get_addr(void);
void dcc_set_supercap_enable(uint8_t en);
