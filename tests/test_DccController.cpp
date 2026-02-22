// TEST_SOURCES: src/DccController.cpp tests/mocks/mocks.cpp
// TEST_FLAGS: -DSKIP_MOCK_DCC_CONTROLLER -DUNIT_TEST

#include "../src/DccController.h" // Use source header explicitly
#include "SystemContext.h"
#include <assert.h>
#include <stdio.h>

void test_notifyDccSpeed_emergency_stop() {
  SystemContext &ctx = SystemContext::getInstance();
  SystemState &state = ctx.getState();

  // Reset state
  {
    ScopedLock lock(ctx);
    state.speed = 100;
    state.speedSource = SOURCE_DCC;
    state.lastDccSpeed = 100;
  }

  // Act: Speed 1 (Emergency Stop)
  // Addr=3, AddrType=0, Speed=1, Dir=FWD, Steps=128
  notifyDccSpeed(3, 0, 1, DCC_DIR_FWD, 128);

  // Assert
  assert(state.speed == 0);
  printf("test_notifyDccSpeed_emergency_stop passed.\n");
}

void test_notifyDccSpeed_stop() {
  SystemContext &ctx = SystemContext::getInstance();
  SystemState &state = ctx.getState();

  // Reset state
  {
    ScopedLock lock(ctx);
    state.speed = 50;
    state.speedSource = SOURCE_DCC;
    state.lastDccSpeed = 50;
  }

  // Act: Speed 0 (Stop)
  notifyDccSpeed(3, 0, 0, DCC_DIR_FWD, 128);

  // Assert
  assert(state.speed == 0);
  printf("test_notifyDccSpeed_stop passed.\n");
}

void test_notifyDccSpeed_valid_speed() {
  SystemContext &ctx = SystemContext::getInstance();
  SystemState &state = ctx.getState();

  // Reset state
  {
    ScopedLock lock(ctx);
    state.speed = 0;
    state.speedSource = SOURCE_DCC;
    state.lastDccSpeed = 0;
  }

  // Act: Speed 50
  notifyDccSpeed(3, 0, 50, DCC_DIR_FWD, 128);

  // Assert
  assert(state.speed == 50);
  printf("test_notifyDccSpeed_valid_speed passed.\n");
}

int main() {
  test_notifyDccSpeed_emergency_stop();
  test_notifyDccSpeed_stop();
  test_notifyDccSpeed_valid_speed();

  printf("All DccController tests passed.\n");
  return 0;
}
