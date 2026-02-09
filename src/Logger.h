#ifndef NIMRS_LOGGER_H
#define NIMRS_LOGGER_H

#include <Arduino.h>
#include <deque>

// Max log lines to keep in RAM
#define MAX_LOG_LINES 50

class Logger : public Print {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // Initialize (call in setup)
    void begin(unsigned long baud) {
        Serial.begin(baud);
        println("Logger: Initialized");
    }

    // Helper for printf style since Print::printf might not be virtual/available everywhere
    size_t printf(const char *format, ...)  __attribute__ ((format (printf, 2, 3)));

    // Print interface implementation
    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    // Access for Web Server
    String getLogsHTML();
    String getLogsJSON();

private:
    Logger() {}
    std::deque<String> _lines;
    String _currentLine; // Buffer for partial writes (print vs println)

    void _addToBuffer(const String& line);
};

// Global accessor helper
#define Log Logger::getInstance()

#endif
