#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H
#include <WiFi.h>
#include <FS.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>

WiFiManager wifiManager;

// Non-blocking: if WiFi isn't available, the setup portal runs in the
// background (see handleWiFi) and the display keeps working offline.
void connectWiFi() {
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConnectTimeout(10);
  wifiManager.setAPCallback([](WiFiManager*) {
    Serial.println("[WiFi] Not connected - setup portal \"SmartDisplay-Setup\" is open");
  });

  if (wifiManager.autoConnect("SmartDisplay-Setup")) {
    Serial.print("[WiFi] Connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] Continuing offline");
  }
}

// Called from loop() - keeps the background setup portal running
void handleWiFi() {
  wifiManager.process();
}
#endif
