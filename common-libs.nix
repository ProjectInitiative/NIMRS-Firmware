{ pkgsWithArduino }:
with pkgsWithArduino.arduinoLibraries;
[
  NmraDcc."2.0.17"
  WiFiManager."2.0.17"
  ArduinoJson."7.3.0"
  ESP8266Audio."1.9.7"
]
