#include "Logger.h"
#include <stdarg.h>
#include <stdio.h>

// Logger methods not inline in header
size_t Logger::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  // vprintf(format, arg);
  va_end(arg);
  return 0;
}
void Logger::debug(const char *format, ...) {}
size_t Logger::write(uint8_t c) { return 1; }
size_t Logger::write(const uint8_t *buffer, size_t size) { return size; }
String Logger::getLogsJSON(const String &filter) { return "[]"; }
String Logger::getLogsHTML() { return ""; }
void Logger::_addToBuffer(const String &line) {}
