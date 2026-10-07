#ifndef CONNECTION_STATUS_H
#define CONNECTION_STATUS_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include <WiFi.h>

// ============================================================
// Polls the WiFi state every 2 s and updates the icon on the
// current screen when it changes (screens also set it on open).
// ============================================================
bool lastConnectionState = false;

void handleConnectionStatus() {
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

  updateWiFiIcon(connected);
}
#endif
