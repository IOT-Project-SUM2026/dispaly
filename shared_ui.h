#ifndef SHARED_UI_H
#define SHARED_UI_H
#include "globals.h"
#include <WiFi.h>

// ============================================================
// WiFi icon - called at the top of EVERY screen's draw function,
// so it's always correct and never left over from a screen wipe.
// (This replaces the old connection_status.h polling approach.)
// ============================================================
void drawWiFiIcon(int cx, int cy, bool connected) {
  uint16_t color = connected ? UI_TEXT : UI_OFFLINE;

  tft.drawArc(cx, cy, 12, 9, 220 - 90, 320 - 90, color, color);
  tft.drawArc(cx, cy, 8, 6, 220 - 90, 320 - 90, color, color);
  tft.drawArc(cx, cy, 4, 3, 220 - 90, 320 - 90, color, color);
  tft.fillCircle(cx, cy + 7, 2, color);

  if (!connected) {
    tft.drawLine(cx + 5, cy - 9, cx + 11, cy - 3, UI_OFFLINE);
    tft.drawLine(cx + 11, cy - 9, cx + 5, cy - 3, UI_OFFLINE);
  }
}

// ============================================================
// Gear icon (top-right, opens Settings)
// ============================================================
void drawGearIcon() {
  tft.fillCircle(GEAR_X, GEAR_Y, GEAR_R, UI_TEXT);
  for (int i = 0; i < 8; i++) {
    float angle = i * 45.0f * DEG_TO_RAD;
    int x = GEAR_X + round(cos(angle) * (GEAR_R + 1));
    int y = GEAR_Y + round(sin(angle) * (GEAR_R + 1));
    tft.fillRect(x - 2, y - 2, 4, 4, UI_TEXT);
  }
  tft.fillCircle(GEAR_X, GEAR_Y, GEAR_R / 2, UI_BG);
}

// ============================================================
// Back button (circular, used on Settings + Weather screens)
// ============================================================
void drawBackButton() {
  tft.fillCircle(BACK_CX, BACK_CY, BACK_R, UI_SURFACE);
  int wingX = BACK_CX + 5;
  int tipX = BACK_CX - 5;
  tft.drawLine(wingX, BACK_CY - 7, tipX, BACK_CY, UI_TEXT);
  tft.drawLine(tipX, BACK_CY, wingX, BACK_CY + 7, UI_TEXT);
  tft.drawLine(wingX + 1, BACK_CY - 7, tipX + 1, BACK_CY, UI_TEXT);
  tft.drawLine(tipX + 1, BACK_CY, wingX + 1, BACK_CY + 7, UI_TEXT);
}

// Shared touch-zone check for the back button, used by Settings + Weather
bool backButtonTouched(int16_t x, int16_t y) {
  return (x >= 0 && x <= BACK_CX + BACK_R + 15 &&
          y >= BACK_CY - BACK_R - 15 && y <= BACK_CY + BACK_R + 15);
}
#endif
