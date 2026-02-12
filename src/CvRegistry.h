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
static constexpr uint16_t ADDR_LONG_MSB = 17;
static constexpr uint16_t ADDR_LONG_LSB = 18;
static constexpr uint16_t CONFIG = 29;

// Audio
static constexpr uint16_t MASTER_VOL = 50;
static constexpr uint16_t AUDIO_MAP_BASE = 100; // CV = 100 + SoundID. Value = Function (0-28)

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
    {CV::V_START, 60, "Vstart", "Starting Voltage/Speed (0-255)"},
    {CV::ACCEL, 2, "Acceleration", "Momentum Delay (Rate)"},
    {CV::DECEL, 2, "Deceleration", "Momentum Delay (Rate)"},
    {CV::V_HIGH, 255, "Vhigh", "Max Voltage/Speed"},
    {CV::V_MID, 128, "Vmid", "Mid-range Speed Curve"},
    {CV::DECODER_VERSION, 10, "Version ID", "Read-only Version"},
    {CV::DECODER_MAN_ID, 13, "Manufacturer", "Read-only Man ID (DIY=13)"},
    {CV::ADDR_LONG_MSB, 192, "Long Addr MSB", "Upper byte of Long Address"},
    {CV::ADDR_LONG_LSB, 3, "Long Addr LSB", "Lower byte of Long Address"},
    {CV::CONFIG, 38, "Configuration", "Bit 5=LongAddr, Bit 2=Analog"},

    {CV::MASTER_VOL, 128, "Master Volume", "Audio Volume (0-255)"},

    // Audio Mapping (Examples for common IDs)
    {CV::AUDIO_MAP_BASE + 1, 0, "Map: Sound ID 1", "Function to trigger Sound 1 (0-28)"},
    {CV::AUDIO_MAP_BASE + 2, 0, "Map: Sound ID 2", "Function to trigger Sound 2 (0-28)"},
    {CV::AUDIO_MAP_BASE + 3, 0, "Map: Sound ID 3", "Function to trigger Sound 3 (0-28)"},
    {CV::AUDIO_MAP_BASE + 4, 0, "Map: Sound ID 4", "Function to trigger Sound 4 (0-28)"},
    {CV::AUDIO_MAP_BASE + 10, 0, "Map: Sound ID 10", "Function to trigger Sound 10 (0-28)"},
    {CV::AUDIO_MAP_BASE + 11, 0, "Map: Sound ID 11", "Function to trigger Sound 11 (0-28)"},

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