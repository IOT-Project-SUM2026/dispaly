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

// Returns true when new data was loaded
bool fetchWeather() {
  bool updated = false;
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
      updated = true;
      Serial.println("[Weather] Updated");
    } else {
      Serial.println("[Weather] JSON parse failed");
    }
  } else {
    Serial.print("[Weather] HTTP error: ");
    Serial.println(httpCode);
  }
  http.end();
  return updated;
}

// ============================================================
// Weather icon (sun / cloud / rain) built from simple shapes
// ============================================================
lv_obj_t* createWeatherIcon(lv_obj_t* parent, uint8_t type, int s, lv_color_t color) {
  if (type == 0) return createSunIcon(parent, s, color);

  lv_obj_t* box = createShape(parent, 0, 0, s, s, 0, color);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  int top = (type == 2) ? 0 : s / 8;               // rain: cloud higher, drops below
  createShape(box, s / 8, top + s / 4, s / 2, s / 2, LV_RADIUS_CIRCLE, color);
  createShape(box, s * 7 / 20, top + s / 10, s * 3 / 5, s * 3 / 5, LV_RADIUS_CIRCLE, color);
  createShape(box, 0, top + s * 9 / 20, s, s * 3 / 10, s * 3 / 20, color);
  if (type == 2) {
    for (int i = 1; i <= 3; i++) {
      createShape(box, s * i / 4 - s / 24, s * 4 / 5, s / 12 + 1, s / 5, 2, color);
    }
  }
  return box;
}

// ============================================================
// Home weather card (tap to open the full Weather page)
// ============================================================
void createHomeWeatherCard(lv_obj_t* parent, lv_event_cb_t onClick) {
  lv_obj_t* card = createCard(parent, WEATHER_CARD_X, WEATHER_CARD_Y, WEATHER_CARD_W, WEATHER_CARD_H, 10, onClick);

  if (!weatherLoaded) {
    lv_obj_t* loading = createLabel(card, "Loading weather...", &lv_font_montserrat_14, UI_SECONDARY);
    lv_obj_align(loading, LV_ALIGN_LEFT_MID, 16, 0);
    return;
  }

  lv_obj_t* icon = createWeatherIcon(card, currentIcon, 28, UI_ACCENT);
  lv_obj_align(icon, LV_ALIGN_LEFT_MID, 10, 0);

  lv_obj_t* temp = createLabel(card, (String(currentTemp) + "°").c_str(), &lv_font_montserrat_28, UI_TEXT);
  lv_obj_align(temp, LV_ALIGN_LEFT_MID, 48, 0);

  lv_obj_t* condition = createLabel(card, currentCondition.c_str(), &lv_font_montserrat_14, UI_SECONDARY);
  lv_obj_align(condition, LV_ALIGN_LEFT_MID, 110, -9);
  lv_obj_t* place = createLabel(card, location.c_str(), &lv_font_montserrat_14, UI_TEXT);
  lv_obj_align(place, LV_ALIGN_LEFT_MID, 110, 9);

  lv_obj_t* arrow = createLabel(card, LV_SYMBOL_RIGHT, &lv_font_montserrat_14, UI_ACCENT);
  lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -12, 0);
}

// ============================================================
// Forecast day card (compact, part of the forecast row)
// ============================================================
#define FORECAST_GAP 8
#define FORECAST_CARD_W 64  // (280 total width - 3 gaps of 8) / 4 cards

void createForecastCard(lv_obj_t* parent, const ForecastDay& d, int x) {
  lv_obj_t* card = createCard(parent, x, 174, FORECAST_CARD_W, 55, 6, NULL);
  lv_obj_set_style_border_width(card, 0, 0);

  lv_obj_t* day = createLabel(card, d.day.c_str(), &lv_font_montserrat_12, UI_SECONDARY);
  lv_obj_align(day, LV_ALIGN_TOP_MID, 0, 3);

  lv_obj_t* icon = createWeatherIcon(card, d.icon, 16, UI_ACCENT);
  lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 19);

  lv_obj_t* high = createLabel(card, (String(d.high) + "°").c_str(), &lv_font_montserrat_12, UI_TEXT);
  lv_obj_align(high, LV_ALIGN_BOTTOM_MID, -14, -3);
  lv_obj_t* low = createLabel(card, (String(d.low) + "°").c_str(), &lv_font_montserrat_12, UI_SECONDARY);
  lv_obj_align(low, LV_ALIGN_BOTTOM_MID, 14, -3);
}

// ============================================================
// Full Weather + Forecast screen
// ============================================================
lv_obj_t* buildWeatherScreen(lv_event_cb_t onBack) {
  lv_obj_t* scr = createScreen();

  createWiFiIcon(scr);
  createBackButton(scr, onBack);

  // header: title left, location right, same row
  lv_obj_t* title = createLabel(scr, "Weather", &lv_font_montserrat_28, UI_TEXT);
  lv_obj_set_pos(title, 20, 76);
  lv_obj_t* place = createLabel(scr, location.c_str(), &lv_font_montserrat_14, UI_SECONDARY);
  lv_obj_align(place, LV_ALIGN_TOP_RIGHT, -22, 88);

  createShape(scr, 20, 108, 280, 1, 0, UI_TRACK);

  if (!weatherLoaded) {
    lv_obj_t* loading = createLabel(scr, "Loading...", &lv_font_montserrat_14, UI_SECONDARY);
    lv_obj_set_pos(loading, 20, 130);
    return scr;
  }

  // compact current-weather card: icon, temp, condition, single row
  lv_obj_t* card = createCard(scr, 20, 115, 280, 40, 8, NULL);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_t* icon = createWeatherIcon(card, currentIcon, 24, UI_ACCENT);
  lv_obj_align(icon, LV_ALIGN_LEFT_MID, 12, 0);
  lv_obj_t* temp = createLabel(card, (String(currentTemp) + "°").c_str(), &lv_font_montserrat_20, UI_TEXT);
  lv_obj_align(temp, LV_ALIGN_LEFT_MID, 50, 0);
  lv_obj_t* condition = createLabel(card, currentCondition.c_str(), &lv_font_montserrat_14, UI_SECONDARY);
  lv_obj_align(condition, LV_ALIGN_LEFT_MID, 110, 0);

  // forecast label + row
  lv_obj_t* label = createLabel(scr, "4-DAY FORECAST", &lv_font_montserrat_12, UI_SECONDARY);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 158);

  for (int i = 0; i < FORECAST_DAYS; i++) {
    createForecastCard(scr, forecast[i], 20 + i * (FORECAST_CARD_W + FORECAST_GAP));
  }
  return scr;
}

// Fetches every 30 s until the first success, then every 10 min.
// Returns true when new data arrived (the caller redraws the screen).
bool handleWeather() {
  static unsigned long lastAttempt = 0;
  static bool firstAttempt = true;
  unsigned long interval = weatherLoaded ? 600000 : 30000;
  if (!firstAttempt && millis() - lastAttempt < interval) return false;
  firstAttempt = false;
  lastAttempt = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Weather] Skipped - no WiFi");
    return false;
  }
  return fetchWeather();
}
#endif
