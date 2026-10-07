#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "globals.h"
#include "shared_ui.h"
#include "src/weather/weather.h"
#include "src/settings/settings.h"

TFT_eSPI tft = TFT_eSPI();
Screen currentScreen = HOME;
int brightness = 200;

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ============================================================
// Home screen
// ============================================================
void drawHomeScreen() {
  tft.fillScreen(UI_BG);
  currentScreen = HOME;

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

// ============================================================
// Home screen touch zones
// ============================================================
bool gearTouched(int16_t x, int16_t y) {
  return (x >= 270 && x <= 319 && y >= 0 && y <= 50);
}

// ============================================================
// Init
// ============================================================
void initDisplay() {
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);

  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(true);

  initBrightness();
}

// ============================================================
// Touch dispatch, per screen
// ============================================================
void handleTouch() {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    int x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    int y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);

    if (currentScreen == HOME) {
      if (gearTouched(x, y)) {
        drawSettingsScreen(brightness);
        delay(200);
        return;
      }
      if (homeWeatherCardTouched(x, y)) {
        drawWeatherScreen();
        delay(200);
        return;
      }
    }
    else if (currentScreen == SETTINGS) {
      if (backButtonTouched(x, y)) {
        drawHomeScreen();
        delay(200);
        return;
      }
      handleBrightnessTouch(x, y, brightness);
    }
    else if (currentScreen == WEATHER) {
      if (backButtonTouched(x, y)) {
        drawHomeScreen();
        delay(200);
        return;
      }
    }
  }
}
#endif
