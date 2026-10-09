#ifndef TIME_DATE_H
#define TIME_DATE_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../storage/storage.h"
#include <WiFi.h>
#include <time.h>

// Israel time zone with automatic daylight saving (POSIX TZ rule)
#define LOCAL_TZ "IST-2IDT,M3.4.4/26,M10.5.0"

bool use24h = true;
bool ntpStarted = false;

// Load the saved time format (call after initStorage)
void initTimeDate() {
  use24h = loadInt("use24h", 1) != 0;
}

// The ESP32 clock starts at 1970 - anything after 2023 means NTP has synced
bool timeSynced() {
  return time(NULL) > 1700000000;
}

// "14:32" (24h) or "2:32" + "PM" (12h)
void formatClock(const struct tm& t, char* timeBuf, size_t timeLen, char* ampmBuf, size_t ampmLen) {
  if (use24h) {
    strftime(timeBuf, timeLen, "%H:%M", &t);
    ampmBuf[0] = '\0';
  } else {
    int hour = t.tm_hour % 12;
    snprintf(timeBuf, timeLen, "%d:%02d", hour == 0 ? 12 : hour, t.tm_min);
    strftime(ampmBuf, ampmLen, "%p", &t);
  }
}

// "Wed, 8 Oct"
void formatDate(const struct tm& t, char* buf, size_t len) {
  char day[4], month[4];
  strftime(day, sizeof(day), "%a", &t);
  strftime(month, sizeof(month), "%b", &t);
  snprintf(buf, len, "%s, %d %s", day, t.tm_mday, month);
}

// ============================================================
// Home clock card - labels are updated in place by handleTimeDate()
// ============================================================
lv_obj_t* clockCard = NULL;       // the card that owns the labels below
lv_obj_t* clockTimeLabel = NULL;
lv_obj_t* clockAmPmLabel = NULL;
lv_obj_t* clockDateLabel = NULL;

bool setLabelIfChanged(lv_obj_t* label, const char* text) {
  bool changed = false;
  if (strcmp(lv_label_get_text(label), text) != 0) {
    lv_label_set_text(label, text);
    changed = true;
  }
  return changed;
}

void updateHomeClock() {
  if (clockTimeLabel == NULL){
    Serial.println("[Time] clock labels are NULL, skipping update");
    return;
  }

  if (!timeSynced()) {
    setLabelIfChanged(clockTimeLabel, "--:--");
    setLabelIfChanged(clockDateLabel, "Waiting for time sync");
    lv_obj_add_flag(clockAmPmLabel, LV_OBJ_FLAG_HIDDEN);
    Serial.printf("[Time] waiting for sync\n");
    return;
  }

  struct tm t;
  time_t now = time(NULL);
  localtime_r(&now, &t);

  char timeBuf[8], ampmBuf[4], dateBuf[20];
  formatClock(t, timeBuf, sizeof(timeBuf), ampmBuf, sizeof(ampmBuf));
  formatDate(t, dateBuf, sizeof(dateBuf));

  bool c1, c2, c3;
  c1 = setLabelIfChanged(clockTimeLabel, timeBuf);
  c2 = setLabelIfChanged(clockDateLabel, dateBuf);
  c3 = setLabelIfChanged(clockAmPmLabel, ampmBuf);
  if (c1 || c2 || c3) {
    Serial.printf("[Time] updated: %s %s %s\n", timeBuf, ampmBuf, dateBuf);
  }
  
  if (use24h) lv_obj_add_flag(clockAmPmLabel, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(clockAmPmLabel, LV_OBJ_FLAG_HIDDEN);
}

// The new Home screen is built before the old one is deleted, so only
// clear the labels if the deleted card is the one that owns them
void onHomeClockDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) != clockCard) return;
  clockCard = clockTimeLabel = clockAmPmLabel = clockDateLabel = NULL;
}

void createHomeClock(lv_obj_t* parent, lv_event_cb_t onClick) {
  lv_obj_t* card = createCard(parent, 60, 12, 200, 72, 14, onClick);
  lv_obj_set_style_bg_grad_color(card, UI_BG, 0);           // soft top-to-bottom fade
  lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
  lv_obj_add_event_cb(card, onHomeClockDeleted, LV_EVENT_DELETE, NULL);
  clockCard = card;

  // time + AM/PM on one baseline
  lv_obj_t* row = lv_obj_create(card);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(row, 4, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 8);

  clockTimeLabel = createLabel(row, "--:--", &lv_font_montserrat_28, UI_TEXT);
  lv_obj_set_style_text_letter_space(clockTimeLabel, 2, 0);
  clockAmPmLabel = createLabel(row, "", &lv_font_montserrat_14, UI_ACCENT);
  lv_obj_set_style_pad_bottom(clockAmPmLabel, 4, 0);

  createShape(card, 84, 44, 32, 2, 1, UI_ACCENT);  // thin accent line

  clockDateLabel = createLabel(card, "", &lv_font_montserrat_14, UI_SECONDARY);
  lv_obj_align(clockDateLabel, LV_ALIGN_BOTTOM_MID, 0, -6);

  updateHomeClock();
}

// ============================================================
// World clock screen
// ============================================================
struct WorldCity {
  const char* name;
  const char* tz;  // POSIX TZ rule, incl. daylight saving
};

#define WORLD_CITY_COUNT 6
const WorldCity worldCities[WORLD_CITY_COUNT] = {
  {"London",      "GMT0BST,M3.5.0/1,M10.5.0"},
  {"Paris",       "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"New York",    "EST5EDT,M3.2.0,M11.1.0"},
  {"Los Angeles", "PST8PDT,M3.2.0,M11.1.0"},
  {"Tokyo",       "JST-9"},
  {"Sydney",      "AEST-10AEDT,M10.1.0,M4.1.0/3"},
};

// Local time in another zone: switch TZ, convert, switch back
void timeIn(const char* tz, time_t now, struct tm* out) {
  setenv("TZ", tz, 1);
  tzset();
  localtime_r(&now, out);
  setenv("TZ", LOCAL_TZ, 1);
  tzset();
}

// "Today" / "Tomorrow" / "Yesterday" compared to Israel
const char* dayDifference(const struct tm& city, const struct tm& local) {
  int diff = (city.tm_year - local.tm_year) * 400 + (city.tm_yday - local.tm_yday);
  if (diff > 0) return "Tomorrow";
  if (diff < 0) return "Yesterday";
  return "Today";
}

lv_obj_t* worldCard = NULL;  // owns the labels below (same pattern as the home clock)
lv_obj_t* worldLocalTimeLabel = NULL;
lv_obj_t* worldLocalDateLabel = NULL;
lv_obj_t* worldTimeLabels[WORLD_CITY_COUNT];
lv_obj_t* worldDayLabels[WORLD_CITY_COUNT];

void updateWorldClock() {
  if (worldCard == NULL) return;

  if (!timeSynced()) {
    setLabelIfChanged(worldLocalTimeLabel, "--:--");
    setLabelIfChanged(worldLocalDateLabel, "Jerusalem - waiting for time sync");
    for (int i = 0; i < WORLD_CITY_COUNT; i++) {
      setLabelIfChanged(worldTimeLabels[i], "--:--");
      setLabelIfChanged(worldDayLabels[i], "");
    }
    return;
  }

  time_t now = time(NULL);
  struct tm local;
  localtime_r(&now, &local);

  char timeBuf[8], ampmBuf[4], text[32];
  formatClock(local, timeBuf, sizeof(timeBuf), ampmBuf, sizeof(ampmBuf));
  snprintf(text, sizeof(text), "%s%s%s", timeBuf, ampmBuf[0] ? " " : "", ampmBuf);
  setLabelIfChanged(worldLocalTimeLabel, text);
  char dateBuf[20];
  formatDate(local, dateBuf, sizeof(dateBuf));
  snprintf(text, sizeof(text), "Jerusalem - %s", dateBuf);
  setLabelIfChanged(worldLocalDateLabel, text);

  for (int i = 0; i < WORLD_CITY_COUNT; i++) {
    struct tm city;
    timeIn(worldCities[i].tz, now, &city);
    formatClock(city, timeBuf, sizeof(timeBuf), ampmBuf, sizeof(ampmBuf));
    snprintf(text, sizeof(text), "%s%s%s", timeBuf, ampmBuf[0] ? " " : "", ampmBuf);
    setLabelIfChanged(worldTimeLabels[i], text);
    setLabelIfChanged(worldDayLabels[i], dayDifference(city, local));
  }
}

void onWorldClockDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) != worldCard) return;
  worldCard = worldLocalTimeLabel = worldLocalDateLabel = NULL;
}

lv_obj_t* buildTimeScreen(lv_event_cb_t onBack) {
  lv_obj_t* scr = createScreen();

  createWiFiIcon(scr);
  createBackButton(scr, onBack);

  // header: title + Jerusalem date on the left, Jerusalem time on the right
  lv_obj_t* title = createLabel(scr, "World Clock", &lv_font_montserrat_20, UI_TEXT);
  lv_obj_set_pos(title, 60, 42);
  worldLocalDateLabel = createLabel(scr, "", &lv_font_montserrat_12, UI_SECONDARY);
  lv_obj_set_pos(worldLocalDateLabel, 60, 66);
  worldLocalTimeLabel = createLabel(scr, "", &lv_font_montserrat_28, UI_ACCENT);
  lv_obj_align(worldLocalTimeLabel, LV_ALIGN_TOP_RIGHT, -14, 42);

  // one card with a row per city: name | day | time
  worldCard = createCard(scr, 12, 90, 296, 146, 10, NULL);
  lv_obj_add_event_cb(worldCard, onWorldClockDeleted, LV_EVENT_DELETE, NULL);

  for (int i = 0; i < WORLD_CITY_COUNT; i++) {
    int y = 4 + i * 23;
    if (i > 0) createShape(worldCard, 10, y - 2, 274, 1, 0, UI_TRACK);  // row separator

    lv_obj_t* name = createLabel(worldCard, worldCities[i].name, &lv_font_montserrat_14, UI_TEXT);
    lv_obj_set_pos(name, 12, y + 2);

    worldDayLabels[i] = createLabel(worldCard, "", &lv_font_montserrat_12, UI_SECONDARY);
    lv_obj_set_pos(worldDayLabels[i], 120, y + 4);

    worldTimeLabels[i] = createLabel(worldCard, "", &lv_font_montserrat_14, UI_ACCENT);
    lv_obj_align(worldTimeLabels[i], LV_ALIGN_TOP_RIGHT, -12, y + 2);
  }

  updateWorldClock();
  return scr;
}

// ============================================================
// Starts NTP once WiFi is up, logs the first sync,
// and refreshes the clock labels once per second
// ============================================================
void handleTimeDate() {
  if (!ntpStarted && WiFi.status() == WL_CONNECTED) {
    configTzTime(LOCAL_TZ, "pool.ntp.org", "time.google.com");
    ntpStarted = true;
    Serial.println("[Time] NTP started");
  }

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 1000) return;
  lastUpdate = millis();

  static bool syncLogged = false;
  if (!syncLogged && timeSynced()) {
    syncLogged = true;
    struct tm t;
    time_t now = time(NULL);
    localtime_r(&now, &t);
    Serial.printf("[Time] Synced: %04d-%02d-%02d %02d:%02d\n",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
  }

  updateHomeClock();
  updateWorldClock();
}
#endif
