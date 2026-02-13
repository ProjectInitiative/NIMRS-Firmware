#include "AudioController.h"
#include "Logger.h"
#include <AudioFileSourceLittleFS.h>
#include <AudioGeneratorWAV.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include "DccController.h"
#include "CvRegistry.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

AudioController::AudioController()
    : _out(nullptr), _generator(nullptr), _file(nullptr) {}

void AudioController::setup() {
  Log.println("AudioController: Initializing...");

  // I2S Setup
  _out = new AudioOutputI2S();
  _out->SetPinout(Pinout::AMP_BCLK, Pinout::AMP_LRCLK, Pinout::AMP_DIN);
  _out->SetOutputModeMono(true); // Mono DAC
  
  // Initial Volume from CV
  uint8_t vol = DccController::getInstance().getDcc().getCV(CV::MASTER_VOL);
  _out->SetGain(vol / 255.0f);

  // Load Assets
  loadAssets();

  // Note: Generator is allocated on playFile()

  // Enable Amp (Initially LOW/Muted until playback)
  pinMode(Pinout::AMP_SD_MODE, OUTPUT);
  digitalWrite(Pinout::AMP_SD_MODE, LOW);

  Log.println("AudioController: Ready.");
}

void AudioController::loop() {
  // Update Volume dynamically
  if (_out) {
      uint8_t vol = DccController::getInstance().getDcc().getCV(CV::MASTER_VOL);
      _out->SetGain(vol / 255.0f);
  }

  // Check for Function Changes
  SystemContext &ctx = SystemContext::getInstance();
  // We need a way to peek at state without holding the lock for the whole loop if possible,
  // but for safety we'll lock briefly to copy.
  bool currentFunctions[29];
  {
      ScopedLock lock(ctx);
      memcpy(currentFunctions, ctx.getState().functions, 29 * sizeof(bool));
  }

  // Iterate over Sound Assets to check triggers
  // (We iterate assets because they are the "consumers" of functions)
  // Optimization: Pre-calculate which function maps to which asset? 
  // For now, simple iteration is fine (N < 20).
  
  for (auto const& [id, asset] : _assets) {
      uint16_t cvId = CV::AUDIO_MAP_BASE + id;
      // Get mapped function index (0-28)
      int funcIdx = DccController::getInstance().getDcc().getCV(cvId);
      
      // If mapped to a valid function
      if (funcIdx >= 0 && funcIdx <= 28) {
          bool state = currentFunctions[funcIdx];
          bool lastState = _lastFunctions[funcIdx]; // Note: This logic assumes 1-to-1 or handled properly

          // Detect Change
          if (state != lastState) {
              Log.printf("Audio: Function F%d Changed to %d. Triggering Asset %d (%s)\n", funcIdx, state, id, asset.name.c_str());
              
              if (asset.type == "toggle") {
                  if (state) playFile(asset.fileLoop.c_str());
                  else stop(); // Or playOutro if supported
              } 
              else if (asset.type == "simple" || asset.type == "complex_loop") {
                  if (state) {
                       // Trigger start
                       if (asset.fileIntro.length() > 0) playFile(asset.fileIntro.c_str());
                       else playFile(asset.fileLoop.c_str());
                  }
                  // On release, complex_loop might play outro, simple does nothing
                  else if (!state && asset.type == "complex_loop") {
                       // Ensure we don't just cut off if loop is running?
                       // For now, simple Stop. Phase 3/4 handles complex transitions.
                       stop();
                  }
              }
          }
      }
  }

  // Update history
  memcpy(_lastFunctions, currentFunctions, 29 * sizeof(bool));

  if (_generator && _generator->isRunning()) {
    if (!_generator->loop()) {
      _generator->stop();
      // Auto-mute on finish
      digitalWrite(Pinout::AMP_SD_MODE, LOW);
      Log.println("Audio: Playback Finished");
    }
  }
}

void AudioController::loadAssets() {
    if (!LittleFS.exists("/sound_assets.json")) {
        Log.println("Audio: No sound_assets.json found.");
        return;
    }

    File file = LittleFS.open("/sound_assets.json", "r");
    if (!file) {
        Log.println("Audio: Failed to open sound_assets.json");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Log.printf("Audio: JSON parse error: %s\n", error.c_str());
        file.close();
        return;
    }

    JsonArray assets = doc["assets"];
    _assets.clear();
    for (JsonObject obj : assets) {
        SoundAsset asset;
        asset.id = obj["id"];
        asset.name = obj["name"].as<String>();
        asset.type = obj["type"].as<String>();
        
        JsonObject files = obj["files"];
        if (files.containsKey("intro")) asset.fileIntro = files["intro"].as<String>();
        if (files.containsKey("loop")) asset.fileLoop = files["loop"].as<String>();
        if (files.containsKey("outro")) asset.fileOutro = files["outro"].as<String>();

        _assets[asset.id] = asset;
        Log.printf("Audio: Loaded Asset %d (%s)\n", asset.id, asset.name.c_str());
    }
    file.close();
}

const SoundAsset* AudioController::getAsset(uint8_t id) {
    if (_assets.find(id) != _assets.end()) {
        return &_assets.at(id);
    }
    return nullptr;
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

  // Wake up Amp
  digitalWrite(Pinout::AMP_SD_MODE, HIGH);
  delay(10); // Small warmup

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

  // Mute Amp
  digitalWrite(Pinout::AMP_SD_MODE, LOW);
}