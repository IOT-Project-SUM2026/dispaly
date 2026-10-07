#ifndef SETTINGS_H
#define SETTINGS_H
#include "../../globals.h"
#include "../../shared_ui.h"

#define BACKLIGHT_PIN 21
#define PWM_FREQ 5000
#define PWM_RES 8

TFT_eSprite brightnessSprite = TFT_eSprite(&tft);

void applyBrightness(int value) {
  ledcWrite(BACKLIGHT_PIN, value);
}

void initBrightness() {
  ledcAttach(BACKLIGHT_PIN, PWM_FREQ, PWM_RES);
  applyBrightness(brightness);
}

void drawBrightnessIcon() {
  int cx = 28, cy = SLIDER_Y;
  tft.drawCircle(cx, cy, 6, UI_TEXT);
  tft.drawLine(cx, cy - 11, cx, cy - 8, UI_TEXT);
  tft.drawLine(cx, cy + 8, cx, cy + 11, UI_TEXT);
  tft.drawLine(cx - 11, cy, cx - 8, cy, UI_TEXT);
  tft.drawLine(cx + 8, cy, cx + 11, cy, UI_TEXT);
  tft.drawLine(cx - 8, cy - 8, cx - 6, cy - 6, UI_TEXT);
  tft.drawLine(cx + 6, cy + 6, cx + 8, cy + 8, UI_TEXT);
  tft.drawLine(cx + 6, cy - 6, cx + 8, cy - 8, UI_TEXT);
  tft.drawLine(cx - 8, cy + 8, cx - 6, cy + 6, UI_TEXT);
}

void drawBrightnessSlider(int value) {
  value = constrain(value, 0, 255);

  if (!brightnessSprite.created()) {
    brightnessSprite.createSprite(SLIDER_W + 20, 50);
  }
  brightnessSprite.fillSprite(UI_BG);

  int trackX = 10, trackY = 21;
  brightnessSprite.fillRoundRect(trackX, trackY, SLIDER_W, SLIDER_H, 4, UI_TRACK);

  int filled = map(value, 0, 255, 0, SLIDER_W);
  if (filled > 0) {
    brightnessSprite.fillRoundRect(trackX, trackY, filled, SLIDER_H, 4, UI_ACCENT);
  }

  int thumbX = trackX + filled;
  brightnessSprite.fillCircle(thumbX, trackY + SLIDER_H / 2, 10, UI_ACCENT);
  brightnessSprite.fillCircle(thumbX, trackY + SLIDER_H / 2, 6, UI_TEXT);

  brightnessSprite.pushSprite(SLIDER_X - 10, SLIDER_Y - 25);
}

void drawBrightnessValue(int value) {
  int percent = map(constrain(value, 0, 255), 0, 255, 0, 100);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", percent);

  tft.fillRect(280, 85, 40, 28, UI_BG);
  tft.setTextDatum(MR_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(UI_TEXT, UI_BG);
  tft.drawString(buf, 310, 100);
  tft.setTextDatum(TL_DATUM);
}

void drawSettingsScreen(int brightnessValue) {
  tft.fillScreen(UI_BG);
  currentScreen = SETTINGS;

  drawWiFiIcon(WIFI_ICON_X, WIFI_ICON_Y, WiFi.status() == WL_CONNECTED);
  drawBackButton();

  tft.setTextFont(4);
  tft.setTextColor(UI_TEXT, UI_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Brightness", 20, 95);

  drawBrightnessValue(brightnessValue);
  drawBrightnessIcon();
  drawBrightnessSlider(brightnessValue);
}

int brightnessFromTouchX(int16_t x) {
  x = constrain(x, SLIDER_MIN_X, SLIDER_MAX_X);
  return map(x, SLIDER_MIN_X, SLIDER_MAX_X, 0, 255);
}

void handleBrightnessTouch(int16_t x, int16_t y, int &brightnessRef) {
  if (x < SLIDER_MIN_X - 10 || x > SLIDER_MAX_X + 10 ||
      y < SLIDER_Y - 25 || y > SLIDER_Y + 25) {
    return;
  }
  brightnessRef = brightnessFromTouchX(x);
  applyBrightness(brightnessRef);
  drawBrightnessSlider(brightnessRef);
  drawBrightnessValue(brightnessRef);

  // log at most every 300 ms so a drag doesn't flood the serial monitor
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 300) {
    lastLog = millis();
    Serial.printf("[Settings] Brightness %d%%\n", (int)map(brightnessRef, 0, 255, 0, 100));
  }
}
#endif
