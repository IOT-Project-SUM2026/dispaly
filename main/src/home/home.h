#ifndef HOME_H
#define HOME_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../weather/weather.h"

// ============================================================
// Home screen
// ============================================================
void drawHomeScreen() {
  tft.fillScreen(UI_BG);

  drawWiFiIcon(WIFI_ICON_X, WIFI_ICON_Y, WiFi.status() == WL_CONNECTED);
  drawGearIcon();

  const char* smart = "Smart";
  const char* disp = " Display";

  tft.setTextFont(4);
  int smartW = tft.textWidth(smart);
  int dispW = tft.textWidth(disp);
  int startX = 160 - (smartW + dispW) / 2;

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(UI_TEXT, UI_BG);
  tft.drawString(smart, startX, 112);
  tft.setTextColor(UI_ACCENT, UI_BG);
  tft.drawString(disp, startX + smartW, 112);

  tft.fillRoundRect(151, 133, 58, 4, 2, UI_ACCENT);

  drawHomeWeatherCard();

  tft.setTextDatum(TL_DATUM);
}
#endif
