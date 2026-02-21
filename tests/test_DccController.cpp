#include "../src/DccController.h"
#include "SystemContext.h"
#include <cassert>
#include <iostream>

#define TEST_CASE(name) void name()
#define RUN_TEST(name)                                                         \
  std::cout << "Running " << #name << "... ";                                  \
  name();                                                                      \
  std::cout << "PASSED" << std::endl;

TEST_CASE(test_notifyDccSpeed_EmergencyStop) {
  SystemContext &ctx = SystemContext::getInstance();

  // Reset state
  {
    ScopedLock lock(ctx);
    ctx.getState().speed = 50;
    ctx.getState().speedSource = SOURCE_DCC;
    ctx.getState().lastDccSpeed = 50;
    ctx.getState().lastDccDirection = true;
  }

  // Speed 1 should be treated as Stop (0)
  // notifyDccSpeed(Addr, AddrType, Speed, Dir, SpeedSteps)
  notifyDccSpeed(123, DCC_ADDR_SHORT, 1, DCC_DIR_FWD, DCC_128_STEPS);

  {
    ScopedLock lock(ctx);
    // Verify speed is set to 0
    assert(ctx.getState().speed == 0);
    // Verify source is still DCC
    assert(ctx.getState().speedSource == SOURCE_DCC);
  }
}

TEST_CASE(test_notifyDccSpeed_Normal) {
  SystemContext &ctx = SystemContext::getInstance();

  // Reset state
  {
    ScopedLock lock(ctx);
    ctx.getState().speed = 0;
    ctx.getState().speedSource = SOURCE_DCC;
    ctx.getState().lastDccSpeed = 0;
  }

  // Speed 50
  notifyDccSpeed(123, DCC_ADDR_SHORT, 50, DCC_DIR_FWD, DCC_128_STEPS);

  {
    ScopedLock lock(ctx);
    assert(ctx.getState().speed == 50);
  }
}

TEST_CASE(test_notifyDccSpeed_Stop) {
  SystemContext &ctx = SystemContext::getInstance();

  // Reset state
  {
    ScopedLock lock(ctx);
    ctx.getState().speed = 50;
    ctx.getState().speedSource = SOURCE_DCC;
    ctx.getState().lastDccSpeed = 50;
  }

  // Speed 0
  notifyDccSpeed(123, DCC_ADDR_SHORT, 0, DCC_DIR_FWD, DCC_128_STEPS);

  {
    ScopedLock lock(ctx);
    assert(ctx.getState().speed == 0);
  }
}

int main() {
  RUN_TEST(test_notifyDccSpeed_EmergencyStop);
  RUN_TEST(test_notifyDccSpeed_Normal);
  RUN_TEST(test_notifyDccSpeed_Stop);
  std::cout << "All DccController tests passed!" << std::endl;
  return 0;
}
