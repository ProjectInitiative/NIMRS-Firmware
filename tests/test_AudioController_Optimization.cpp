#include "AudioController.h"
#include "LittleFS.h"
#include <assert.h>
#include <stdio.h>

void test_playFile_exists() {
  printf("Test: playFile_exists\n");
  // Setup
  LittleFS.callCount_exists = 0;
  LittleFS.callCount_open = 0;
  LittleFS.mockFiles.clear();
  LittleFS.mockFiles.push_back(File("test.mp3", 1024, false));

  AudioController &audio = AudioController::getInstance();
  audio.playFile("test.mp3");

  printf("Calls to LittleFS.exists: %d\n", LittleFS.callCount_exists);
  printf("Calls to LittleFS.open: %d\n", LittleFS.callCount_open);
}

void test_playFile_not_exists() {
  printf("Test: playFile_not_exists\n");
  // Setup
  LittleFS.callCount_exists = 0;
  LittleFS.callCount_open = 0;
  LittleFS.mockFiles.clear();

  AudioController &audio = AudioController::getInstance();
  audio.playFile("missing.mp3");

  printf("Calls to LittleFS.exists: %d\n", LittleFS.callCount_exists);
  printf("Calls to LittleFS.open: %d\n", LittleFS.callCount_open);
}

int main() {
  test_playFile_exists();
  test_playFile_not_exists();
  return 0;
}
