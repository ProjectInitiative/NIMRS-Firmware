// clang-format off
// TEST_SOURCES: tests/mocks/mocks.cpp
// clang-format on

#include "AudioUtils.h"
#include <cassert>
#include <iostream>

#define TEST_CASE(name) void name()
#define RUN_TEST(name)                                                         \
  std::cout << "Running " << #name << "... ";                                  \
  name();                                                                      \
  std::cout << "PASSED" << std::endl;

TEST_CASE(test_isMp3File_null) { assert(isMp3File(nullptr) == false); }

TEST_CASE(test_isMp3File_empty) { assert(isMp3File("") == false); }

TEST_CASE(test_isMp3File_short) {
  assert(isMp3File("mp3") == false);
  assert(isMp3File(".mp3") == true);
  assert(isMp3File(".MP3") == true);
}

TEST_CASE(test_isMp3File_valid) {
  assert(isMp3File("song.mp3") == true);
  assert(isMp3File("SONG.MP3") == true);
  assert(isMp3File("Song.Mp3") == true);
  assert(isMp3File("path/to/song.mp3") == true);
}

TEST_CASE(test_isMp3File_invalid) {
  assert(isMp3File("song.wav") == false);
  assert(isMp3File("song.txt") == false);
  assert(isMp3File("song.mp3.wav") == false);
  assert(isMp3File("songmp3") == false);
}

int main() {
  RUN_TEST(test_isMp3File_null);
  RUN_TEST(test_isMp3File_empty);
  RUN_TEST(test_isMp3File_short);
  RUN_TEST(test_isMp3File_valid);
  RUN_TEST(test_isMp3File_invalid);
  std::cout << "All AudioUtils tests passed!" << std::endl;
  return 0;
}
