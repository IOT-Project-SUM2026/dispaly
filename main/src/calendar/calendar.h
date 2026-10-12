#ifndef CALENDAR_H
#define CALENDAR_H
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../time_date/time_date.h"

#if __has_include("../../secrets.h")
#include "../../secrets.h"
#endif

// ============================================================
// Upcoming events from Google Calendar (Google Calendar API).
//
// Reads a public Google calendar with an API key, so the display
// needs no Google login: put CALENDAR_ID and CALENDAR_API_KEY in
// main/secrets.h (see README). Without them the calendar shows
// "Calendar not set up" and the rest works as usual.
// Checks every 10 minutes. No screen code here (calendar_screen.h).
// Runs in loop(), like the weather request.
// ============================================================

#ifndef CALENDAR_ID
#define CALENDAR_ID ""
#endif
#ifndef CALENDAR_API_KEY
#define CALENDAR_API_KEY ""
#endif

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

bool calendarConfigured() {
  return strlen(CALENDAR_ID) > 0 && strlen(CALENDAR_API_KEY) > 0 &&
         strncmp(CALENDAR_ID, "PASTE", 5) != 0 && strncmp(CALENDAR_API_KEY, "PASTE", 5) != 0;
}

CalendarEvent calendarEvents[CALENDAR_MAX_EVENTS];
int calendarEventCount = 0;
CalendarState calendarState = calendarConfigured() ? CALENDAR_LOADING : CALENDAR_NOT_SET_UP;
int calendarError = 0;  // HTTP code of the last failed request (negative = no connection)

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

// Fills calendarEvents from Google's reply. Returns false if the reply can't be read.
bool calendarParseEvents(const String& payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  JsonArray items = doc["items"];
  if (items.isNull()) return false;

  int count = 0;
  for (JsonObject item : items) {
    if (count >= CALENDAR_MAX_EVENTS) break;
    CalendarEvent& e = calendarEvents[count];
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
  calendarEventCount = count;
  return true;
}

// Returns true if the events were updated
static bool fetchCalendar() {
  time_t now = time(nullptr);
  struct tm utc;
  gmtime_r(&now, &utc);
  char timeMin[24];
  snprintf(timeMin, sizeof(timeMin), "%04d-%02d-%02dT%02d:%02d:%02dZ", utc.tm_year + 1900, utc.tm_mon + 1,
           utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);

  // Events that haven't ended yet, soonest first, times in Israel time
  String url = "https://www.googleapis.com/calendar/v3/calendars/" + calendarUrlEncode(CALENDAR_ID) +
               "/events?key=" + calendarUrlEncode(CALENDAR_API_KEY) +
               "&timeMin=" + timeMin + "&maxResults=" + String(CALENDAR_MAX_EVENTS) +
               "&singleEvents=true&orderBy=startTime&timeZone=Asia%2FJerusalem"
               "&fields=items(summary,start,end)";

  NetworkClientSecure client;
  client.setInsecure();           // encrypted, certificate not checked (same as the weather and Firebase requests)
  client.setHandshakeTimeout(8);  // seconds; the default (120 s) could freeze the screen if WiFi drops
  HTTPClient http;
  if (!http.begin(client, url)) {
    calendarError = -1;
    Serial.println("[Calendar] Could not start the request");
    return false;
  }
  http.setTimeout(8000);
  int code = http.GET();
  String payload = code > 0 ? http.getString() : String();
  http.end();

  if (code != 200) {
    calendarError = code;
    // Google explains the problem in error.message (e.g. "API key not valid")
    JsonDocument error;
    String reason = HTTPClient::errorToString(code);  // for no connection, timeouts...
    if (code > 0) reason = deserializeJson(error, payload) ? "unknown error" : (error["error"]["message"] | "unknown error");
    Serial.printf("[Calendar] Error %d: %s\n", code, reason.c_str());
    return false;
  }
  if (!calendarParseEvents(payload)) {
    calendarError = 0;
    Serial.println("[Calendar] Could not read the reply");
    return false;
  }
  Serial.printf("[Calendar] Updated: %d upcoming event%s\n", calendarEventCount, calendarEventCount == 1 ? "" : "s");
  return true;
}

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

  if (!calendarConfigured()) {
    if (!attempted) {  // say it once
      attempted = true;
      Serial.println("[Calendar] Not set up: add CALENDAR_ID and CALENDAR_API_KEY to secrets.h");
    }
    return false;
  }
  unsigned long wait = calendarState == CALENDAR_READY ? CALENDAR_REFRESH_MS : CALENDAR_RETRY_MS;
  // the Calendar screen was just opened: check now (but not more than every 15 s)
  bool checkNow = calendarCheckNow && (!attempted || millis() - lastAttempt >= 15000);
  calendarCheckNow = false;
  if (!checkNow) {
    if (attempted && millis() - lastAttempt < wait) return eventChanged;
    // a request blocks the screen for about a second: refreshes wait until it hasn't been touched for 10 s
    if (calendarState == CALENDAR_READY && lv_display_get_inactive_time(NULL) < 10000) return eventChanged;
  }
  // needs WiFi, and the time (to ask for events from now on)
  if (WiFi.status() != WL_CONNECTED || !timeSynced()) return eventChanged;
  attempted = true;
  lastAttempt = millis();

  if (checkNow) Serial.println("[Calendar] Calendar screen opened: checking for new events");
  CalendarState before = calendarState;
  if (fetchCalendar()) {
    calendarState = CALENDAR_READY;
    return true;
  }
  // keep showing the events from the last check (offline mode); only report the error if there are none
  if (calendarState != CALENDAR_READY) calendarState = CALENDAR_FAILED;
  return calendarState != before || eventChanged;
}
#endif
