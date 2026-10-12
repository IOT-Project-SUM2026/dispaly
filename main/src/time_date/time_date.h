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

// "Sunday 11 Oct"
void formatLongDate(const struct tm& t, char* buf, size_t len) {
  char day[12], month[4];
  strftime(day, sizeof(day), "%A", &t);
  strftime(month, sizeof(month), "%b", &t);
  snprintf(buf, len, "%s %d %s", day, t.tm_mday, month);
}

// ============================================================
// Home clock (big digits, tap: world clock). Labels are updated
// in place by handleTimeDate(), never rebuilt.
// ============================================================
lv_obj_t* clockBox = NULL;       // owns the labels below
lv_obj_t* clockTimeLabel = NULL;
lv_obj_t* clockAmPmLabel = NULL;
lv_obj_t* clockDateLabel = NULL;
lv_obj_t* clockTopLabel = NULL;   // top row, left of the WiFi icon


const char* greetingFor(int hour) {
  if (hour >= 5 && hour < 12) return "Good morning";
  if (hour >= 12 && hour < 17) return "Good afternoon";
  if (hour >= 17 && hour < 22) return "Good evening";
  return "Good night";
}

// Lines the clock's left edge up with the date and the cards (x 12): each
// digit has a little empty space before it (from font_display_40.c), so the
// label moves left by that amount. AM/PM follows the clock's real width.
// Lines the clock's left edge up with the greeting, the date and the cards
// (x 12): each digit has a little empty space before it in font_display.c,
// so the label moves left by that amount. AM/PM follows the clock's width.
static void alignClock(const char* text) {
  static const int8_t firstGapPx[10] = {1, 3, 1, 1, 1, 2, 1, 2, 1, 1};  // '0'..'9'
  int gap = (text[0] >= '0' && text[0] <= '9') ? firstGapPx[text[0] - '0'] : 2;  // '-': 2
  lv_obj_set_x(clockTimeLabel, MARGIN - 4 - gap);  // the tap box starts at x 4
  lv_obj_update_layout(clockTimeLabel);
  lv_obj_align_to(clockAmPmLabel, clockTimeLabel, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
}

void updateHomeClock() {
  if (clockTimeLabel == NULL) return;

  if (!timeSynced()) {
    setLabelIfChanged(clockTimeLabel, "--:--");
    setLabelIfChanged(clockAmPmLabel, "");
    alignClock("--:--");
    const char* waiting = WiFi.status() == WL_CONNECTED ? "Getting the time..." : "Waiting for WiFi";
    setLabelIfChanged(clockTopLabel, "Hello");
    setLabelIfChanged(clockDateLabel, waiting);
    return;
  }

  struct tm t;
  time_t now = time(NULL);
  localtime_r(&now, &t);

  char timeBuf[8], ampmBuf[4], dateBuf[24];
  formatClock(t, timeBuf, sizeof(timeBuf), ampmBuf, sizeof(ampmBuf));
  formatLongDate(t, dateBuf, sizeof(dateBuf));
  bool changed = setLabelIfChanged(clockTimeLabel, timeBuf);
  changed |= setLabelIfChanged(clockAmPmLabel, ampmBuf);
  changed |= setLabelIfChanged(clockDateLabel, dateBuf);
  if (changed) alignClock(timeBuf);
  setLabelIfChanged(clockTopLabel, greetingFor(t.tm_hour));
  setLabelIfChanged(clockDateLabel, dateBuf);
}

// The new Home screen is built before the old one is deleted, so only
// clear the labels if the deleted box is the one that owns them
void onHomeClockDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) != clockBox) return;
  clockBox = clockTimeLabel = clockAmPmLabel = clockDateLabel = clockTopLabel = NULL;
}

// Left half of the top of Home: x 4..160, y 4..116 (tap area), text starts at x 12
void createHomeClock(lv_obj_t* parent, lv_event_cb_t onClick) {
  lv_obj_t* box = createCard(parent, 4, 4, COL_W + 12, 108, onClick, &stFlat);
  lv_obj_set_style_radius(box, 12, 0);  // shape of the pressed highlight
  lv_obj_add_event_cb(box, onHomeClockDeleted, LV_EVENT_DELETE, NULL);
  clockBox = box;

  // Greeting on the top row; clock and date as one group next to the weather card.
  // Hierarchy: the clock is the brightest thing; the greeting is a quieter
  // grey heading; the date sits right under the clock in the accent color
  clockTopLabel = createLabel(box, "", &stTitle);
  lv_obj_add_style(clockTopLabel, &stSecondary, 0);
  lv_obj_set_style_text_font(clockTopLabel, &font_hebrew_20, 0);  // grey, still 20 px
  lv_obj_set_pos(clockTopLabel, 8, 7);
  // clock and date 8 px apart; the date sits on the same line as the city name
  const int digitsY = 52 - 3;  // the digits start 3 px below the label's top
  clockDateLabel = createLabel(box, "", &stAccent);
  lv_obj_set_pos(clockDateLabel, 8, 87);
  clockTimeLabel = createLabel(box, "--:--", &stDisplay);
  lv_obj_set_pos(clockTimeLabel, 8, digitsY);
  clockAmPmLabel = createLabel(box, "", &stSmall);  // 12-hour clock only
  updateHomeClock();
  alignClock(lv_label_get_text(clockTimeLabel));
}

// Small clock in the header of the other screens (shared_ui.h)
void updateStatusClock() {
  if (statusClock == NULL) return;
  if (!timeSynced()) {
    setLabelIfChanged(statusClock, "");
    return;
  }
  struct tm t;
  time_t now = time(NULL);
  localtime_r(&now, &t);
  char timeBuf[8], ampmBuf[4], text[16];
  formatClock(t, timeBuf, sizeof(timeBuf), ampmBuf, sizeof(ampmBuf));
  snprintf(text, sizeof(text), "%s%s%s", timeBuf, ampmBuf[0] ? " " : "", ampmBuf);
  setLabelIfChanged(statusClock, text);
}

// ============================================================
// World clock screen: 6 cities as a 2 x 3 grid of tiles
// (the local time is in the header)
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


lv_obj_t* worldList = NULL;  // owns the objects below (same pattern as the home clock)
lv_obj_t* worldDials[WORLD_CITY_COUNT];
lv_obj_t* worldTimeLabels[WORLD_CITY_COUNT];

// ------------------------------------------------------------
// Analog dial: LVGL's lv_scale widget in round mode (12 ticks,
// 0..60 around the circle) with two line needles. Light face from
// 6:00 to 18:00, dark face at night (state LV_STATE_USER_1), so day
// and night show at a glance. The needles move once a minute.
// ------------------------------------------------------------
lv_obj_t* worldHourHands[WORLD_CITY_COUNT];
lv_obj_t* worldMinuteHands[WORLD_CITY_COUNT];
lv_obj_t* worldDialCaps[WORLD_CITY_COUNT];
int worldDialMinutes[WORLD_CITY_COUNT];

static lv_obj_t* createDial(lv_obj_t* parent, int size, int i) {
  lv_obj_t* dial = lv_scale_create(parent);
  lv_obj_remove_style_all(dial);
  lv_obj_set_size(dial, size, size);
  lv_scale_set_mode(dial, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_range(dial, 0, 60);
  lv_scale_set_total_tick_count(dial, 13);     // one every 5 minutes (every hour)
  lv_scale_set_major_tick_every(dial, 3);      // 12, 3, 6, 9 are major
  lv_scale_set_angle_range(dial, 360);
  lv_scale_set_rotation(dial, 270);            // 0 at the top
  lv_scale_set_label_show(dial, false);
  lv_obj_add_style(dial, &stDialFace, LV_PART_MAIN);
  lv_obj_add_style(dial, &stDialFaceNight, LV_PART_MAIN | LV_STATE_USER_1);
  lv_obj_add_style(dial, &stDialTicks, LV_PART_INDICATOR);
  lv_obj_add_style(dial, &stDialTicksNight, LV_PART_INDICATOR | LV_STATE_USER_1);
  lv_obj_set_style_length(dial, 0, LV_PART_ITEMS);  // only the 4 major ticks
  passClicks(dial);

  worldHourHands[i] = lv_line_create(dial);
  lv_obj_add_style(worldHourHands[i], &stDialHour, 0);
  lv_obj_add_style(worldHourHands[i], &stDialHourNight, LV_STATE_USER_1);
  worldMinuteHands[i] = lv_line_create(dial);
  lv_obj_add_style(worldMinuteHands[i], &stDialMinute, 0);
  lv_obj_t* cap = lv_obj_create(dial);  // small dot over the middle, where the hands meet
  lv_obj_remove_style_all(cap);
  lv_obj_set_size(cap, 6, 6);
  lv_obj_center(cap);
  lv_obj_add_style(cap, &stDialCap, 0);
  lv_obj_add_style(cap, &stDialCapNight, LV_STATE_USER_1);
  passClicks(cap);
  worldDialCaps[i] = cap;
  worldDialMinutes[i] = -2;
  return dial;
}

static void setDial(int i, int minutes) {
  if (worldDialMinutes[i] == minutes) return;
  worldDialMinutes[i] = minutes;
  lv_obj_t* dial = worldDials[i];
  if (minutes < 0) {  // time unknown: no hands
    lv_obj_add_flag(worldHourHands[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(worldMinuteHands[i], LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_remove_flag(worldHourHands[i], LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(worldMinuteHands[i], LV_OBJ_FLAG_HIDDEN);
  bool night = minutes < 6 * 60 || minutes >= 18 * 60;
  lv_state_t st = LV_STATE_USER_1;
  lv_obj_t* parts[] = {dial, worldHourHands[i], worldDialCaps[i]};
  for (lv_obj_t* o : parts) {
    if (night) lv_obj_add_state(o, st);
    else lv_obj_remove_state(o, st);
  }

  lv_obj_update_layout(dial);  // the needles are placed from the dial's position on screen
  int r = lv_obj_get_width(dial) / 2;
  int hourValue = (minutes / 60 % 12) * 5 + (minutes % 60) / 12;  // the hour hand on the 0..60 scale
  lv_scale_set_line_needle_value(dial, worldHourHands[i], r * 45 / 100, hourValue);
  lv_scale_set_line_needle_value(dial, worldMinuteHands[i], r * 72 / 100, minutes % 60);
}

void updateWorldClock() {
  if (worldList == NULL) return;
  bool synced = timeSynced();
  time_t now = time(NULL);
  for (int i = 0; i < WORLD_CITY_COUNT; i++) {
    if (!synced) {
      setLabelIfChanged(worldTimeLabels[i], "--:--");
      setDial(i, -1);
      continue;
    }
    struct tm city;
    timeIn(worldCities[i].tz, now, &city);
    char timeBuf[8], ampmBuf[4], text[24];
    formatClock(city, timeBuf, sizeof(timeBuf), ampmBuf, sizeof(ampmBuf));
    snprintf(text, sizeof(text), "%s%s%s", timeBuf, ampmBuf[0] ? " " : "", ampmBuf);
    setLabelIfChanged(worldTimeLabels[i], text);
    setDial(i, city.tm_hour * 60 + city.tm_min);
  }
}

void onWorldClockDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) == worldList) worldList = NULL;
}

// World clock: 2 x 3 tiles, each with an analog dial (light by day, dark
// at night) next to the city name and the digital time
lv_obj_t* buildTimeScreen(lv_event_cb_t onBack) {
  lv_obj_t* scr = createScreen();
  createHeader(scr, "World Clock", onBack);
  worldList = createBox(scr, 0, CONTENT_Y, SCREEN_WIDTH, SCREEN_HEIGHT - CONTENT_Y);
  lv_obj_add_event_cb(worldList, onWorldClockDeleted, LV_EVENT_DELETE, NULL);

  const int tileH = 60;
  for (int i = 0; i < WORLD_CITY_COUNT; i++) {
    int x = MARGIN + (i % 2) * (COL_W + GAP);
    int y = (i / 2) * (tileH + 6);
    lv_obj_t* tile = createCard(worldList, x, y, COL_W, tileH, NULL);
    worldDials[i] = createDial(tile, 38, i);
    lv_obj_set_pos(worldDials[i], 8, (tileH - 2 - 38) / 2);
    lv_obj_t* name = createLabel(tile, worldCities[i].name, &stSecondary);
    makeSingleLine(name, COL_W - 2 - 52 - 2);  // "Los Angeles" just fits
    lv_obj_set_pos(name, 52, 8);
    worldTimeLabels[i] = createLabel(tile, "", &stTitle);
    lv_obj_set_pos(worldTimeLabels[i], 52, 26);
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
    // course guide: other time servers are blocked on Technion WiFi, so the Technion one goes first
    configTzTime(LOCAL_TZ, "ntp.technion.ac.il", "pool.ntp.org", "time.google.com");
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
  updateStatusClock();
  updateWorldClock();
}
#endif
