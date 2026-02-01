/*
 * NIMRS-21Pin-Decoder Firmware
 * Platform: ESP32-S3
 */

#include "config.h"
#include "NmraDcc.h"

// Core Connectivity
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <DNSServer.h>

// Vendored Libraries
#include <NmraDcc.h>
#include <WiFiManager.h>

NmraDcc Dcc;
WiFiManager wifiManager;

// Telnet Debugging
WiFiServer telnetServer(23);
WiFiClient telnetClient;

void handleTelnet() {
  if (telnetServer.hasClient()) {
    if (!telnetClient || !telnetClient.connected()) {
      if (telnetClient) telnetClient.stop();
      telnetClient = telnetServer.available();
      telnetClient.println("Connected to NIMRS Telnet Debug");
    } else {
      // Reject new connection if one is active
      telnetServer.available().stop();
    }
  }
}

void debugPrint(String msg) {
  Serial.print(msg);
  if (telnetClient && telnetClient.connected()) {
    telnetClient.print(msg);
  }
}

void debugPrintln(String msg) {
  Serial.println(msg);
  if (telnetClient && telnetClient.connected()) {
    telnetClient.println(msg);
  }
}

void setupWiFi() {
  debugPrintln("Initializing WiFi...");
  
  // Custom IP for portal
  // wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  // Set Hostname
  #ifdef HOSTNAME
  WiFi.setHostname(HOSTNAME);
  #endif
  
  // AutoConnect
  if (!wifiManager.autoConnect("NIMRS-Setup")) {
    debugPrintln("failed to connect and hit timeout");
    delay(3000);
    ESP.restart(); // reset and try again
  }

  debugPrintln("WiFi Connected!");
  debugPrint("IP Address: ");
  debugPrintln(WiFi.localIP().toString());
  
  // Start Telnet Server
  telnetServer.begin();
  telnetServer.setNoDelay(true);
}

void setupOTA() {
  // Hostname defaults to esp3232-[MAC]
  #ifdef HOSTNAME
  ArduinoOTA.setHostname(HOSTNAME);
  #endif

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    debugPrintln("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    debugPrintln("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // Avoid spamming telnet/serial with progress
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  debugPrintln("OTA Ready");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000); // Wait for serial
  
  Serial.println("NIMRS Decoder Booting...");

  // Setup WiFi & OTA
  setupWiFi();
  setupOTA();
  
  // Configure DCC Pin (Must be interrupt capable)
  #ifdef DCC_PIN
  Dcc.pin(0, DCC_PIN, 1);
  Dcc.init(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE, 0);
  Serial.printf("DCC Initialized on Pin %d\n", DCC_PIN);
  if (telnetClient) telnetClient.printf("DCC Initialized on Pin %d\n", DCC_PIN);
  #else
  debugPrintln("Error: DCC_PIN not defined in config.h");
  #endif

  #ifdef STATUS_LED_PIN
  pinMode(STATUS_LED_PIN, OUTPUT);
  #endif
}

void loop() {
  ArduinoOTA.handle();
  handleTelnet();
  Dcc.process();
  
  static unsigned long lastLoop = 0;
  if (millis() - lastLoop > 1000) {
    lastLoop = millis();
    // Heartbeat
    #ifdef STATUS_LED_PIN
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    #endif
    // Heartbeat message
    debugPrintln("NIMRS Alive (Telnet+Serial)");
  }
}
