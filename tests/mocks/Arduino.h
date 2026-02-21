#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Time
inline uint32_t millis() { return 0; }
inline void delay(uint32_t ms) {}

// Serial
class HardwareSerial {
public:
    void begin(unsigned long baud) {}
    void print(const char* s) { printf("%s", s); }
    void println(const char* s) { printf("%s\n", s); }
    void println(int i) { printf("%d\n", i); }
    void printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
};
extern HardwareSerial Serial;
