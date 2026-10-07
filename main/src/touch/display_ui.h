#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../home/home.h"
#include "../weather/weather.h"
#include "../settings/settings.h"

TFT_eSPI tft = TFT_eSPI();
int brightness = 200;

// The open screen - only changed by handleTouch(). setup() draws HOME first.
enum Screen { HOME, SETTINGS, WEATHER };
Screen currentScreen = HOME;

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// ============================================================
// Touch zones
// ============================================================
bool gearTouched(int16_t x, int16_t y) {
  return (x >= 270 && x <= 319 && y >= 0 && y <= 50);
}

bool homeWeatherCardTouched(int16_t x, int16_t y) {
  return (x >= WEATHER_CARD_X - 10 && x <= WEATHER_CARD_X + WEATHER_CARD_W + 10 &&
          y >= WEATHER_CARD_Y - 10 && y <= WEATHER_CARD_Y + WEATHER_CARD_H + 10);
}

// Shared by Settings + Weather
bool backButtonTouched(int16_t x, int16_t y) {
  return (x >= 0 && x <= BACK_CX + BACK_R + 15 &&
          y >= BACK_CY - BACK_R - 15 && y <= BACK_CY + BACK_R + 15);
}

bool sliderTouched(int16_t x, int16_t y) {
  return (x >= SLIDER_MIN_X - 10 && x <= SLIDER_MAX_X + 10 &&
          y >= SLIDER_Y - 25 && y <= SLIDER_Y + 25);
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
// Redraw the open screen (e.g. after new weather data)
// ============================================================
void refreshScreen() {
  if (currentScreen == HOME) drawHomeScreen();
  if (currentScreen == WEATHER) drawWeatherScreen();
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
        Serial.println("[UI] Home -> Settings");
        currentScreen = SETTINGS;
        drawSettingsScreen(brightness);
        delay(200);
        return;
      }
      if (homeWeatherCardTouched(x, y)) {
        Serial.println("[UI] Home -> Weather");
        currentScreen = WEATHER;
        drawWeatherScreen();
        delay(200);
        return;
      }
    }
    else if (currentScreen == SETTINGS) {
      if (backButtonTouched(x, y)) {
        Serial.println("[UI] Settings -> Home");
        currentScreen = HOME;
        drawHomeScreen();
        delay(200);
        return;
      }
      if (sliderTouched(x, y)) {
        setBrightnessFromTouch(x, brightness);
      }
    }
    else if (currentScreen == WEATHER) {
      if (backButtonTouched(x, y)) {
        Serial.println("[UI] Weather -> Home");
        currentScreen = HOME;
        drawHomeScreen();
        delay(200);
        return;
      }
    }
  }
}
#endif
