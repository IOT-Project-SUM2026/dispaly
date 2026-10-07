#ifndef CONNECTION_STATUS_H
#define CONNECTION_STATUS_H
#include "../../globals.h"
#include <WiFi.h>

// ============================================================
// Polls the WiFi state every 2 s and updates the icon on the
// current screen when it changes (screens also redraw it on open).
// ============================================================
bool lastConnectionState = false;

void checkConnectionStatus() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 2000) return;
  lastCheck = millis();

  bool connected = (WiFi.status() == WL_CONNECTED);
  if (connected == lastConnectionState) return;
  lastConnectionState = connected;

  if (connected) {
    Serial.print("[WiFi] Connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] Disconnected");
  }

  tft.fillRect(WIFI_ICON_X - 14, WIFI_ICON_Y - 14, 28, 26, UI_BG);
  drawWiFiIcon(WIFI_ICON_X, WIFI_ICON_Y, connected);
}
#endif
