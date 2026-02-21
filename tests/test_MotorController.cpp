#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// Mock everything before including the class under test
#define private public
#include "../src/MotorController.h"
#undef private

// Mock Print implementation
class StringPrint : public Print {
public:
  std::string output;
  size_t write(uint8_t c) override {
    output += (char)c;
    return 1;
  }
  size_t write(const uint8_t *buf, size_t size) override {
    output.append((const char *)buf, size);
    return size;
  }
};

void test_printTestJSON() {
  MotorController &mc = MotorController::getInstance();

  // Setup test data manually
  mc._testDataIdx = 2;
  mc._testData[0] = {100, 50, 1024, 0.5f, 10.0f};
  mc._testData[1] = {200, 60, 2048, 1.0f, 20.0f};

  StringPrint p;
  mc.printTestJSON(p);

  std::cout << "JSON: " << p.output << std::endl;

  std::string expected =
      "[{\"t\":100,\"tgt\":50,\"pwm\":1024,\"cur\":0.500,\"spd\":10.0},{\"t\":"
      "200,\"tgt\":60,\"pwm\":2048,\"cur\":1.000,\"spd\":20.0}]";
  assert(p.output == expected);
}

int main() {
  test_printTestJSON();
  std::cout << "test_MotorController PASSED" << std::endl;
  return 0;
}
