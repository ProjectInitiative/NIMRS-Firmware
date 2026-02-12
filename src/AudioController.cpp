#include "AudioController.h"
#include "Logger.h"
#include <AudioOutputI2S.h>
#include <AudioGeneratorWAV.h>
#include <AudioFileSourceLittleFS.h>

AudioController::AudioController() : _out(nullptr), _wav(nullptr), _file(nullptr) {}

void AudioController::setup() {
    Log.println("AudioController: Initializing...");
    
    // I2S Setup
    _out = new AudioOutputI2S();
    _out->SetPinout(Pinout::AMP_BCLK, Pinout::AMP_LRCLK, Pinout::AMP_DIN);
    _out->SetGain(0.1); 

    _wav = new AudioGeneratorWAV();
    
    // Enable Amp
    pinMode(Pinout::AMP_SD_MODE, OUTPUT);
    digitalWrite(Pinout::AMP_SD_MODE, HIGH);

    Log.println("AudioController: Ready.");
}

void AudioController::loop() {
    if (_wav && _wav->isRunning()) {
        if (!_wav->loop()) {
            _wav->stop();
            Log.println("Audio: Playback Finished");
        }
    }
}

void AudioController::playFile(const char* filename) {
    if (_wav->isRunning()) _wav->stop();
    if (_file) delete _file;

    if (!LittleFS.exists(filename)) {
        Log.printf("Audio: File not found: %s\n", filename);
        return;
    }

    _file = new AudioFileSourceLittleFS(filename);
    _wav->begin(_file, _out);
    Log.printf("Audio: Playing %s\n", filename);
}

void AudioController::stop() {
    if (_wav && _wav->isRunning()) _wav->stop();
    if (_file) { delete _file; _file = nullptr; }
}