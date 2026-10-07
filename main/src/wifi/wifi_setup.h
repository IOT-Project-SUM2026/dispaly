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

// Called from loop() - keeps the background setup portal running and
// retries the saved network while offline (the portal turns the WiFi
// station off, and the ESP32's own auto-reconnect can give up)
void handleWiFi() {
  wifiManager.process();

  static unsigned long offlineSince = 0;
  if (WiFi.status() == WL_CONNECTED) {
    offlineSince = 0;
    if (wifiManager.getConfigPortalActive()) {
      Serial.println("[WiFi] Connected - closing setup portal");
      wifiManager.stopConfigPortal();
    }
    return;
  }

  // after 1 min offline, open the setup portal so a new network can be chosen
  if (offlineSince == 0) offlineSince = millis();
  if (millis() - offlineSince > 60000 && !wifiManager.getConfigPortalActive()) {
    Serial.println("[WiFi] Offline for 1 min");
    wifiManager.startConfigPortal("SmartDisplay-Setup");
  }

  // retries make the portal unresponsive for a moment, so retry less often while it's open
  unsigned long retryInterval = wifiManager.getConfigPortalActive() ? 60000 : 15000;
  static unsigned long lastRetry = 0;
  if (millis() - lastRetry < retryInterval) return;
  lastRetry = millis();

  // don't disturb a phone that is using the setup portal
  if (WiFi.softAPgetStationNum() > 0 || !wifiManager.getWiFiIsSaved()) return;
  Serial.println("[WiFi] Retrying saved network");
  WiFi.begin();
}
#endif
