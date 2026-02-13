#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "DccController.h"
#include "SystemContext.h"
#include "nimrs-pinout.h"
#include <Arduino.h>

class MotorController {
public:
  MotorController();
  void setup();
  void loop();

  // Configuration
  void setGain(bool high);

private:
  const uint8_t _gainPin = 34;

  // PWM Configuration (LEDC)
  // 100Hz + Slow Decay = High Torque Crawl
  const uint32_t _pwmFreq = 100; 
  const uint8_t _pwmResolution = 8;
  const uint8_t _pwmChannel1 = 2; // Avoid 0/1 to minimize Audio conflict risk
  const uint8_t _pwmChannel2 = 3;

  // Momentum & Direction State
  float _currentSpeed = 0.0f;
  bool _currentDirection = true; // Tracks physical bridge state
  unsigned long _lastMomentumUpdate = 0;

  // CV Cache
  uint8_t _cvAccel = 0;
  uint8_t _cvDecel = 0;
  uint8_t _cvVstart = 0;
  unsigned long _lastCvUpdate = 0;

  // Kick Start State
  bool _kickActive = false;
  unsigned long _kickStartTime = 0;
  static const uint8_t KICK_STRENGTH = 180; 
  static const uint16_t KICK_DURATION = 80; 

  void _drive(uint8_t speed, bool direction);
  void _updateCvCache();
};

#endif
