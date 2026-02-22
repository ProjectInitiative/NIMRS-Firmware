// TEST_SOURCES: tests/mocks/mocks.cpp
// TEST_FLAGS: -DUNIT_TEST -DSKIP_MOCK_AUDIO_CONTROLLER

#include "Arduino.h"
#include "AudioFileSourceLittleFS.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"
#include "LittleFS.h"

// Include source file directly to test internal logic
#include "../src/AudioController.cpp"

void test_playFile_checks_exists() {
  // Setup
  LittleFS.callCount_exists = 0;
  LittleFS.callCount_open = 0;

  // Create a dummy file in LittleFS mock
  LittleFS.mockFiles.push_back(File("/test.mp3", 1024));

  // Get instance (singleton)
  AudioController &audio = AudioController::getInstance();

  // Act
  audio.playFile("/test.mp3");

  // Assert
  // Current behavior: calls exists() then open()
  if (LittleFS.callCount_exists != 0) {
    printf("FAIL: callCount_exists should be 0 after optimization\n");
    exit(1);
  }
  if (LittleFS.callCount_open == 0) {
    printf("FAIL: callCount_open should be > 0\n");
    exit(1);
  }

  printf("PASS: playFile called open() but not exists()\n");
}

int main() {
  test_playFile_checks_exists();
  return 0;
}
