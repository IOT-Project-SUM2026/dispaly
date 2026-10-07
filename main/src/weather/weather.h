#ifndef WEATHER_H
#define WEATHER_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define WEATHER_LAT 32.79
#define WEATHER_LON 34.99
#define FORECAST_DAYS 4

struct ForecastDay {
  String day;
  uint8_t icon; // 0=sun, 1=cloud, 2=rain
  int high;
  int low;
};

int currentTemp = 24;
uint8_t currentIcon = 0;
String currentCondition = "Clear";
String location = "Haifa";
ForecastDay forecast[FORECAST_DAYS];
bool weatherLoaded = false;

// weather_code (Open-Meteo) -> simple icon type
uint8_t iconForCode(int code) {
  if (code == 0) return 0;        // sun
  if (code <= 3) return 1;        // cloud
  return 2;                       // rain/other
}

String descriptionForCode(int code) {
  if (code == 0) return "Clear";
  if (code <= 3) return "Cloudy";
  if (code <= 48) return "Fog";
  if (code <= 67) return "Rain";
  if (code <= 77) return "Snow";
  if (code <= 82) return "Showers";
  if (code <= 99) return "Storm";
  return "Unknown";
}

// Sakamoto's algorithm - day of week from a calendar date, no NTP needed
int dayOfWeek(int y, int m, int d) {
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7; // 0=Sunday
}

const char* weekdayNames[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Weather] Skipped - no WiFi");
    return;
  }

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(WEATHER_LAT) +
               "&longitude=" + String(WEATHER_LON) +
               "&current_weather=true"
               "&daily=weathercode,temperature_2m_max,temperature_2m_min"
               "&timezone=auto";

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      currentTemp = (int)round((float)doc["current_weather"]["temperature"]);
      int code = doc["current_weather"]["weathercode"];
      currentIcon = iconForCode(code);
      currentCondition = descriptionForCode(code);

      const char* labels[FORECAST_DAYS] = {"TODAY", "MON", "TUE", "WED"};
      for (int i = 0; i < FORECAST_DAYS; i++) {
        forecast[i].high = (int)round((float)doc["daily"]["temperature_2m_max"][i]);
        forecast[i].low = (int)round((float)doc["daily"]["temperature_2m_min"][i]);
        int dcode = doc["daily"]["weathercode"][i];
        forecast[i].icon = iconForCode(dcode);

        // derive the actual weekday from the API's own date, so it always
        // matches reality regardless of what day it's run
        String dateStr = doc["daily"]["time"][i].as<String>(); // "YYYY-MM-DD"
        int y = dateStr.substring(0, 4).toInt();
        int mo = dateStr.substring(5, 7).toInt();
        int da = dateStr.substring(8, 10).toInt();
        int wd = dayOfWeek(y, mo, da);
        forecast[i].day = (i == 0) ? "TODAY" : String(weekdayNames[wd]);
      }
      weatherLoaded = true;
      Serial.println("[Weather] Updated");
    } else {
      Serial.println("[Weather] JSON parse failed");
    }
  } else {
    Serial.print("[Weather] HTTP error: ");
    Serial.println(httpCode);
  }
  http.end();
}

// ============================================================
// Weather icon drawing (shared shape logic, different sizes)
// ============================================================
void drawWeatherIconShape(int cx, int cy, uint8_t type, uint16_t color, int scale) {
  if (type == 0) { // sun
    tft.drawCircle(cx, cy, 7 * scale / 10, color);
    for (int i = 0; i < 8; i++) {
      float a = i * 45.0f * DEG_TO_RAD;
      int x1 = cx + round(cos(a) * 11 * scale / 10);
      int y1 = cy + round(sin(a) * 11 * scale / 10);
      int x2 = cx + round(cos(a) * 14 * scale / 10);
      int y2 = cy + round(sin(a) * 14 * scale / 10);
      tft.drawLine(x1, y1, x2, y2, color);
    }
  } else if (type == 1) { // cloud
    tft.drawCircle(cx - 5, cy + 1, 6 * scale / 10, color);
    tft.drawCircle(cx + 3, cy - 2, 8 * scale / 10, color);
    tft.drawCircle(cx + 10, cy + 2, 5 * scale / 10, color);
    tft.drawRoundRect(cx - 10, cy + 1, 25 * scale / 10, 9 * scale / 10, 4, color);
  } else { // rain
    tft.drawCircle(cx - 4, cy - 2, 6 * scale / 10, color);
    tft.drawCircle(cx + 4, cy - 4, 7 * scale / 10, color);
    tft.drawRoundRect(cx - 10, cy - 2, 23 * scale / 10, 8 * scale / 10, 4, color);
    tft.drawLine(cx - 5, cy + 9, cx - 8, cy + 14, color);
    tft.drawLine(cx + 3, cy + 9, cx, cy + 14, color);
  }
}

// ============================================================
// Home weather card (tap to open the full Weather page)
// ============================================================
void drawHomeWeatherCard() {
  int x = WEATHER_CARD_X, y = WEATHER_CARD_Y, w = WEATHER_CARD_W, h = WEATHER_CARD_H;

  tft.fillRoundRect(x, y, w, h, 10, UI_SURFACE);
  tft.drawRoundRect(x, y, w, h, 10, UI_TRACK);

  if (!weatherLoaded) {
    tft.setTextFont(2);
    tft.setTextColor(UI_SECONDARY, UI_SURFACE);
    tft.setTextDatum(ML_DATUM);
    tft.drawString("Loading weather...", x + 16, y + h / 2);
    tft.setTextDatum(TL_DATUM);
    return;
  }

  drawWeatherIconShape(x + 24, y + 25, currentIcon, UI_ACCENT, 10);

  char temp[8];
  snprintf(temp, sizeof(temp), "%d\xB0", currentTemp);
  tft.setTextFont(4);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(UI_TEXT, UI_SURFACE);
  tft.drawString(temp, x + 48, y + 25);

  tft.setTextFont(2);
  tft.setTextColor(UI_SECONDARY, UI_SURFACE);
  tft.drawString(currentCondition, x + 92, y + 17);

  tft.setTextColor(UI_TEXT, UI_SURFACE);
  tft.drawString(location, x + 92, y + 34);

  tft.setTextFont(4);
  tft.setTextColor(UI_ACCENT, UI_SURFACE);
  tft.drawString(">", x + w - 17, y + 25);

  tft.setTextDatum(TL_DATUM);
}

bool homeWeatherCardTouched(int16_t x, int16_t y) {
  return (x >= WEATHER_CARD_X - 10 && x <= WEATHER_CARD_X + WEATHER_CARD_W + 10 &&
          y >= WEATHER_CARD_Y - 10 && y <= WEATHER_CARD_Y + WEATHER_CARD_H + 10);
}

// ============================================================
// Forecast day card (compact, part of the forecast row)
// ============================================================
#define FORECAST_GAP 8
#define FORECAST_CARD_W 64  // (280 total width - 3 gaps of 8) / 4 cards

void drawForecastCard(const ForecastDay& d, int x) {
  int y = 174, w = FORECAST_CARD_W, h = 55;
  tft.fillRoundRect(x, y, w, h, 6, UI_SURFACE);

  tft.setTextFont(1);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(UI_SECONDARY, UI_SURFACE);
  tft.drawString(d.day, x + w / 2, y + 6);

  drawWeatherIconShape(x + w / 2, y + 26, d.icon, UI_ACCENT, 10);

  char high[8], low[8];
  snprintf(high, sizeof(high), "%d\xB0", d.high);
  snprintf(low, sizeof(low), "%d\xB0", d.low);

  // centered within each half of the card, so text can't spill past the edges
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(UI_TEXT, UI_SURFACE);
  tft.drawString(high, x + (int)(w * 0.28), y + 44);

  tft.setTextColor(UI_SECONDARY, UI_SURFACE);
  tft.drawString(low, x + (int)(w * 0.72), y + 44);

  tft.setTextDatum(TL_DATUM);
}

// ============================================================
// Full Weather + Forecast screen
// ============================================================
void drawWeatherScreen() {
  tft.fillScreen(UI_BG);
  currentScreen = WEATHER;

  drawWiFiIcon(WIFI_ICON_X, WIFI_ICON_Y, WiFi.status() == WL_CONNECTED);
  drawBackButton();

  // header: title left, location right, same row
  tft.setTextFont(4);
  tft.setTextColor(UI_TEXT, UI_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Weather", 20, 80);

  tft.setTextFont(2);
  tft.setTextColor(UI_SECONDARY, UI_BG);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(location, 298, 90);

  tft.drawFastHLine(20, 108, 280, UI_TRACK);

  if (!weatherLoaded) {
    tft.setTextFont(2);
    tft.setTextColor(UI_SECONDARY, UI_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Loading...", 20, 130);
    return;
  }

  // compact current-weather card: icon, temp, condition, single row
  int cardX = 20, cardY = 115, cardW = 280, cardH = 40;
  tft.fillRoundRect(cardX, cardY, cardW, cardH, 8, UI_SURFACE);

  drawWeatherIconShape(cardX + 24, cardY + cardH / 2, currentIcon, UI_ACCENT, 10);

  char temp[8];
  snprintf(temp, sizeof(temp), "%d\xB0", currentTemp);
  tft.setTextFont(4);
  tft.setTextColor(UI_TEXT, UI_SURFACE);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(temp, cardX + 50, cardY + cardH / 2);

  tft.setTextFont(2);
  tft.setTextColor(UI_SECONDARY, UI_SURFACE);
  tft.drawString(currentCondition, cardX + 110, cardY + cardH / 2);

  // forecast label
  tft.setTextFont(1);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(UI_SECONDARY, UI_BG);
  tft.drawString("4-DAY FORECAST", 160, 163);

  int xs[FORECAST_DAYS];
  for (int i = 0; i < FORECAST_DAYS; i++) {
    xs[i] = 20 + i * (FORECAST_CARD_W + FORECAST_GAP);
  }
  for (int i = 0; i < FORECAST_DAYS; i++) {
    drawForecastCard(forecast[i], xs[i]);
  }

  tft.setTextDatum(TL_DATUM);
}

// Waits for WiFi; retries every 30 s until the first success, then every 10 min
void checkWeather() {
  static unsigned long lastFetch = 0;
  if (WiFi.status() != WL_CONNECTED) return;
  unsigned long now = millis();
  unsigned long interval = weatherLoaded ? 600000 : 30000;
  if (lastFetch == 0 || now - lastFetch > interval) {
    lastFetch = now;
    fetchWeather();
    if (currentScreen == HOME) drawHomeScreen();
    if (currentScreen == WEATHER) drawWeatherScreen();
  }
}
#endif
