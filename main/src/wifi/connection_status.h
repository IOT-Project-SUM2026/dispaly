#ifndef CONNECTION_STATUS_H
#define CONNECTION_STATUS_H
#include "../../globals.h"
#include <WiFi.h>

#define WIFI_ICON_X 20
#define WIFI_ICON_Y 20

bool lastConnectionState = true;
bool firstDraw = true;


void drawWifiConnectedIcon(int x, int y) {
  tft.fillCircle(x, y + 8, 2, TFT_GREEN);
  tft.drawArc(x, y + 8, 5, 4, 135, 225, TFT_GREEN, TFT_WHITE, true);
  tft.drawArc(x, y + 8, 9, 8, 135, 225, TFT_GREEN, TFT_WHITE, true);
  tft.drawArc(x, y + 8, 13, 12, 135, 225, TFT_GREEN, TFT_WHITE, true);
}

void drawWifiDisconnectedIcon(int x, int y) {
  tft.fillCircle(x, y + 8, 2, TFT_LIGHTGREY);
  tft.drawArc(x, y + 8, 9, 8, 135, 225, TFT_LIGHTGREY, TFT_WHITE, true);
  tft.drawLine(x - 8, y - 2, x + 8, y + 14, TFT_RED);
  tft.drawLine(x + 8, y - 2, x - 8, y + 14, TFT_RED);
  tft.drawLine(x - 7, y - 2, x + 9, y + 14, TFT_RED);
  tft.drawLine(x + 9, y - 2, x - 7, y + 14, TFT_RED);
}

void drawConnectionStatus() {
  bool connected = (WiFi.status() == WL_CONNECTED);
  if (!firstDraw && connected == lastConnectionState) return;

  tft.fillRect(WIFI_ICON_X - 15, WIFI_ICON_Y - 5, 40, 25, TFT_WHITE);

  if (connected) {
    drawWifiConnectedIcon(WIFI_ICON_X, WIFI_ICON_Y);
  } else {
    drawWifiDisconnectedIcon(WIFI_ICON_X, WIFI_ICON_Y);
  }

  lastConnectionState = connected;
  firstDraw = false;
}


void forceRedrawConnectionStatus() {
  firstDraw = true;
  drawConnectionStatus();
}

void checkConnectionStatus() {
  static unsigned long lastCheck = 0;
  unsigned long now = millis();
  if (now - lastCheck > 2000) {
    lastCheck = now;
    drawConnectionStatus();
  }
}
#endif