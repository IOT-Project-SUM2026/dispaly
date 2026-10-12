#ifndef WEATHER_H
#define WEATHER_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../storage/storage.h"
#include "../time_date/time_date.h"
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define FORECAST_DAYS 4

struct WeatherCity {
  const char* name;
  float lat;
  float lon;
};

#define WEATHER_CITY_COUNT 5
const WeatherCity weatherCities[WEATHER_CITY_COUNT] = {
  {"Jerusalem",  31.77, 35.21},
  {"Tel Aviv",   32.08, 34.78},
  {"Haifa",      32.79, 34.99},
  {"Beer Sheba", 31.25, 34.79},
  {"Eilat",      29.56, 34.95},
};
int selectedCity = 2;      // Haifa
bool cityChanged = false;  // set by the dropdown, handled in handleWeather()

// Load the saved city (call after initStorage)
void initWeather() {
  selectedCity = constrain(loadInt("city", 2), 0, WEATHER_CITY_COUNT - 1);
}

struct ForecastDay {
  String day;
  uint8_t icon; // WeatherIcon (shared_ui.h)
  int high;
  int low;
};

// What the screens show: the selected city (copied from cityWeather by showCityWeather())
int currentTemp = 24;
uint8_t currentIcon = 0;
String currentCondition = "Clear";
ForecastDay forecast[FORECAST_DAYS];
bool weatherLoaded = false;
time_t weatherUpdatedAt = 0;  // when the weather was last downloaded (0 = unknown)

// Every city is downloaded in ONE request and kept here, so switching
// cities is instant and needs no internet.
struct CityWeather {
  bool loaded;
  int temp;
  uint8_t icon;
  int code;
  ForecastDay days[FORECAST_DAYS];
};
CityWeather cityWeather[WEATHER_CITY_COUNT];

// weather_code (Open-Meteo, WMO codes) -> icon
uint8_t iconForCode(int code) {
  if (code == 0) return ICON_SUN;
  if (code <= 2) return ICON_PARTLY;
  if (code == 3) return ICON_CLOUD;
  if (code <= 48) return ICON_FOG;
  if (code <= 67 || (code >= 80 && code <= 82)) return ICON_RAIN;
  if (code <= 77 || code == 85 || code == 86) return ICON_SNOW;
  return ICON_STORM;
}

String descriptionForCode(int code) {
  if (code == 0) return "Clear";
  if (code <= 2) return "Partly cloudy";
  if (code == 3) return "Cloudy";
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

// Copies the selected city's saved weather into what the screens show
void showCityWeather() {
  const CityWeather& w = cityWeather[selectedCity];
  weatherLoaded = w.loaded;
  if (!w.loaded) return;
  currentTemp = w.temp;
  currentIcon = w.icon;
  currentCondition = descriptionForCode(w.code);
  for (int i = 0; i < FORECAST_DAYS; i++) forecast[i] = w.days[i];
}

// Reads one city from Open-Meteo's reply
static bool parseCityWeather(JsonObject src, CityWeather& w) {
  if (src["current_weather"].isNull() || src["daily"].isNull()) return false;
  w.temp = (int)round((float)src["current_weather"]["temperature"]);
  w.code = src["current_weather"]["weathercode"] | 0;
  w.icon = iconForCode(w.code);
  for (int i = 0; i < FORECAST_DAYS; i++) {
    ForecastDay& d = w.days[i];
    d.high = (int)round((float)src["daily"]["temperature_2m_max"][i]);
    d.low = (int)round((float)src["daily"]["temperature_2m_min"][i]);
    d.icon = iconForCode(src["daily"]["weathercode"][i] | 0);
    // weekday from the API's own date ("YYYY-MM-DD"), so it always matches reality
    const char* date = src["daily"]["time"][i] | "2000-01-01";
    int y = atoi(date), mo = atoi(date + 5), da = atoi(date + 8);
    d.day = (i == 0) ? "TODAY" : String(weekdayNames[dayOfWeek(y, mo, da)]);
  }
  w.loaded = true;
  return true;
}

// Downloads the weather of ALL cities in one request (one slow secure
// handshake instead of one per city). The connection is closed afterwards:
// the next request is 10 minutes later, so keeping it open would only hold memory.
// Returns true when new data was loaded.
bool fetchWeather() {
  static NetworkClientSecure client;
  static HTTPClient http;
  static bool clientReady = false;
  if (!clientReady) {
    client.setInsecure();           // encrypted, certificate not checked
    client.setHandshakeTimeout(8);  // seconds (default 120 would freeze the screen if WiFi drops)
    http.setReuse(false);           // close after each request (we only fetch every 10 min)
    clientReady = true;
  }

  // latitude=31.77,32.08,...&longitude=35.21,34.78,... -> one answer per city
  String lats, lons;
  for (int i = 0; i < WEATHER_CITY_COUNT; i++) {
    if (i > 0) { lats += ','; lons += ','; }
    lats += String(weatherCities[i].lat, 2);
    lons += String(weatherCities[i].lon, 2);
  }
  // HTTPS: plain HTTP is often held back by campus/office proxies
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + lats + "&longitude=" + lons +
               "&current_weather=true"
               "&daily=weathercode,temperature_2m_max,temperature_2m_min"
               "&forecast_days=" + String(FORECAST_DAYS) +
               "&timezone=Asia%2FJerusalem";

  Serial.println("[Weather] Fetching all cities...");
  unsigned long start = millis();
  if (!http.begin(client, url)) {
    Serial.println("[Weather] Could not start the request");
    return false;
  }
  http.setTimeout(8000);
  int httpCode = http.GET();
  String payload = httpCode == 200 ? http.getString() : String();
  http.end();

  if (httpCode != 200) {
    Serial.printf("[Weather] HTTP error: %d (after %lu ms)\n", httpCode, millis() - start);
    return false;
  }

  // keep only the fields we use: less memory, faster parsing
  JsonDocument filter;
  JsonObject f = filter.add<JsonObject>();
  f["current_weather"]["temperature"] = true;
  f["current_weather"]["weathercode"] = true;
  f["daily"] = true;
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error) {
    Serial.printf("[Weather] JSON parse failed: %s\n", error.c_str());
    return false;
  }

  // several cities -> a list; one city -> a single object
  int updatedCount = 0;
  if (doc.is<JsonArray>()) {
    JsonArray all = doc.as<JsonArray>();
    for (int i = 0; i < WEATHER_CITY_COUNT && i < (int)all.size(); i++) {
      if (parseCityWeather(all[i], cityWeather[i])) updatedCount++;
    }
  } else if (parseCityWeather(doc.as<JsonObject>(), cityWeather[0])) {
    updatedCount = 1;
  }
  if (updatedCount == 0) {
    Serial.println("[Weather] Reply had no weather data");
    return false;
  }
  showCityWeather();
  weatherUpdatedAt = timeSynced() ? time(NULL) : 0;
  Serial.printf("[Weather] Updated %d cities in %lu ms\n", updatedCount, millis() - start);
  return true;
}

// ============================================================
// Home weather card (top right of Home; tap: Weather screen)
// ============================================================
lv_obj_t* createHomeWeatherCard(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t onClick) {
  lv_obj_t* card = createCard(parent, x, y, w, h, onClick);

  if (!weatherLoaded) {
    bool online = WiFi.status() == WL_CONNECTED;
    lv_obj_t* title = createLabel(card, online ? "Loading..." : "No WiFi", online ? &stSecondary : &stDanger);
    lv_obj_set_pos(title, 12, 12);
    lv_obj_t* hint = createLabel(card, online ? "Weather" : "Join the WiFi network\nSmartDisplay-Setup", &stSmall);
    lv_obj_set_pos(hint, 12, 32);
    return card;
  }

  lv_obj_t* icon = createWeatherIcon(card, (WeatherIcon)currentIcon, 40);
  lv_obj_set_pos(icon, 10, (h - 40) / 2);
  lv_obj_t* temp = createLabel(card, (String(currentTemp) + "\xC2\xB0").c_str(), &stValue);
  lv_obj_set_pos(temp, 58, 9);
  lv_obj_t* place = createLabel(card, weatherCities[selectedCity].name, &stSecondary);
  makeSingleLine(place, w - 58 - 8);
  lv_obj_set_pos(place, 58, 42);
  return card;
}

// ============================================================
// Weather screen: city picker in the header, current weather,
// 4-day forecast. Only the content below the header is rebuilt
// when new data arrives (the city picker stays as it is).
// ============================================================
lv_obj_t* weatherContent = NULL;

void onWeatherContentDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) == weatherContent) weatherContent = NULL;
}

// "Updated 11:20"
static void weatherUpdatedText(char* out, size_t size) {
  if (weatherUpdatedAt == 0) { out[0] = '\0'; return; }
  struct tm t;
  localtime_r(&weatherUpdatedAt, &t);
  char timeBuf[8], ampmBuf[4];
  formatClock(t, timeBuf, sizeof(timeBuf), ampmBuf, sizeof(ampmBuf));
  snprintf(out, size, "Updated %s%s%s", timeBuf, ampmBuf[0] ? " " : "", ampmBuf);
}

void createForecastCard(lv_obj_t* parent, const ForecastDay& d, int x, int y, int w, int h) {
  lv_obj_t* card = createCard(parent, x, y, w, h, NULL);
  lv_obj_t* day = createCaption(card, d.day.c_str());
  lv_obj_align(day, LV_ALIGN_TOP_MID, 0, 6);
  lv_obj_t* icon = createWeatherIcon(card, (WeatherIcon)d.icon, 24);
  lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 23);

  // high (20 px) and low (14 px, grey) side by side, bottoms lined up
  lv_obj_t* temps = lv_obj_create(card);
  lv_obj_remove_style_all(temps);
  lv_obj_set_size(temps, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(temps, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(temps, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(temps, 4, 0);
  passClicks(temps);
  createLabel(temps, (String(d.high) + "\xC2\xB0").c_str(), &stTitle);
  lv_obj_t* low = createLabel(temps, (String(d.low) + "\xC2\xB0").c_str(), &stSecondary);
  lv_obj_set_style_pad_bottom(low, 2, 0);
  lv_obj_align(temps, LV_ALIGN_TOP_MID, 0, 50);
}

void buildWeatherContent(lv_obj_t* scr) {
  if (weatherContent) lv_obj_delete(weatherContent);
  weatherContent = createBox(scr, 0, CONTENT_Y, SCREEN_WIDTH, SCREEN_HEIGHT - CONTENT_Y);
  lv_obj_add_event_cb(weatherContent, onWeatherContentDeleted, LV_EVENT_DELETE, NULL);
  lv_obj_move_to_index(weatherContent, 0);  // behind the header, so the open city list stays on top

  if (!weatherLoaded) {
    if (WiFi.status() == WL_CONNECTED) createMessage(weatherContent, NULL, "Loading weather...", NULL, 110);
    else createMessage(weatherContent, LV_SYMBOL_WARNING, "No WiFi", "The weather loads when WiFi is back", 110);
    return;
  }

  // "Updated 10:30" next to the city button (red when offline: the data is old)
  char text[32];
  weatherUpdatedText(text, sizeof(text));
  lv_obj_t* updated = createLabel(weatherContent, text, WiFi.status() == WL_CONNECTED ? &stSmall : &stDanger);
  lv_obj_align(updated, LV_ALIGN_TOP_RIGHT, -MARGIN, 15);

  // current weather: icon, big temperature, condition and high / low
  lv_obj_t* icon = createWeatherIcon(weatherContent, (WeatherIcon)currentIcon, 48);
  lv_obj_set_pos(icon, MARGIN + 2, 48);
  lv_obj_t* temp = createLabel(weatherContent, (String(currentTemp) + "\xC2\xB0").c_str(), &stDisplay);
  lv_obj_set_pos(temp, 74, 53);
  lv_obj_t* condition = createLabel(weatherContent, currentCondition.c_str(), &stTitle);
  makeSingleLine(condition, SCREEN_WIDTH - MARGIN - 168);
  lv_obj_set_pos(condition, 168, 50);
  snprintf(text, sizeof(text), "H %d\xC2\xB0  L %d\xC2\xB0", forecast[0].high, forecast[0].low);
  lv_obj_t* highLow = createLabel(weatherContent, text, &stSecondary);
  lv_obj_set_pos(highLow, 168, 76);

  // 4-day forecast
  const int w = (SCREEN_WIDTH - 2 * MARGIN - 3 * GAP) / 4;  // 68
  for (int i = 0; i < FORECAST_DAYS; i++) {
    createForecastCard(weatherContent, forecast[i], MARGIN + i * (w + GAP), 110, w, 76);
  }
}

// A city was picked in the city button's list
void onCityChanged(lv_event_t* e) {
  int city = lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e));
  if (city == selectedCity) return;
  selectedCity = city;
  showCityWeather();  // already downloaded: shows instantly, no request
  cityChanged = true;
  Serial.printf("[Weather] City -> %s\n", weatherCities[city].name);
  saveInt("city", city);
}

// City button: a filled, outlined pill with a location pin and a down
// arrow, so it reads as "tap to change". Its list opens below it.
// On the screen itself (not in the weather content), so it stays open
// when new weather arrives.
lv_obj_t* createCityPicker(lv_obj_t* parent) {
  static char options[64] = "";
  if (options[0] == '\0') {
    for (int i = 0; i < WEATHER_CITY_COUNT; i++) {
      if (i > 0) strcat(options, "\n");
      strcat(options, weatherCities[i].name);
    }
  }
  lv_obj_t* dd = createDropdown(parent, options, selectedCity, onCityChanged);
  lv_obj_add_style(dd, &stCityButton, 0);
  lv_obj_set_pos(dd, MARGIN, CONTENT_Y + 4);
  lv_obj_set_size(dd, 150, 36);
  lv_obj_set_style_pad_left(dd, 34, 0);  // room for the pin
  lv_obj_set_ext_click_area(dd, 4);      // 44 px tall tap area
  lv_dropdown_set_symbol(dd, LV_SYMBOL_DOWN);

  lv_obj_t* pin = createLabel(dd, LV_SYMBOL_GPS, &stAccent);
  lv_obj_align(pin, LV_ALIGN_LEFT_MID, -22, 0);  // inside the left padding
  return dd;
}

lv_obj_t* buildWeatherScreen(lv_event_cb_t onBack) {
  lv_obj_t* scr = createScreen();
  createHeader(scr, "Weather", onBack);
  createCityPicker(scr);
  buildWeatherContent(scr);
  return scr;
}

// Fetches all cities every 10 min (every 30 s until the first success).
// A failed fetch is retried once right away, then waits for the interval.
// Returns true when the screen should be redrawn: new data arrived,
// or the city changed (its weather is already saved, so no request).
bool handleWeather() {
  static unsigned long lastAttempt = 0;
  static bool firstAttempt = true;
  static bool retried = false;
  if (cityChanged) {
    cityChanged = false;
    return true;  // redraw with the saved weather of the new city
  }
  unsigned long interval = weatherLoaded ? 600000 : 30000;
  if (!firstAttempt && millis() - lastAttempt < interval) return false;
  firstAttempt = false;
  lastAttempt = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Weather] Skipped - no WiFi");
    return false;
  }

  bool ok = fetchWeather();
  if (!ok && !retried) {
    retried = true;
    firstAttempt = true;  // one immediate retry on the next pass
    Serial.println("[Weather] Retrying once");
  } else {
    retried = false;
  }
  return ok;
}
#endif
