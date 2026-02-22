#include "../src/MotorTask.h"
#include <iostream>

// TEST_SOURCES: src/MotorTask.cpp src/BemfEstimator.cpp src/DspFilters.cpp src/RippleDetector.cpp tests/mocks/MotorHal_mock.cpp tests/mocks/mocks.cpp
// TEST_FLAGS: -DUNIT_TEST -DSKIP_MOCK_MOTOR_CONTROLLER

int main() {
  std::cout << "MotorTask compilation test..." << std::endl;
  MotorTask &task = MotorTask::getInstance();
  task.setTargetSpeed(0, true);
  std::cout << "MotorTask instantiated successfully." << std::endl;
  return 0;
}
