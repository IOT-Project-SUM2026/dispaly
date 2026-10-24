#ifndef CONNECTION_STATUS_H
#define CONNECTION_STATUS_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include <WiFi.h>
#include "../net/net.h"

// Polls the connection every 2 s and updates the icon on the current screen
// when it changes (screens also set it on open): WiFi icon, "Offline" or
// "No internet" (net.h).
ConnectionState lastConnectionState = CONN_OFFLINE;
bool lastWiFiConnected = false;

void handleConnectionStatus() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 2000) return;
  lastCheck = millis();

  bool wifi = (WiFi.status() == WL_CONNECTED);
  if (wifi != lastWiFiConnected) {
    lastWiFiConnected = wifi;
    if (wifi) {
      Serial.print("[WiFi] Connected, IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("[WiFi] Disconnected");
    }
  }

  ConnectionState state = connectionState();
  if (state == lastConnectionState) return;
  if (state == CONN_NO_INTERNET) Serial.println("[WiFi] Connected, but no internet (no server answered for 20 s)");
  else if (lastConnectionState == CONN_NO_INTERNET && state == CONN_OK) Serial.println("[WiFi] Internet is back");
  lastConnectionState = state;
  updateWiFiIcon(state);
}
#endif
