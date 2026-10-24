#ifndef CALENDAR_H
#define CALENDAR_H
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "../time_date/time_date.h"
#include "../config/config.h"
#include "../net/net.h"
#include "../home/home_layout.h"

// ============================================================
// Upcoming events from Google Calendar (Google Calendar API).
//
// Reads a public Google calendar with an API key, so the display
// needs no Google login. The calendar ID and key are set on the setup
// page (config.h; secrets.h gives the starting values). Without them
// the calendar shows "Calendar not set up" and the rest works as usual.
// Checks every 10 minutes. No screen code here (calendar_screen.h).
// The download runs in the background (net task), like the weather.
// ============================================================

// Days since 1 Jan 1970 for a calendar date, used to compare dates
// ("is this event today or tomorrow?")
long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const long yoe = y - era * 400;
  const long doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

long dayNumber(const struct tm& t) {
  return daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

#define CALENDAR_MAX_EVENTS  8
#define CALENDAR_TITLE_LEN   48       // bytes incl. the terminator
#define CALENDAR_REFRESH_MS  600000   // 10 min
#define CALENDAR_RETRY_MS    60000    // after an error

struct CalendarEvent {
  char title[CALENDAR_TITLE_LEN];  // as written in Google Calendar (UTF-8)
  bool allDay;
  long startDay;    // local date, as daysFromCivil()
  long endDay;      // all-day events: the day after the last day (Google's "end.date")
  int startHour, startMinute;      // local time (timed events)
  time_t start, end;               // timed events: seconds since 1970, for "is it on now?"
};

enum CalendarState { CALENDAR_NOT_SET_UP, CALENDAR_LOADING, CALENDAR_READY, CALENDAR_FAILED };

CalendarEvent calendarEvents[CALENDAR_MAX_EVENTS];
int calendarEventCount = 0;
CalendarState calendarState = CALENDAR_NOT_SET_UP;  // set by initCalendar()
int calendarError = 0;  // HTTP code of the last failed request (negative = no connection)

// Filled by the background download, copied by handleCalendar() in loop()
static CalendarEvent calendarDownloaded[CALENDAR_MAX_EVENTS];
static int calendarDownloadedCount = 0;
static int calendarDownloadError = 0;

time_t calendarUpdatedAt = 0;  // when the events were last downloaded (0 = unknown)
#define CALENDAR_STALE_S 3600   // older than 1 hour: "Updated" is shown in red

// ---------- saved in flash, so the events show after a restart without internet ----------
#define CALENDAR_SAVE_VERSION 1
struct SavedCalendar {
  uint8_t version, count;
  uint32_t updatedAt;
  CalendarEvent events[CALENDAR_MAX_EVENTS];
};

// Saves the events when they changed (rare), plus the time of the download
static void saveCalendar() {
  static uint32_t savedSum = 0;
  uint32_t sum = calendarEventCount;  // cheap fingerprint of the events
  const uint8_t* bytes = (const uint8_t*)calendarEvents;
  for (size_t i = 0; i < sizeof(CalendarEvent) * calendarEventCount; i++) sum = sum * 31 + bytes[i];
  if (sum == savedSum) return;  // nothing new: no flash write
  SavedCalendar* s = (SavedCalendar*)calloc(1, sizeof(SavedCalendar));
  if (!s) return;
  s->version = CALENDAR_SAVE_VERSION;
  s->count = calendarEventCount;
  s->updatedAt = (uint32_t)calendarUpdatedAt;
  memcpy(s->events, calendarEvents, sizeof(CalendarEvent) * calendarEventCount);
  Preferences p;
  if (p.begin("calendar", false)) {
    if (p.putBytes("data", s, sizeof(SavedCalendar)) == sizeof(SavedCalendar)) savedSum = sum;
    p.end();
  }
  free(s);
}

// Call once in setup(), after initConfig()
void initCalendar() {
  calendarState = calendarConfigured() ? CALENDAR_LOADING : CALENDAR_NOT_SET_UP;
  if (calendarState == CALENDAR_NOT_SET_UP) return;
  Preferences p;
  if (!p.begin("calendar", true)) return;  // never saved
  SavedCalendar* s = (SavedCalendar*)calloc(1, sizeof(SavedCalendar));
  bool ok = s && p.getBytesLength("data") == sizeof(SavedCalendar) &&
            p.getBytes("data", s, sizeof(SavedCalendar)) == sizeof(SavedCalendar) &&
            s->version == CALENDAR_SAVE_VERSION && s->count <= CALENDAR_MAX_EVENTS;
  p.end();
  if (ok) {
    memcpy(calendarEvents, s->events, sizeof(CalendarEvent) * s->count);
    for (int i = 0; i < s->count; i++) calendarEvents[i].title[CALENDAR_TITLE_LEN - 1] = '\0';
    calendarEventCount = s->count;
    calendarUpdatedAt = s->updatedAt;
    calendarState = CALENDAR_READY;  // shown once the clock is set; replaced by the first download
    Serial.printf("[Calendar] %d saved event%s loaded\n", s->count, s->count == 1 ? "" : "s");
  }
  free(s);
}

// True when the events on screen are older than 1 hour (or their age is unknown)
bool calendarStale() {
  if (calendarUpdatedAt == 0 || !timeSynced()) return true;
  return time(NULL) - calendarUpdatedAt > CALENDAR_STALE_S;
}

bool calendarCheckNow = false;  // set when the Calendar screen opens: check right away

// Called when the Calendar screen opens
void requestCalendarCheck() {
  calendarCheckNow = true;
}

// Ended events stay in the list until the next check, so screens skip them
bool calendarEventOver(const CalendarEvent& e, time_t now, long today) {
  return e.allDay ? e.endDay <= today : e.end <= now;
}

// True if an event started or ended after `from` and up to `to`, so the
// screen must change ("14:00" becomes "Now", an ended event disappears).
// All-day events change at midnight, which the clock handles.
bool calendarChangedBetween(time_t from, time_t to) {
  for (int i = 0; i < calendarEventCount; i++) {
    const CalendarEvent& e = calendarEvents[i];
    if (e.allDay) continue;
    if ((e.start > from && e.start <= to) || (e.end > from && e.end <= to)) return true;
  }
  return false;
}

// Calendar IDs contain '@' (and sometimes '#'), which must be escaped in a URL
static String calendarUrlEncode(const char* text) {
  const char* hex = "0123456789ABCDEF";
  String out;
  for (const char* p = text; *p; p++) {
    uint8_t c = *p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

// Copies at most size-1 bytes without cutting a multi-byte (e.g. Hebrew) character in half
static void calendarCopyTitle(char* dst, const char* src, size_t size) {
  size_t len = strlen(src);
  if (len > size - 1) {
    len = size - 1;
    while (len > 0 && ((uint8_t)src[len] & 0xC0) == 0x80) len--;
  }
  memcpy(dst, src, len);
  dst[len] = '\0';
}

// Reads Google's "2026-10-07T14:00:00+03:00" (timed) or "2026-10-07" (all-day).
// Returns false if the text isn't a date.
static bool calendarParseTime(const char* text, long& day, int& hour, int& minute, time_t& utc) {
  int y, mo, d;
  if (strlen(text) < 10 || sscanf(text, "%4d-%2d-%2d", &y, &mo, &d) != 3) return false;
  day = daysFromCivil(y, mo, d);
  hour = minute = 0;
  int second = 0;
  long offset = 0;  // seconds ahead of UTC
  if (text[10] == 'T') {
    if (strlen(text) < 16 || sscanf(text + 11, "%2d:%2d", &hour, &minute) != 2) return false;
    const char* zone = text + 16;  // after "YYYY-MM-DDTHH:MM"
    if (*zone == ':') {            // seconds
      second = atoi(zone + 1);
      zone++;
      while (isdigit((uint8_t)*zone)) zone++;
    }
    if (*zone == '.') {            // fractions of a second
      zone++;
      while (isdigit((uint8_t)*zone)) zone++;
    }
    int oh = 0, om = 0;
    if ((*zone == '+' || *zone == '-') && sscanf(zone + 1, "%2d:%2d", &oh, &om) >= 1) {
      offset = (oh * 3600L + om * 60L) * (*zone == '-' ? -1 : 1);
    }
  }
  utc = (time_t)day * 86400 + hour * 3600L + minute * 60L + second - offset;
  return true;
}

// Fills calendarDownloaded from Google's reply. Returns false if the reply can't be read.
bool calendarParseEvents(const String& payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  JsonArray items = doc["items"];
  if (items.isNull()) return false;

  int count = 0;
  for (JsonObject item : items) {
    if (count >= CALENDAR_MAX_EVENTS) break;
    CalendarEvent& e = calendarDownloaded[count];
    const char* startText = item["start"]["dateTime"].as<const char*>();  // nullptr for all-day events
    const char* endText = item["end"]["dateTime"].as<const char*>();
    e.allDay = startText == nullptr;
    if (e.allDay) {
      startText = item["start"]["date"] | "";
      endText = item["end"]["date"] | "";
    }
    long endDay;
    int endHour, endMinute;
    if (!calendarParseTime(startText, e.startDay, e.startHour, e.startMinute, e.start)) continue;
    if (!endText || !calendarParseTime(endText, endDay, endHour, endMinute, e.end)) {
      endDay = e.startDay + 1;  // no end given: treat as one day / starting now
      e.end = e.start;
    }
    e.endDay = endDay;
    calendarCopyTitle(e.title, item["summary"] | "(No title)", CALENDAR_TITLE_LEN);
    count++;
  }
  calendarDownloadedCount = count;
  return true;
}

// Runs in the background (net task). Returns true if the events were downloaded.
static bool downloadCalendar() {
  time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  char timeMin[24];
  snprintf(timeMin, sizeof(timeMin), "%04d-%02d-%02dT%02d:%02d:%02dZ", utc.tm_year + 1900, utc.tm_mon + 1,
           utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);

  // Events that haven't ended yet, soonest first, times in Israel time
  String url = "https://www.googleapis.com/calendar/v3/calendars/" + calendarUrlEncode(config.calendarId) +
               "/events?key=" + calendarUrlEncode(config.calendarKey) +
               "&timeMin=" + timeMin + "&maxResults=" + String(CALENDAR_MAX_EVENTS) +
               "&singleEvents=true&orderBy=startTime&timeZone=Asia%2FJerusalem"
               "&fields=items(summary,start,end)";

  NetworkClientSecure client;
  setupSecureClient(client, 8);   // handshake limit 8 s (default 120)
  HTTPClient http;
  if (!http.begin(client, url)) {
    calendarDownloadError = -1;
    Serial.println("[Calendar] Could not start the request");
    return false;
  }
  http.setTimeout(8000);
  int code = http.GET();
  String payload = code > 0 ? http.getString() : String();
  http.end();
  netReport(code);
  netLogFailure("Calendar", client, code);

  if (code != 200) {
    calendarDownloadError = code;
    // Google explains the problem in error.message (e.g. "API key not valid")
    JsonDocument error;
    String reason = HTTPClient::errorToString(code);  // for no connection, timeouts...
    if (code > 0) reason = deserializeJson(error, payload) ? "unknown error" : (error["error"]["message"] | "unknown error");
    Serial.printf("[Calendar] Error %d: %s\n", code, reason.c_str());
    return false;
  }
  if (!calendarParseEvents(payload)) {
    calendarDownloadError = 0;
    Serial.println("[Calendar] Could not read the reply");
    return false;
  }
  Serial.printf("[Calendar] Updated: %d upcoming event%s\n", calendarDownloadedCount, calendarDownloadedCount == 1 ? "" : "s");
  return true;
}

static BgJob calendarJob = {"calendar", downloadCalendar, BG_IDLE, false};

// In loop(). Checks every 10 min (every minute after an error).
// Returns true when the calendar screen or home card should be redrawn
// (new events, an error, or an event that just started or ended).
bool handleCalendar() {
  static unsigned long lastAttempt = 0;
  static bool attempted = false;
  static time_t lastCheck = 0;

  // every 30 s: did an event start ("14:00" -> "Now") or end (disappears)?
  bool eventChanged = false;
  if (calendarState == CALENDAR_READY && timeSynced()) {
    time_t now = time(NULL);
    if (now - lastCheck >= 30) {
      eventChanged = lastCheck != 0 && calendarChangedBetween(lastCheck, now);
      lastCheck = now;
    }
  }

  bool ok;
  if (bgTake(calendarJob, ok)) {  // a download finished
    if (ok) {
      memcpy(calendarEvents, calendarDownloaded, sizeof(CalendarEvent) * calendarDownloadedCount);
      calendarEventCount = calendarDownloadedCount;
      calendarState = CALENDAR_READY;
      calendarUpdatedAt = time(NULL);
      saveCalendar();
      return true;
    }
    calendarError = calendarDownloadError;
    // keep showing the events from the last check (offline mode); only report the error if there are none
    CalendarState before = calendarState;
    if (calendarState != CALENDAR_READY) calendarState = CALENDAR_FAILED;
    return calendarState != before || calendarState == CALENDAR_FAILED || eventChanged;
  }
  if (bgBusy(calendarJob)) return eventChanged;
  if (!homeWidgetShown(HW_NEXT)) return false;  // widget hidden: the feature is off, nothing to download

  if (!calendarConfigured()) {
    if (!attempted) {  // say it once
      attempted = true;
      Serial.println("[Calendar] Not set up: add the calendar ID and API key on the setup page (or in secrets.h)");
    }
    return false;
  }
  unsigned long wait = calendarState == CALENDAR_READY ? CALENDAR_REFRESH_MS : CALENDAR_RETRY_MS;
  // the Calendar screen was just opened: check now (but not more than every 15 s)
  bool checkNow = calendarCheckNow && (!attempted || millis() - lastAttempt >= 15000);
  calendarCheckNow = false;
  if (!checkNow && attempted && millis() - lastAttempt < wait) return eventChanged;
  // needs WiFi, and the time (to ask for events from now on)
  if (WiFi.status() != WL_CONNECTED || !timeSynced()) return eventChanged;
  attempted = true;
  lastAttempt = millis();

  if (checkNow) Serial.println("[Calendar] Calendar screen opened: checking for new events");
  bgRequest(calendarJob);
  return eventChanged;
}
#endif
