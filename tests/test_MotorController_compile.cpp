#include "../src/MotorController.h"
#include <iostream>

// clang-format off
// TEST_SOURCES: src/MotorController.cpp src/MotorTask.cpp src/BemfEstimator.cpp src/DspFilters.cpp src/RippleDetector.cpp src/DccController.cpp tests/mocks/MotorHal_mock.cpp tests/mocks/mocks.cpp
// TEST_FLAGS: -DUNIT_TEST -DSKIP_MOCK_MOTOR_CONTROLLER -DSKIP_MOCK_DCC_CONTROLLER
// clang-format on

int main() {
  std::cout << "MotorController compilation test..." << std::endl;
  MotorController &ctrl = MotorController::getInstance();
  ctrl.setup();
  std::cout << "MotorController instantiated successfully." << std::endl;
  return 0;
}
