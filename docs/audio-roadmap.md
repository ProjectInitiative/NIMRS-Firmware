# NIMRS Audio Engine Roadmap

This document outlines the architecture and implementation plan for adding high-quality, dynamic audio capabilities to the NIMRS-21Pin-Decoder.

## 1. Objectives

- **Polyphonic Sound:** Ability to play multiple sounds simultaneously (e.g., Engine Chuff + Whistle + Bell).
- **Dynamic Logic:** Support for complex behaviors like "Loop while held", "Play tail on release", and "RPM-dependent Chuff".
- **Hybrid Configuration:**
  - **JSON:** Defines "Sound Assets" (complex logic, file paths) and assigns them a unique **Sound ID**.
  - **CVs:** Maps DCC Functions (F0-F28) to these **Sound IDs**, allowing standard DCC remapping.
- **Format Support:** Uncompressed WAV (low latency) and Compressed MP3/AAC (storage efficiency).

## 2. Architecture

### 2.1 Hardware Layer

- **DAC/Amp:** MAX98357A (I2S Standard).
- **Pins (ESP32-S3):**
  - **BCLK:** GPIO 38
  - **LRCLK:** GPIO 36
  - **DIN:** GPIO 37
  - **SD_MODE:** GPIO 33 (Enable/Mute)
- **Storage:** LittleFS (4MB partition).

### 2.2 Software Stack

We will utilize the **ESP8266Audio** library (compatible with ESP32) for decoding and I2S output, wrapped in our own `AudioController`.

#### The Audio Pipeline

1.  **Source:** File (LittleFS) or Generator.
2.  **Decoder:** MP3/WAV -> PCM Raw.
3.  **Mixer:** Sums multiple PCM streams.
4.  **Gain:** Applies Volume (Master & Per-Channel).
5.  **Output:** I2S DMA Transfer.

#### The "Sound Asset" (JSON Definition)

`sound_assets.json` defines the available sounds and their behaviors, assigning each a unique ID.

**Example `sound_assets.json`:**

```json
{
  "assets": [
    {
      "id": 10,
      "name": "Steam Whistle",
      "type": "complex_loop",
      "files": {
        "intro": "whistle_start.mp3",
        "loop": "whistle_hold.mp3",
        "outro": "whistle_end.mp3"
      }
    },
    {
      "id": 11,
      "name": "Bell",
      "type": "toggle",
      "files": { "loop": "bell_loop.wav" }
    }
  ]
}
```

#### CV Mapping Layer

Users map DCC Functions to Sound IDs using CVs.

- **CV 100:** Audio Map for F0
- **CV 101:** Audio Map for F1
- **CV 102:** Audio Map for F2 -> Set to `10` to play Whistle.
- ...

### 2.3 System Integration

- **Core Allocation:**
  - **Core 0 (Real-time):** `DccController` & `MotorController`.
  - **Core 1 (System/Web):** `AudioController` (Decoding/Mixing).
- **Inter-Core Communication:** `SystemState` mutex ensures safe access. Audio thread reads function state and triggers sounds based on the CV map.

## 3. Implementation Plan

### Phase 1: Dependencies & I2S Setup [DONE]

- **Tasks:**
  - [x] Add `ESP8266Audio` to `common-libs.nix`.
  - [x] Implement `AudioController` class (Singleton).
  - [x] Configure I2S output (BCLK=38, LRCLK=36, DIN=37).
  - [x] Add API endpoint `/api/audio/play` for testing.
- **Verification:**
  1.  Upload a `.wav` file (e.g., `test.wav`) via the Web UI "Files" tab.
  2.  Send POST request: `/api/audio/play?file=test.wav`.
  3.  Confirm audio output from speaker.

### Phase 2: The Sound Asset Engine [TODO]

- **Tasks:**
  - Implement JSON parser for `sound_assets.json`.
  - Implement `SoundAsset` class that handles the logic (intro/loop/outro).
  - Implement `CV Registry` expansion for Audio Mapping (CV 100+).

### Phase 3: The Mixer [TODO]

- **Tasks:**
  - Implement software mixer to sum outputs from multiple active `SoundAssets`.
  - Handle priority/polyphony limits.

### Phase 4: Web UI Integration [TODO]

- **Tasks:**
  - File Manager (upload MP3s).
  - Sound Editor Tab (Edit `sound_assets.json` visually).
  - CV Mapping UI (Dropdowns to map Functions to Sound Names).

## 4. Dependencies & Constraints

- **Memory:** ESP32-S3 has 512KB RAM (+ PSRAM on some modules). We need to manage buffers carefully.
- **Storage:** 4MB LittleFS is plenty for MP3s, tight for many WAVs.
- **CPU:** Audio decoding is heavy. We must ensure it doesn't cause DCC packet loss.
