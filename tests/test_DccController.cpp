#include "../src/DccController.h"
#include "../src/SystemContext.h"
#include <cassert>
#include <iostream>

// Helper to reset SystemContext state before each test
void resetState() {
    SystemContext &ctx = SystemContext::getInstance();
    // Reset functions
    for(int i=0; i<29; i++) {
        ctx.getState().functions[i] = false;
    }
}

void test_notifyDccFunc_FN04() {
    resetState();
    SystemContext &ctx = SystemContext::getInstance();

    // Test F0 (Light) -> Bit 4 (FN_BIT_00=0x10)
    notifyDccFunc(0, DCC_ADDR_SHORT, FN_0_4, FN_BIT_00);
    assert(ctx.getState().functions[0] == true);
    assert(ctx.getState().functions[1] == false);

    // Test F1 -> Bit 0 (FN_BIT_01=0x01)
    notifyDccFunc(0, DCC_ADDR_SHORT, FN_0_4, FN_BIT_01);
    assert(ctx.getState().functions[0] == false); // F0 should be cleared if not in mask
    assert(ctx.getState().functions[1] == true);

    // Test F4 -> Bit 3 (FN_BIT_04=0x08)
    notifyDccFunc(0, DCC_ADDR_SHORT, FN_0_4, FN_BIT_04);
    assert(ctx.getState().functions[4] == true);

    std::cout << "test_notifyDccFunc_FN04 passed." << std::endl;
}

void test_notifyDccFunc_FN58() {
    resetState();
    SystemContext &ctx = SystemContext::getInstance();

    // Test F5 -> Bit 0 (FN_BIT_05=0x01)
    notifyDccFunc(0, DCC_ADDR_SHORT, FN_5_8, FN_BIT_05);
    assert(ctx.getState().functions[5] == true);
    assert(ctx.getState().functions[6] == false);

    // Test F8 -> Bit 3 (FN_BIT_08=0x08)
    notifyDccFunc(0, DCC_ADDR_SHORT, FN_5_8, FN_BIT_08);
    assert(ctx.getState().functions[5] == false);
    assert(ctx.getState().functions[8] == true);

    std::cout << "test_notifyDccFunc_FN58 passed." << std::endl;
}

void test_notifyDccFunc_FN912() {
    resetState();
    SystemContext &ctx = SystemContext::getInstance();

    // Test F9 -> Bit 0 (FN_BIT_09=0x01)
    notifyDccFunc(0, DCC_ADDR_SHORT, FN_9_12, FN_BIT_09);
    assert(ctx.getState().functions[9] == true);

    // Test F12 -> Bit 3 (FN_BIT_12=0x08)
    notifyDccFunc(0, DCC_ADDR_SHORT, FN_9_12, FN_BIT_12);
    assert(ctx.getState().functions[9] == false);
    assert(ctx.getState().functions[12] == true);

    std::cout << "test_notifyDccFunc_FN912 passed." << std::endl;
}

void test_notifyDccFunc_clobbering() {
    resetState();
    SystemContext &ctx = SystemContext::getInstance();

    // 1. Enable F9 (FN_9_12)
    notifyDccFunc(0, DCC_ADDR_SHORT, FN_9_12, FN_BIT_09);
    assert(ctx.getState().functions[9] == true);

    // 2. Enable F5 (FN_5_8)
    // This should NOT affect F9
    notifyDccFunc(0, DCC_ADDR_SHORT, FN_5_8, FN_BIT_05);
    assert(ctx.getState().functions[5] == true);

    // 3. Verify F9 is still true
    if (ctx.getState().functions[9] == false) {
        std::cout << "FAILURE: F9 was cleared by FN_5_8 update!" << std::endl;
    }
    assert(ctx.getState().functions[9] == true);

    std::cout << "test_notifyDccFunc_clobbering passed." << std::endl;
}

int main() {
    test_notifyDccFunc_FN04();
    test_notifyDccFunc_FN58();
    test_notifyDccFunc_FN912();
    test_notifyDccFunc_clobbering();
    return 0;
}
