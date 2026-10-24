#ifndef TIME_DATE_H
#define TIME_DATE_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../storage/storage.h"
#include "../home/home_layout.h"
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
// Home clock widget (big digits, tap: world clock) and the greeting row
// above the widgets. Labels are updated in place by handleTimeDate(),
// never rebuilt. The widget can be any size (home_layout.h):
//   compact (under 64 px tall): time, date beside it (wide) or under it
//   normal: time with the date under it (the original look)
//   big (full width, 72 px or more): 76 px digits, date beside or under
// ============================================================
lv_obj_t* clockBox = NULL;       // owns the labels below
lv_obj_t* clockTimeLabel = NULL;
lv_obj_t* clockAmPmLabel = NULL;
lv_obj_t* clockDateLabel = NULL;
lv_obj_t* clockTopLabel = NULL;   // greeting row, left of the WiFi icon (not part of the widget)

enum ClockMode { CLOCK_NORMAL, CLOCK_COMPACT_STACK, CLOCK_COMPACT_ROW, CLOCK_BIG_ROW, CLOCK_BIG_CENTER };
static ClockMode clockMode = CLOCK_NORMAL;
static int clockBoxW = 0, clockBoxH = 0;
static int clockGrowH = 0;  // greeting off: extra height above the clock's row (home_layout.h)

const char* greetingFor(int hour) {
  if (hour >= 5 && hour < 12) return "Good morning";
  if (hour >= 12 && hour < 17) return "Good afternoon";
  if (hour >= 17 && hour < 22) return "Good evening";
  return "Good night";
}

// Places the time, AM/PM and date for the widget's size. Left-aligned
// modes line the digits' left edge up with the cards (x 12): each digit
// has a little empty space before it in the font, so the label moves left
// by that amount. Called whenever the text changes (widths change).
static bool clockMidFont = false;  // time drawn with font_display_mid (56 px)
static bool clockTall = false;     // half width with the greeting row's space: two-line date

static void placeClock(const char* text) {
  static const int8_t firstGapPx[10] = {1, 3, 1, 1, 1, 2, 1, 2, 1, 1};  // '0'..'9' at 38 px
  bool big = clockMode == CLOCK_BIG_ROW || clockMode == CLOCK_BIG_CENTER;
  int gap = (text[0] >= '0' && text[0] <= '9') ? firstGapPx[text[0] - '0'] : 2;
  if (big) gap *= 2;
  else if (clockMidFont) gap = gap * 3 / 2;  // 56 px digits
  lv_obj_update_layout(clockTimeLabel);
  lv_obj_update_layout(clockDateLabel);
  int timeH = lv_obj_get_height(clockTimeLabel), dateH = lv_obj_get_height(clockDateLabel);
  int timeW = lv_obj_get_width(clockTimeLabel);
  lv_obj_update_layout(clockAmPmLabel);
  int ampmW = lv_label_get_text(clockAmPmLabel)[0] ? lv_obj_get_width(clockAmPmLabel) + 6 : 0;
  const int left = 8;  // the box starts 8 px left of the widget, for a bigger tap area

  switch (clockMode) {
    case CLOCK_NORMAL: {  // half width: 56 px digits (font_display_mid), date under them
      // In the 45 px label the digits are drawn from y 4 to 44; the date starts 4 px below them.
      const int digitTop = 4, dateY = 48;
      // the date never reaches the widget beside it: a long one gets a smaller font
      if (lv_obj_get_width(clockDateLabel) > clockBoxW - left) {
        lv_obj_set_style_text_font(clockDateLabel, &lv_font_montserrat_12, 0);
        lv_obj_update_layout(clockDateLabel);
        dateH = lv_obj_get_height(clockDateLabel);
      }
      int top;
      if (clockGrowH > 0) {
        // greeting off: the clock also has the space above its row. The date
        // (two lines, "Monday" / "19 Oct", when there is room) ends where the
        // widget beside it ends, and the digits go up into the free space.
        top = max(0, clockBoxH - dateH - dateY);
      } else {
        // centered in its row, so it lines up with the widget beside it
        int visualH = dateY - digitTop + dateH;
        top = 4 + (clockBoxH - 4 - visualH) / 2 - digitTop;
      }
      lv_obj_set_pos(clockTimeLabel, left - gap, top);
      lv_obj_set_pos(clockDateLabel, left, top + dateY);
      break;
    }
    case CLOCK_COMPACT_STACK: {  // narrow and low: smaller time, small date right under it
      int top = 4 + (clockBoxH - 4 - timeH - dateH + 6) / 2;
      lv_obj_set_pos(clockTimeLabel, left - 1, top);
      lv_obj_set_pos(clockDateLabel, left, top + timeH - 6);
      break;
    }
    case CLOCK_COMPACT_ROW:  // wide and low: time on the left, date on the right
    case CLOCK_BIG_ROW: {
      int mid = 4 + (clockBoxH - 4) / 2;
      lv_obj_set_pos(clockTimeLabel, left - gap, mid - timeH / 2);
      lv_obj_set_pos(clockDateLabel, clockBoxW - lv_obj_get_width(clockDateLabel), mid - dateH / 2);
      break;
    }
    case CLOCK_BIG_CENTER: {  // big digits in the middle, date under them
      const int space = 14;  // the digits reach a little below the font's line
      int groupH = timeH + space + dateH;
      int top = 4 + (clockBoxH - 4 - groupH) / 2;
      lv_obj_set_pos(clockTimeLabel, left + (clockBoxW - left - timeW - ampmW) / 2, top);
      lv_obj_set_pos(clockDateLabel, left + (clockBoxW - left - lv_obj_get_width(clockDateLabel)) / 2, top + timeH + space);
      break;
    }
  }
  lv_obj_update_layout(clockTimeLabel);
  lv_obj_update_layout(clockDateLabel);
  if (clockMode == CLOCK_NORMAL && lv_obj_get_x(clockTimeLabel) + timeW + ampmW > clockBoxW) {
    // "12:45 PM" is wider than half the screen: AM/PM goes at the end of the date line
    lv_obj_align_to(clockAmPmLabel, clockDateLabel, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
  } else {
    lv_obj_align_to(clockAmPmLabel, clockTimeLabel, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, big ? -8 : -3);
  }
}

// Greeting row of Home: the greeting (unless turned off in Settings > Home
// screen), or the connection problem in red ("No internet" / "No
// WiFi"), which is always shown so it is the first thing seen (the top-right
// icon then only shows a red warning sign).
static void setHomeTopRow(const char* greeting) {
  if (clockTopLabel == NULL) return;
  ConnectionState conn = connectionState();
  if (!homeGreetingShown) {  // the WiFi icon shows problems itself (home.h)
    setLabelIfChanged(clockTopLabel, "");
    return;
  }
  const char* text = conn == CONN_NO_INTERNET ? LV_SYMBOL_WARNING " No internet"
                   : conn == CONN_OFFLINE     ? LV_SYMBOL_WARNING " No WiFi"
                                              : greeting;
  setLabelIfChanged(clockTopLabel, text);
  if (conn == CONN_OK) lv_obj_remove_state(clockTopLabel, LV_STATE_USER_1);
  else lv_obj_add_state(clockTopLabel, LV_STATE_USER_1);
}

void updateHomeClock() {
  if (!timeSynced()) {
    setHomeTopRow("Hello");
    if (clockTimeLabel == NULL) return;
    setLabelIfChanged(clockTimeLabel, "--:--");
    setLabelIfChanged(clockAmPmLabel, "");
    const char* waiting = WiFi.status() == WL_CONNECTED ? "Getting the time..." : "Waiting for WiFi";
    setLabelIfChanged(clockDateLabel, waiting);
    placeClock("--:--");
    return;
  }

  struct tm t;
  time_t now = time(NULL);
  localtime_r(&now, &t);
  setHomeTopRow(greetingFor(t.tm_hour));
  if (clockTimeLabel == NULL) return;  // clock widget hidden

  char timeBuf[8], ampmBuf[4], dateBuf[24];
  formatClock(t, timeBuf, sizeof(timeBuf), ampmBuf, sizeof(ampmBuf));
  if (clockMode == CLOCK_COMPACT_STACK) formatDate(t, dateBuf, sizeof(dateBuf));  // short: "Mon, 19 Oct"
  else formatLongDate(t, dateBuf, sizeof(dateBuf));
  if (clockTall) {  // "Monday" and "19 Oct" on two lines
    char* space = strchr(dateBuf, ' ');
    if (space) *space = '\n';
  }
  bool changed = setLabelIfChanged(clockTimeLabel, timeBuf);
  changed |= setLabelIfChanged(clockAmPmLabel, ampmBuf);
  changed |= setLabelIfChanged(clockDateLabel, dateBuf);
  if (changed) placeClock(timeBuf);
}

// The new Home screen is built before the old one is deleted, so only
// clear the labels if the deleted object is the one that owns them
void onHomeClockDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) != clockBox) return;
  clockBox = clockTimeLabel = clockAmPmLabel = clockDateLabel = NULL;
}

void onHomeTopRowDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) == clockTopLabel) clockTopLabel = NULL;
}

// Greeting row (always on Home, even when the clock widget is hidden)
void createHomeTopRow(lv_obj_t* parent) {
  clockTopLabel = createLabel(parent, "", &stTitle);
  passClicks(clockTopLabel);
  lv_obj_add_style(clockTopLabel, &stSecondary, 0);
  lv_obj_set_style_text_font(clockTopLabel, &font_hebrew_20, 0);  // grey, still 20 px
  lv_obj_add_style(clockTopLabel, &stDanger, LV_STATE_USER_1);     // red for a connection problem
  lv_obj_set_style_text_font(clockTopLabel, &font_hebrew_20, LV_STATE_USER_1);
  lv_obj_set_width(clockTopLabel, 220);
  lv_obj_set_pos(clockTopLabel, MARGIN, 11);
  lv_obj_add_event_cb(clockTopLabel, onHomeTopRowDeleted, LV_EVENT_DELETE, NULL);
}

// The clock widget at x, y, w, h (screen coordinates of the widget's area).
// The tap box reaches 8 px further left and 4 px higher, into the margin.
void createHomeClock(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t onClick) {
  clockBoxW = w + 8;
  clockBoxH = h + 4;
  clockGrowH = y < HOME_AREA_Y ? HOME_AREA_Y - y : 0;
  lv_obj_t* box = createCard(parent, x - 8, y - 4, clockBoxW, clockBoxH, onClick, &stFlat);
  lv_obj_set_style_radius(box, 12, 0);  // shape of the pressed highlight
  lv_obj_add_event_cb(box, onHomeClockDeleted, LV_EVENT_DELETE, NULL);
  clockBox = box;

  bool full = w >= HOME_AREA_W;
  clockMode = full && h >= 100 ? CLOCK_BIG_CENTER
            : full && h >= 72  ? CLOCK_BIG_ROW
            : full             ? CLOCK_COMPACT_ROW  // time left, date right: the whole row is used
            : h < 64           ? CLOCK_COMPACT_STACK
                               : CLOCK_NORMAL;
  bool big = clockMode == CLOCK_BIG_ROW || clockMode == CLOCK_BIG_CENTER;
  // 56 px digits wherever they fit, so the clock fills its space
  clockMidFont = clockMode == CLOCK_NORMAL || (clockMode == CLOCK_COMPACT_ROW && h >= 60);
  // greeting off and enough height for digits + a two-line 20 px date (48 + 46 px)
  clockTall = clockMode == CLOCK_NORMAL && clockGrowH > 0 && clockBoxH >= 100;

  // Hierarchy: the clock is the brightest thing; the date is in the accent color
  clockDateLabel = createLabel(box, "", &stAccent);
  if (clockMode == CLOCK_COMPACT_STACK) lv_obj_set_style_text_font(clockDateLabel, &lv_font_montserrat_12, 0);  // fits under the time
  if (clockTall) lv_obj_set_style_text_font(clockDateLabel, &lv_font_montserrat_20, 0);  // two short lines fit at 20 px
  // compact and narrow: Montserrat 28 digits, so the date fits under them
  clockTimeLabel = createLabel(box, "--:--", big                            ? &stDisplayBig
                                           : clockMidFont                    ? &stDisplayMid
                                           : clockMode == CLOCK_COMPACT_STACK ? &stValue
                                                                              : &stDisplay);
  clockAmPmLabel = createLabel(box, "", big ? &stSecondary : &stSmall);  // 12-hour clock only
  updateHomeClock();
  placeClock(lv_label_get_text(clockTimeLabel));
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

void updateWorldClock(bool force = false) {
  if (worldList == NULL) return;
  bool synced = timeSynced();
  time_t now = time(NULL);
  // the dials and times only change once a minute: switching the time zone
  // 12 times a second costs memory and confuses the other tasks' clocks
  static long lastMinute = -1;
  long minute = synced ? (long)(now / 60) : -1;
  if (!force && minute == lastMinute) return;
  lastMinute = minute;
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
  updateWorldClock(true);
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
