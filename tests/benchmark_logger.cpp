#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <deque>

// Need to make sure Logger's private members are accessible
#define private public

// Include mocks
#include "Arduino.h"
#include "ArduinoJson.h"

// Define extern variables declared in Arduino.h
MockSerial Serial;
ESPClass ESP;

// Include Logger source directly
// We use relative path because this file is in tests/
// But we compile from root, so includes in Logger.cpp (like #include "Logger.h") will look in -Isrc
// This include here is textual inclusion.
#include "../src/Logger.cpp"

#undef private

int main() {
    Logger& logger = Logger::getInstance();
    // logger.begin(115200); // Initialize - Disabled to avoid Serial I/O overhead

    // 1. Correctness Check
    logger.println("Test Message");
    // println calls write('\n') which calls _addToBuffer

    if (logger._lines.empty()) {
        std::cerr << "Error: _lines is empty!" << std::endl;
        return 1;
    }

    String lastLog = logger._lines.back();
    // Expected format: "[1000] Test Message" (millis is mocked to 1000 in Arduino.h)

    // Check if it contains the message and timestamp format
    // Note: Arduino String implementation in mocks/Arduino.h might behave slightly differently but basic operators should work.
    // However, the mock String inherits std::string.

    if (lastLog.find("[1000] Test Message") == std::string::npos) {
         std::cerr << "Error: Log format incorrect. Got: " << lastLog.c_str() << std::endl;
         return 1;
    }
    std::cout << "Correctness check passed: " << lastLog.c_str() << std::endl;

    // Clear logs
    logger._lines.clear();

    // 2. Performance Benchmark
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; ++i) {
        // Constructing a string here adds overhead, but it's constant across tests.
        // We want to measure the overhead of _addToBuffer mainly.
        logger.println("Benchmark Log Entry " + String(i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Time taken for 100,000 logs: " << diff.count() << " s" << std::endl;
    std::cout << "Average time per log: " << (diff.count() / 100000.0) * 1e6 << " us" << std::endl;

    return 0;
}
