/*
 * NIMRS-21Pin-Decoder Firmware
 * Platform: ESP32-S3
 */

#include "config.h"
#include "NmraDcc.h"

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000); // Wait for serial
  
  Serial.println("NIMRS Decoder Booting...");
  
  #ifdef STATUS_LED_PIN
  pinMode(STATUS_LED_PIN, OUTPUT);
  #endif
}

void loop() {
  Serial.println("NIMRS Alive");
  
  #ifdef STATUS_LED_PIN
  digitalWrite(STATUS_LED_PIN, HIGH);
  delay(100);
  digitalWrite(STATUS_LED_PIN, LOW);
  #endif
  
  delay(1000);
}
