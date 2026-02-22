#ifndef NIMRS_LOGGER_H
#define NIMRS_LOGGER_H

#include <Arduino.h>
#include <deque>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

// Max log lines to keep in RAM
#define MAX_LOG_LINES 128
#define MAX_DATA_LINES 32

enum LogLevel {
  LOG_DEBUG = 0,
  LOG_INFO = 1,
  LOG_WARN = 2,
  LOG_ERROR = 3,
  LOG_DATA = 4
};

struct LogMessage {
  char text[128];
};

class Logger : public Print {
public:
  static Logger &getInstance() {
    static Logger instance;
    return instance;
  }

  // Initialize (call in setup)
  void begin(unsigned long baud) {
    Serial.begin(baud);
    _serialEnabled = true;
    println("Logger: Initialized");
  }

  void startTask();

  void setLevel(LogLevel level) { _minLevel = level; }

  // Helper for printf style since Print::printf might not be virtual/available
  // everywhere
  size_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

  // Debug logging (only if level <= LOG_DEBUG)
  void debug(const char *format, ...);

  // Print interface implementation
  virtual size_t write(uint8_t c) override;
  virtual size_t write(const uint8_t *buffer, size_t size) override;

  // Access for Web Server
  String getLogsHTML();
  String getLogsJSON(const String &filter = "");

private:
  Logger();
  std::deque<String> _lines;     // System logs (INFO, WARN, ERROR)
  std::deque<String> _dataLines; // Telemetry logs ([NIMRS_DATA])
  String _currentLine;           // Buffer for partial writes (print vs println)
  LogLevel _minLevel = LOG_INFO; // Default to INFO to suppress debug noise
  bool _serialEnabled = false;

  SemaphoreHandle_t _historyMutex;
  SemaphoreHandle_t _bufferMutex;
  QueueHandle_t _logQueue;
  TaskHandle_t _taskHandle;

  void _addToBuffer(const String &line);
  static void _taskEntry(void *param);
  void _processQueue();
};

// Global accessor helper
#define Log Logger::getInstance()

#endif
