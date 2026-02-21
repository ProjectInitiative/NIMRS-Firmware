#include "Arduino.h"
#include "Logger.h"
#include <cassert>
#include <iostream>
#include <string>

// Helper to flush Logger buffer
void flushLogger() {
  for (int i = 0; i < MAX_LOG_LINES + 5; i++) {
    Log.write((const uint8_t *)"\n", 1);
  }
}

void test_write() {
  Serial.clear();
  Log.begin(115200);
  // Logger.begin prints "Logger: Initialized\r\n"

  // Check if Serial got the init message
  // "Logger: Initialized" -> println -> print string + print \n

  // Let's clear buffer first to test explicit write
  Serial.clear();

  Log.write('H');
  Log.write('i');
  Log.write('\n');

  assert(Serial.mockBuffer == "Hi\n");
  std::cout << "test_write passed." << std::endl;
}

void test_buffer_accumulation() {
  Serial.clear();
  flushLogger(); // Clear internal buffer

  Log.write('P');
  Log.write('a');
  Log.write('r');
  Log.write('t');

  // Not flushed to _lines yet because no newline
  String html = Log.getLogsHTML();
  // HTML should effectively be empty of "Part"
  // Note: getLogsHTML returns lines joined by <br>\n
  // It iterates _lines. _currentLine is not in _lines yet.

  assert(html.indexOf("Part") == -1);

  Log.write('\n');
  html = Log.getLogsHTML();
  // Now it should be there.
  assert(html.indexOf("Part") != -1);

  std::cout << "test_buffer_accumulation passed." << std::endl;
}

void test_circular_buffer() {
  flushLogger();

  // Write distinct lines
  for (int i = 0; i < MAX_LOG_LINES + 10; i++) {
    String msg = "UniqueLine " + String(i);
    Log.println(msg);
  }

  String html = Log.getLogsHTML();

  // UniqueLine 0 should be gone (overwritten)
  // We check for "UniqueLine 0" not appearing
  // But "UniqueLine 0" is not substring of "UniqueLine 10" (which has '1' after
  // 'e') Wait "UniqueLine 10" -> "UniqueLine 10" "UniqueLine 0" -> "UniqueLine
  // 0"
  assert(html.indexOf("UniqueLine 0") == -1);

  // Line MAX_LOG_LINES + 9 should be present
  String lastLine = "UniqueLine " + String(MAX_LOG_LINES + 9);
  assert(html.indexOf(lastLine) != -1);

  std::cout << "test_circular_buffer passed." << std::endl;
}

void test_debug() {
  flushLogger();
  Log.setLevel(LOG_INFO);

  Log.debug("Hidden debug message\n");
  String html = Log.getLogsHTML();
  assert(html.indexOf("Hidden debug message") == -1);

  Log.setLevel(LOG_DEBUG);
  Log.debug("Visible debug message\n");
  html = Log.getLogsHTML();
  assert(html.indexOf("Visible debug message") != -1);

  std::cout << "test_debug passed." << std::endl;
}

int main() {
  test_write();
  test_buffer_accumulation();
  test_circular_buffer();
  test_debug();
  std::cout << "All Logger tests passed." << std::endl;
  return 0;
}
