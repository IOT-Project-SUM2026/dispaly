#ifndef CALENDAR_SCREEN_H
#define CALENDAR_SCREEN_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../time_date/time_date.h"
#include "calendar.h"
#include "../home/home_layout.h"

// ============================================================
// Calendar screen (upcoming events, grouped by day) and the
// home screen card (the next event).
// ============================================================

const char* const CAL_DAY_NAMES[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* const CAL_MONTH_NAMES[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                         "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// "Today", "Tomorrow" or e.g. "Fri 9 Oct"
void calendarDayLabel(long day, long today, char* out, size_t outSize) {
  if (day <= today) {
    snprintf(out, outSize, "Today");
  } else if (day == today + 1) {
    snprintf(out, outSize, "Tomorrow");
  } else {
    time_t t = (time_t)day * 86400L;
    struct tm date;
    gmtime_r(&t, &date);  // day is a date, not a moment, so no time zone here
    snprintf(out, outSize, "%s %d %s", CAL_DAY_NAMES[date.tm_wday], date.tm_mday, CAL_MONTH_NAMES[date.tm_mon]);
  }
}

// "14:00" / "2:00 PM", "Now" for an event that is on, "All day"
void calendarTimeLabel(const CalendarEvent& e, time_t now, char* out, size_t outSize) {
  if (e.allDay) {
    snprintf(out, outSize, "All day");
  } else if (e.start <= now) {
    snprintf(out, outSize, "Now");
  } else if (use24h) {
    snprintf(out, outSize, "%02d:%02d", e.startHour, e.startMinute);
  } else {
    int h = e.startHour % 12;
    snprintf(out, outSize, "%d:%02d %s", h == 0 ? 12 : h, e.startMinute, e.startHour < 12 ? "AM" : "PM");
  }
}

// Message for the states without events (returns false when there are events to show)
bool calendarStatusText(const char*& title, const char*& hint) {
  hint = nullptr;
  if (calendarState == CALENDAR_NOT_SET_UP) {
    title = "Calendar not set up";
    hint = "Add the calendar ID and API key in\nSettings > Device > Setup page";
  } else if (!timeSynced() || calendarState == CALENDAR_LOADING) {
    ConnectionState conn = connectionState();
    title = conn == CONN_OK ? "Loading events..." : conn == CONN_NO_INTERNET ? "No internet" : "No WiFi";
    hint = conn == CONN_OK ? nullptr : "Events load when the internet is back";
  } else if (calendarState == CALENDAR_FAILED) {
    title = "Couldn't load events";
    if (calendarError == 404) hint = "Check the calendar ID (Setup page)\nand that the calendar is public";
    else if (calendarError == 400 || calendarError == 403) hint = "Check the calendar API key\n(Settings > Device > Setup page)";
    else hint = "Trying again every minute";
  } else {
    return false;
  }
  return true;
}

// ============================================================
// Calendar screen: events grouped by day, 44 px rows
// [ 18:00 | Team meeting - Smart Display ... ]
// An event that is on now gets an accent bar on the left.
// ============================================================
lv_obj_t* buildCalendarScreen(lv_event_cb_t onBack) {
  lv_obj_t* scr = createScreen();
  createHeader(scr, "Calendar", onBack);
  const int centerY = CONTENT_Y + (SCREEN_HEIGHT - CONTENT_Y) / 2;

  const char* message;
  const char* hint;
  if (calendarStatusText(message, hint)) {
    bool problem = calendarState == CALENDAR_FAILED || calendarState == CALENDAR_NOT_SET_UP ||
                   WiFi.status() != WL_CONNECTED;
    createMessage(scr, problem ? LV_SYMBOL_WARNING : NULL, message, hint, centerY);
    return scr;
  }

  time_t now = time(NULL);
  struct tm local;
  localtime_r(&now, &local);
  long today = dayNumber(local);

  lv_obj_t* list = createScrollList(scr, MARGIN, CONTENT_Y, SCREEN_WIDTH - 2 * MARGIN, SCREEN_HEIGHT - CONTENT_Y, 4);

  long shownDay = -1;
  int shown = 0;
  for (int i = 0; i < calendarEventCount; i++) {
    const CalendarEvent& e = calendarEvents[i];
    if (calendarEventOver(e, now, today)) continue;
    long day = max(e.startDay, today);  // started earlier and still on: under Today

    if (day != shownDay) {
      char label[20];
      calendarDayLabel(day, today, label, sizeof(label));
      for (char* c = label; *c; c++) *c = toupper(*c);
      lv_obj_t* header = createCaption(list, label);
      lv_obj_set_style_pad_top(header, shown ? 8 : 2, 0);
      lv_obj_set_style_pad_left(header, 4, 0);
      shownDay = day;
    }

    // events aren't tapped, so rows can be shorter than a tap target: 36 px, 14 px text
    lv_obj_t* row = createCard(list, 0, 0, LV_PCT(100), 36, NULL, &stRow);
    bool isNow = !e.allDay && e.start <= now;
    if (isNow) lv_obj_add_style(row, &stNowBar, 0);
    char timeText[12], titleText[CALENDAR_TITLE_LEN];
    calendarTimeLabel(e, now, timeText, sizeof(timeText));
    lv_obj_t* when = createLabel(row, timeText, e.allDay ? &stSecondary : &stAccent);
    const int barShift = isNow ? 4 : 0;  // the bar pushes the content right: keep the columns lined up
    lv_obj_align(when, LV_ALIGN_LEFT_MID, 12 - barShift, 0);
    toScreenText(e.title, titleText, sizeof(titleText));
    lv_obj_t* what = createLabel(row, titleText, &stBody);
    makeSingleLine(what, SCREEN_WIDTH - 2 * MARGIN - 76 - 16);
    lv_obj_align(what, LV_ALIGN_LEFT_MID, 76 - barShift, 0);
    shown++;
  }
  if (shown == 0) {
    lv_obj_delete(list);
    createMessage(scr, LV_SYMBOL_OK, "No upcoming events", "Your calendar is clear", centerY);
    return scr;
  }
  // when the events were checked: red when older than an hour (e.g. the key stopped working)
  if (calendarUpdatedAt > 0) {
    struct tm t;
    localtime_r(&calendarUpdatedAt, &t);
    char timeBuf[8], ampmBuf[4], text[40], day[6] = "";
    formatClock(t, timeBuf, sizeof(timeBuf), ampmBuf, sizeof(ampmBuf));
    if (dayNumber(t) != today) strftime(day, sizeof(day), "%a ", &t);
    snprintf(text, sizeof(text), "Updated %s%s%s%s", day, timeBuf, ampmBuf[0] ? " " : "", ampmBuf);
    lv_obj_t* updated = createLabel(list, text, calendarStale() ? &stDanger : &stSmall);
    lv_obj_set_style_pad_top(updated, 4, 0);
    lv_obj_set_style_pad_left(updated, 4, 0);
  }
  return scr;
}

// ============================================================
// Home "Next event" widget (tap: Calendar screen). Any size (home_layout.h):
//   compact (under 64 px): when + title on two short lines
//   normal: NEXT / NOW, when, title (the original card)
//   wide: UPCOMING, one event per line (as many as fit)
// ============================================================

// "Today 18:00", "Tomorrow", "Until 15:30" (an event that is on now)
static void eventWhenText(const CalendarEvent& e, time_t now, long today, char* out, size_t size) {
  char day[20], timeText[12];
  calendarDayLabel(max(e.startDay, today), today, day, sizeof(day));
  calendarTimeLabel(e, now, timeText, sizeof(timeText));
  if (!e.allDay && e.start <= now) {
    struct tm end;
    localtime_r(&e.end, &end);
    char endText[8], ampm[4];
    formatClock(end, endText, sizeof(endText), ampm, sizeof(ampm));
    snprintf(out, size, "Until %s%s%s", endText, ampm[0] ? " " : "", ampm);
  } else if (e.allDay) {
    snprintf(out, size, "%s", day);
  } else {
    snprintf(out, size, "%s %s", day, timeText);
  }
}

lv_obj_t* createHomeCalendarCard(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t onClick) {
  lv_obj_t* card = createCard(parent, x, y, w, h, onClick);
  bool wide = w >= HOME_AREA_W, compact = h < 64;
  const char* message;
  const char* hint;
  bool hasEvents = !calendarStatusText(message, hint);

  time_t now = time(NULL);
  struct tm local;
  localtime_r(&now, &local);
  long today = dayNumber(local);
  const CalendarEvent* upcoming[CALENDAR_MAX_EVENTS];
  int count = 0, laterToday = 0;
  for (int i = 0; hasEvents && i < calendarEventCount; i++) {
    const CalendarEvent& e = calendarEvents[i];
    if (calendarEventOver(e, now, today)) continue;
    if (count > 0 && max(e.startDay, today) == today) laterToday++;
    upcoming[count++] = &e;
  }
  const CalendarEvent* next = count > 0 ? upcoming[0] : nullptr;
  bool isNow = next && !next->allDay && next->start <= now;

  // no events to show: "Not set up", "Can't load", "No events"...
  if (!next) {
    const char* text = !hasEvents ? (calendarState == CALENDAR_NOT_SET_UP ? "Not set up"
                                     : calendarState == CALENDAR_FAILED   ? "Can't load"
                                     : WiFi.status() != WL_CONNECTED      ? "No WiFi"
                                                                          : "Loading...")
                                  : "No events";
    if (compact) {
      lv_obj_t* label = createLabel(card, LV_SYMBOL_BELL "  ", &stCaption);
      lv_obj_align(label, LV_ALIGN_LEFT_MID, 12, 0);
      lv_obj_t* value = createLabel(card, text, &stSecondary);
      lv_obj_align(value, LV_ALIGN_LEFT_MID, 34, 0);
      return card;
    }
    lv_obj_t* title = createCaption(card, LV_SYMBOL_BELL "  NEXT");
    lv_obj_set_pos(title, 12, 10);
    lv_obj_t* label = createLabel(card, text, &stSecondary);
    lv_obj_set_pos(label, 12, 34);
    return card;
  }

  char when[36], titleText[CALENDAR_TITLE_LEN];
  if (compact) {  // when (accent) over the title
    eventWhenText(*next, now, today, when, sizeof(when));
    lv_obj_t* whenLabel = createLabel(card, when, &stAccent);
    lv_obj_set_pos(whenLabel, 12, h / 2 - 20);
    toScreenText(next->title, titleText, sizeof(titleText));
    lv_obj_t* titleLabel = createLabel(card, titleText, &stBody);
    makeSingleLine(titleLabel, w - 24);
    lv_obj_set_pos(titleLabel, 12, h / 2);
    return card;
  }

  if (wide) {  // one event per line: "Today 18:00   Team meeting"
    int firstY = 32;
    if (h >= 100) {
      lv_obj_t* title = createCaption(card, isNow ? LV_SYMBOL_BELL "  NOW AND NEXT" : LV_SYMBOL_BELL "  UPCOMING");
      lv_obj_set_pos(title, 12, 10);
    }
    int rows = max(1, (h - (h >= 100 ? 36 : 12)) / 22);
    if (h < 100) firstY = (h - min(rows, count) * 22) / 2 + 2;  // no caption: rows centered
    const int whenW = 140;
    for (int i = 0; i < count && i < rows; i++) {
      int ry = firstY + i * 22;
      eventWhenText(*upcoming[i], now, today, when, sizeof(when));
      lv_obj_t* whenLabel = createLabel(card, when, &stAccent);
      makeSingleLine(whenLabel, whenW - 8);
      lv_obj_set_pos(whenLabel, 12, ry);
      toScreenText(upcoming[i]->title, titleText, sizeof(titleText));
      lv_obj_t* titleLabel = createLabel(card, titleText, &stBody);
      makeSingleLine(titleLabel, w - 24 - whenW);
      lv_obj_set_pos(titleLabel, 12 + whenW, ry);
    }
    return card;
  }

  // the original card: NEXT / NOW, when, the title on up to 2 lines, "+2 more today"
  lv_obj_t* title = createCaption(card, isNow ? LV_SYMBOL_BELL "  NOW" : LV_SYMBOL_BELL "  NEXT");
  lv_obj_set_pos(title, 12, 10);
  eventWhenText(*next, now, today, when, sizeof(when));
  lv_obj_t* whenLabel = createLabel(card, when, &stAccent);
  lv_obj_set_pos(whenLabel, 12, 30);
  toScreenText(next->title, titleText, sizeof(titleText));
  lv_obj_t* titleLabel = createLabel(card, titleText, &stBody);
  makeSingleLine(titleLabel, w - 24, h >= 96 ? 2 : 1);
  lv_obj_set_pos(titleLabel, 12, 50);

  if (laterToday > 0 && h >= 100) {
    char more[24];
    snprintf(more, sizeof(more), "+%d more today", laterToday);
    lv_obj_t* moreLabel = createLabel(card, more, &stSmall);
    lv_obj_set_pos(moreLabel, 12, h - 24);
  }
  return card;
}
#endif
