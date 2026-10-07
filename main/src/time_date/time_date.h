#ifndef TIME_DATE_H
#define TIME_DATE_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include <WiFi.h>
#include <time.h>

// Israel time zone with automatic daylight saving (POSIX TZ rule)
#define LOCAL_TZ "IST-2IDT,M3.4.4/26,M10.5.0"

bool use24h = true;
bool ntpStarted = false;

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
lv_obj_t* clockTimeLabel = NULL;
lv_obj_t* clockAmPmLabel = NULL;
lv_obj_t* clockDateLabel = NULL;

void setLabelIfChanged(lv_obj_t* label, const char* text) {
  if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void updateHomeClock() {
  if (clockTimeLabel == NULL) return;

  if (!timeSynced()) {
    setLabelIfChanged(clockTimeLabel, "--:--");
    setLabelIfChanged(clockDateLabel, "Waiting for time sync");
    lv_obj_add_flag(clockAmPmLabel, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  struct tm t;
  time_t now = time(NULL);
  localtime_r(&now, &t);

  char timeBuf[8], ampmBuf[4], dateBuf[20];
  formatClock(t, timeBuf, sizeof(timeBuf), ampmBuf, sizeof(ampmBuf));
  formatDate(t, dateBuf, sizeof(dateBuf));

  setLabelIfChanged(clockTimeLabel, timeBuf);
  setLabelIfChanged(clockDateLabel, dateBuf);
  setLabelIfChanged(clockAmPmLabel, ampmBuf);
  if (use24h) lv_obj_add_flag(clockAmPmLabel, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(clockAmPmLabel, LV_OBJ_FLAG_HIDDEN);
}

void onHomeClockDeleted(lv_event_t* e) {
  clockTimeLabel = clockAmPmLabel = clockDateLabel = NULL;
}

void createHomeClock(lv_obj_t* parent, lv_event_cb_t onClick) {
  lv_obj_t* card = createCard(parent, 60, 12, 200, 72, 14, onClick);
  lv_obj_set_style_bg_grad_color(card, UI_BG, 0);           // soft top-to-bottom fade
  lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
  lv_obj_add_event_cb(card, onHomeClockDeleted, LV_EVENT_DELETE, NULL);

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
}
#endif
