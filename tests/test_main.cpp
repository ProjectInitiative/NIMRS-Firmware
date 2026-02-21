// TEST_SOURCES: tests/mocks/mocks.cpp
#include "Arduino.h"
#include "SystemContext.h"
#include <assert.h>
#include <stdio.h>

void test_system_context() {
  SystemContext &ctx = SystemContext::getInstance();
  SystemState &state = ctx.getState();

  // Initial state check
  // dccAddress default is 3 in SystemContext.h
  assert(state.dccAddress == 3);
  assert(state.speed == 0);
  assert(state.direction == true);

  // Modify state
  {
    ScopedLock lock(ctx);
    state.speed = 128;
  }

  assert(state.speed == 128);
  printf("SystemContext test passed!\n");
}

int main() {
  test_system_context();
  printf("All tests passed.\n");
  return 0;
}
