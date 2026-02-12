#include "AudioController.h"
#include "Logger.h"
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorWAV.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include "DccController.h"
#include "CvRegistry.h"

AudioController::AudioController()
    : _out(nullptr), _generator(nullptr), _file(nullptr) {}

void AudioController::setup() {
  Log.println("AudioController: Initializing...");

  // I2S Setup
  _out = new AudioOutputI2S();
  _out->SetPinout(Pinout::AMP_BCLK, Pinout::AMP_LRCLK, Pinout::AMP_DIN);
  
  // Initial Volume from CV
  uint8_t vol = DccController::getInstance().getDcc().getCV(CV::MASTER_VOL);
  _out->SetGain(vol / 255.0f);

  // Note: Generator is allocated on playFile()

  // Enable Amp
  pinMode(Pinout::AMP_SD_MODE, OUTPUT);
  digitalWrite(Pinout::AMP_SD_MODE, HIGH);

  Log.println("AudioController: Ready.");
}

void AudioController::loop() {
  // Update Volume dynamically
  if (_out) {
      uint8_t vol = DccController::getInstance().getDcc().getCV(CV::MASTER_VOL);
      _out->SetGain(vol / 255.0f);
  }

  if (_generator && _generator->isRunning()) {
    if (!_generator->loop()) {
      _generator->stop();
      Log.println("Audio: Playback Finished");
    }
  }
}

void AudioController::playFile(const char *filename) {
  // Stop and clean up existing playback
  if (_generator) {
      if (_generator->isRunning()) _generator->stop();
      delete _generator;
      _generator = nullptr;
  }
  if (_file) {
      delete _file;
      _file = nullptr;
  }

  if (!LittleFS.exists(filename)) {
    Log.printf("Audio: File not found: %s\n", filename);
    return;
  }

  _file = new AudioFileSourceLittleFS(filename);
  
  String fn = String(filename);
  if (fn.endsWith(".mp3") || fn.endsWith(".MP3")) {
      Log.println("Audio: Detected MP3");
      _generator = new AudioGeneratorMP3();
  } else {
      Log.println("Audio: Detected WAV");
      _generator = new AudioGeneratorWAV();
  }

  _generator->begin(_file, _out);
  Log.printf("Audio: Playing %s\n", filename);
}

void AudioController::stop() {
  if (_generator && _generator->isRunning())
    _generator->stop();
    
  if (_generator) { delete _generator; _generator = nullptr; }
  if (_file) { delete _file; _file = nullptr; }
}