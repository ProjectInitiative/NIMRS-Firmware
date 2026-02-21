#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// Mocking the minimal parts needed for the benchmark

struct TestPoint {
  uint32_t t;
  uint8_t target;
  uint16_t pwm;
  float current;
  float speed;
};

static const int MAX_TEST_POINTS = 60;
TestPoint _testData[MAX_TEST_POINTS];
int _testDataIdx = MAX_TEST_POINTS;

void setupTestData() {
  for (int i = 0; i < MAX_TEST_POINTS; i++) {
    _testData[i] = {(uint32_t)(i * 50), (uint8_t)(i % 100), (uint16_t)(i * 10),
                    (float)(i * 0.1), (float)(i * 0.5)};
  }
}

// Original implementation
std::string getTestJSON_Original() {
  std::string out = "[";
  for (int i = 0; i < _testDataIdx; i++) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"t\":%u,\"tgt\":%u,\"pwm\":%u,\"cur\":%.3f,\"spd\":%.1f}",
             _testData[i].t, _testData[i].target, _testData[i].pwm,
             _testData[i].current, _testData[i].speed);
    out += buf;
    if (i < _testDataIdx - 1)
      out += ",";
  }
  out += "]";
  return out;
}

// Optimized implementation using reserve
std::string getTestJSON_Reserved() {
  std::string out;
  out.reserve(MAX_TEST_POINTS * 64 + 2); // approximate size
  out = "[";
  for (int i = 0; i < _testDataIdx; i++) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"t\":%u,\"tgt\":%u,\"pwm\":%u,\"cur\":%.3f,\"spd\":%.1f}",
             _testData[i].t, _testData[i].target, _testData[i].pwm,
             _testData[i].current, _testData[i].speed);
    out += buf;
    if (i < _testDataIdx - 1)
      out += ",";
  }
  out += "]";
  return out;
}

class Print {
public:
  virtual size_t write(const char *s) = 0;
  virtual size_t print(const char *s) { return write(s); }
};

class MockPrint : public Print {
  size_t totalBytes = 0;

public:
  size_t write(const char *s) override {
    size_t len = strlen(s);
    totalBytes += len;
    return len;
  }
  size_t getTotal() const { return totalBytes; }
  void reset() { totalBytes = 0; }
};

// Streaming implementation
void printTestJSON_Streaming(Print &p) {
  p.print("[");
  for (int i = 0; i < _testDataIdx; i++) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"t\":%u,\"tgt\":%u,\"pwm\":%u,\"cur\":%.3f,\"spd\":%.1f}",
             _testData[i].t, _testData[i].target, _testData[i].pwm,
             _testData[i].current, _testData[i].speed);
    p.print(buf);
    if (i < _testDataIdx - 1)
      p.print(",");
  }
  p.print("]");
}

int main() {
  setupTestData();

  const int ITERATIONS = 10000;

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ITERATIONS; i++) {
    std::string s = getTestJSON_Original();
    volatile size_t len = s.length(); // prevent optimization
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed_original = end - start;
  std::cout << "Original: " << elapsed_original.count() << " ms" << std::endl;

  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ITERATIONS; i++) {
    std::string s = getTestJSON_Reserved();
    volatile size_t len = s.length();
  }
  end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed_reserved = end - start;
  std::cout << "Reserved: " << elapsed_reserved.count() << " ms" << std::endl;

  MockPrint p;
  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ITERATIONS; i++) {
    p.reset();
    printTestJSON_Streaming(p);
    volatile size_t len = p.getTotal();
  }
  end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed_streaming = end - start;
  std::cout << "Streaming: " << elapsed_streaming.count() << " ms" << std::endl;

  return 0;
}
