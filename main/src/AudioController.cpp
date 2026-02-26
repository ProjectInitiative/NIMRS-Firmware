#include "AudioController.h"
#include "AudioUtils.h"
#include "CvRegistry.h"
#include "DccController.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cstring>
#include <strings.h>

AudioController::AudioController()
    : _i2s(nullptr), _volume(nullptr), _decoder(nullptr), _mp3(nullptr),
      _wav(nullptr), _copier(nullptr) {}

void AudioController::setup() {
  Log.println("AudioController: Initializing...");

  // I2S Setup
  _i2s = new I2SStream();
  auto config = _i2s->defaultConfig(TX_MODE);
  config.pin_bck = Pinout::AMP_BCLK;
  config.pin_ws = Pinout::AMP_LRCLK;
  config.pin_data = Pinout::AMP_DIN;
  config.channels = 1;
  config.sample_rate = 44100;
  _i2s->begin(config);

  _volume = new VolumeStream(*_i2s);
  _volume->begin(config); // VolumeStream uses the same config

  _mp3 = new MP3DecoderHelix();
  _wav = new WAVDecoder();
  _decoder = new EncodedAudioStream(_volume, _mp3); // Default decoder

  _copier = new StreamCopy(*_decoder, _file);

  // Initial Volume from CV
  uint8_t vol = DccController::getInstance().getDcc().getCV(CV::MASTER_VOL);
  float gain = (vol / 255.0f); // Map 0-255 to 0.0-1.0
  _volume->setVolume(gain);

  // Load Assets
  loadAssets();

  // Enable Amp (Initially LOW/Muted until playback)
  pinMode(Pinout::AMP_SD_MODE, OUTPUT);
  digitalWrite(Pinout::AMP_SD_MODE, LOW);

  Log.println("AudioController: Ready.");
}

void AudioController::loop() {
  // Update Volume dynamically
  if (_volume) {
    uint8_t vol = DccController::getInstance().getDcc().getCV(CV::MASTER_VOL);
    _volume->setVolume((vol / 255.0f));
  }

  // Check for Function Changes
  SystemContext &ctx = SystemContext::getInstance();
  bool currentFunctions[29];
  {
    ScopedLock lock(ctx);
    memcpy(currentFunctions, ctx.getState().functions, 29 * sizeof(bool));
  }

  for (auto const &[id, asset] : _assets) {
    uint16_t cvId = CV::AUDIO_MAP_BASE + id;
    int funcIdx = DccController::getInstance().getDcc().getCV(cvId);

    if (funcIdx >= 0 && funcIdx <= 28) {
      bool state = currentFunctions[funcIdx];
      bool lastState = _lastFunctions[funcIdx];

      if (state != lastState) {
        Log.printf(
            "Audio: Function F%d Changed to %d. Triggering Asset %d (%s)\n",
            funcIdx, state, id, asset.name.c_str());

        if (asset.type == "toggle") {
          if (state)
            playFile(asset.fileLoop.c_str());
          else
            stop();
        } else if (asset.type == "simple" || asset.type == "complex_loop") {
          if (state) {
            if (asset.fileIntro.length() > 0)
              playFile(asset.fileIntro.c_str());
            else
              playFile(asset.fileLoop.c_str());
          } else if (!state && asset.type == "complex_loop") {
            stop();
          }
        }
      }
    }
  }

  memcpy(_lastFunctions, currentFunctions, 29 * sizeof(bool));

  if (_playing && _copier) {
    if (!_copier->copy()) {
      stop();
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
    if (files["intro"].is<String>())
      asset.fileIntro = files["intro"].as<String>();
    if (files["loop"].is<String>())
      asset.fileLoop = files["loop"].as<String>();
    if (files["outro"].is<String>())
      asset.fileOutro = files["outro"].as<String>();

    _assets[asset.id] = asset;
    Log.printf("Audio: Loaded Asset %d (%s)\n", asset.id, asset.name.c_str());
  }
  file.close();
}

void AudioController::playFile(const char *filename) {
  if (filename == nullptr) {
    Log.println("Audio: Invalid filename (null)");
    return;
  }

  stop();

  // Wake up Amp
  digitalWrite(Pinout::AMP_SD_MODE, HIGH);
  delay(10); // Small warmup

  _file = LittleFS.open(filename, "r");
  if (!_file) {
    Log.printf("Audio: File not found: %s\n", filename);
    digitalWrite(Pinout::AMP_SD_MODE, LOW);
    return;
  }

  if (isMp3File(filename)) {
    Log.println("Audio: Detected MP3");
    _decoder->setDecoder(_mp3);
  } else {
    Log.println("Audio: Detected WAV");
    _decoder->setDecoder(_wav);
  }

  _decoder->begin();
  _copier->begin(*_decoder, _file);
  _playing = true;
  Log.printf("Audio: Playing %s\n", filename);
}

void AudioController::stop() {
  if (_playing) {
    _playing = false;
    _decoder->end();
    if (_file) {
      _file.close();
    }
  }
  // Mute Amp
  digitalWrite(Pinout::AMP_SD_MODE, LOW);
}