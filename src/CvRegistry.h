#ifndef CV_REGISTRY_H
#define CV_REGISTRY_H

#include <stdint.h>

// 1. Constants for Code Usage
namespace CV {
static constexpr uint16_t ADDR_SHORT = 1;
static constexpr uint16_t V_START = 2;
static constexpr uint16_t ACCEL = 3;
static constexpr uint16_t DECEL = 4;
static constexpr uint16_t V_HIGH = 5;
static constexpr uint16_t V_MID = 6;
static constexpr uint16_t DECODER_VERSION = 7;
static constexpr uint16_t DECODER_MAN_ID = 8;
static constexpr uint16_t PWM_FREQ = 9;    // Frequency Low Byte
static constexpr uint16_t PWM_FREQ_H = 10; // Frequency High Byte
static constexpr uint16_t ADDR_LONG_MSB = 17;
static constexpr uint16_t ADDR_LONG_LSB = 18;
static constexpr uint16_t CONFIG = 29;

// Audio
static constexpr uint16_t MASTER_VOL = 50;
static constexpr uint16_t AUDIO_MAP_BASE =
    100; // CV = 100 + SoundID. Value = Function (0-28)
static constexpr uint16_t CHUFF_RATE = 133;
static constexpr uint16_t CHUFF_DRAG = 134;

// Motor Control (Hybrid Torque Strategy)
static constexpr uint16_t LOAD_GAIN = 60; // Grade Compensation Strength (Ki)
static constexpr uint16_t BASELINE_ALPHA = 61;  // Learning Speed
static constexpr uint16_t STICTION_KICK = 62;   // Initial Pulse
static constexpr uint16_t DELTA_CAP = 63;       // Max Boost
static constexpr uint16_t PWM_DITHER = 64;      // Micro-vibration
static constexpr uint16_t BASELINE_RESET = 65;  // Command: Wipe Baseline
static constexpr uint16_t CURVE_INTENSITY = 66; // Parametric S-Curve Strength

// Advanced Tuning
static constexpr uint16_t DRIVE_MODE = 144;       // 0=Fast, 1=Slow Decay
static constexpr uint16_t PEDESTAL_FLOOR = 57;    // Min PWM Floor (0-255)
static constexpr uint16_t LOAD_GAIN_SCALAR = 146; // Multiplier for CV 60
static constexpr uint16_t LEARN_THRESHOLD = 147;  // Min speed to learn baseline
static constexpr uint16_t HARDWARE_GAIN = 148;    // 0=Low, 1=High

// Function Mapping
static constexpr uint16_t FRONT = 33;
static constexpr uint16_t REAR = 34;
static constexpr uint16_t AUX1 = 35;
static constexpr uint16_t AUX2 = 36;
static constexpr uint16_t AUX3 = 37;
static constexpr uint16_t AUX4 = 38;
static constexpr uint16_t AUX5 = 39;
static constexpr uint16_t AUX6 = 40;
static constexpr uint16_t AUX7 = 41;
static constexpr uint16_t AUX8 = 42;
} // namespace CV

// 2. Metadata for UI and Factory Reset
struct CvDef {
  uint16_t id;
  uint8_t defaultValue;
  const char *name;
  const char *desc;
};

// The Registry
static const CvDef CV_DEFS[] = {
    {CV::ADDR_SHORT, 3, "Primary Address", "Short Address (1-127)"},
    {CV::V_START, 60, "Vstart", "Starting Voltage (0-255). Min floor is 40."},
    {CV::ACCEL, 2, "Acceleration", "Momentum Delay (Rate)"},
    {CV::DECEL, 2, "Deceleration", "Momentum Delay (Rate)"},
    {CV::V_HIGH, 255, "Vhigh", "Max Voltage/Speed"},
    {CV::V_MID, 128, "Vmid", "Mid-range Speed Curve"},
    {CV::DECODER_VERSION, 14, "Version ID", "Read-only Version"},
    {CV::DECODER_MAN_ID, 13, "Manufacturer", "Read-only Man ID (DIY=13)"},
    {CV::PWM_FREQ, 128, "PWM Freq Low", "Freq Low Byte (Default 16kHz)"},
    {CV::PWM_FREQ_H, 62, "PWM Freq High", "Freq High Byte (Default 16kHz)"},
    {CV::ADDR_LONG_MSB, 192, "Long Addr MSB", "Upper byte of Long Address"},
    {CV::ADDR_LONG_LSB, 3, "Long Addr LSB", "Lower byte of Long Address"},
    {CV::CONFIG, 38, "Configuration", "Bit 5=LongAddr, Bit 2=Analog"},

    {CV::MASTER_VOL, 128, "Master Volume", "Audio Volume (0-255)"},

    // Virtual Cam Settings
    {CV::CHUFF_RATE, 10, "Chuff Rate", "Sync Multiplier (PWM -> RPM)"},
    {CV::CHUFF_DRAG, 5, "Chuff Load Drag", "Current -> RPM Drag Factor"},

    // Motor Control
    {CV::LOAD_GAIN, 15, "Load Gain", "Grade Comp Strength (0-255)."},
    {CV::BASELINE_ALPHA, 5, "Baseline Alpha", "Learning Speed (0-255)."},
    {CV::STICTION_KICK, 50, "Stiction Kick", "Start Pulse Strength (0-255)."},
    {CV::DELTA_CAP, 180, "Delta Cap", "Max Boost Limit (0-255)."},
    {CV::PWM_DITHER, 0, "PWM Dither", "Vibration for Brushes (0-255)."},
    {CV::BASELINE_RESET, 0, "Baseline Cmd",
     "1=Wipe, 2=Save Snapshot to Flash."},
    {CV::CURVE_INTENSITY, 0, "Curve Intensity",
     "Auto-generate S-Curve (0=Off, 1-255=Strength)."},

    // Advanced Motor Tuning
    {CV::DRIVE_MODE, 1, "Drive Mode", "0=Fast, 1=Slow Decay (Default 1)."},
    {CV::PEDESTAL_FLOOR, 80, "Pedestal Floor",
     "Absolute min PWM floor (0-255)."},
    {CV::LOAD_GAIN_SCALAR, 20, "Load Scalar", "Multiplier for CV60 (*10)."},
    {CV::LEARN_THRESHOLD, 20, "Learn Thresh",
     "Min speed to learn baseline (0-255)."},
    {CV::HARDWARE_GAIN, 1, "Hardware Gain",
     "0=Low, 1=High-Z (Med), 2=High."},

    // Audio Mapping (Examples for common IDs)
    {CV::AUDIO_MAP_BASE + 1, 0, "Map: Sound ID 1",
     "Function to trigger Sound 1 (0-28)"},
    {CV::AUDIO_MAP_BASE + 2, 0, "Map: Sound ID 2",
     "Function to trigger Sound 2 (0-28)"},
    {CV::AUDIO_MAP_BASE + 3, 0, "Map: Sound ID 3",
     "Function to trigger Sound 3 (0-28)"},
    {CV::AUDIO_MAP_BASE + 4, 0, "Map: Sound ID 4",
     "Function to trigger Sound 4 (0-28)"},
    {CV::AUDIO_MAP_BASE + 10, 0, "Map: Sound ID 10",
     "Function to trigger Sound 10 (0-28)"},
    {CV::AUDIO_MAP_BASE + 11, 0, "Map: Sound ID 11",
     "Function to trigger Sound 11 (0-28)"},

    {CV::FRONT, 0, "Map: Front Light", "Function to map to Front Light (0-28)"},
    {CV::REAR, 0, "Map: Rear Light", "Function to map to Rear Light (0-28)"},
    {CV::AUX1, 1, "Map: AUX 1", "Function to map to AUX 1 (0-28)"},
    {CV::AUX2, 2, "Map: AUX 2", "Function to map to AUX 2 (0-28)"},
    {CV::AUX3, 3, "Map: AUX 3", "Function to map to AUX 3 (0-28)"},
    {CV::AUX4, 4, "Map: AUX 4", "Function to map to AUX 4 (0-28)"},
    {CV::AUX5, 5, "Map: AUX 5", "Function to map to AUX 5 (0-28)"},
    {CV::AUX6, 6, "Map: AUX 6", "Function to map to AUX 6 (0-28)"},
    {CV::AUX7, 7, "Map: AUX 7", "Function to map to AUX 7 (0-28)"},
    {CV::AUX8, 8, "Map: AUX 8", "Function to map to AUX 8 (0-28)"}};

static const size_t CV_DEFS_COUNT = sizeof(CV_DEFS) / sizeof(CvDef);

#endif // CV_REGISTRY_H
