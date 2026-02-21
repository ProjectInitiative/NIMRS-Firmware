#!/bin/bash
g++ -std=c++17 -Itests/mocks_audio -Itests/mocks -Itests/mocks/freertos -Isrc -o tests/test_audio tests/test_AudioController_Optimization.cpp src/AudioController.cpp tests/mocks_audio/mocks_audio.cpp
