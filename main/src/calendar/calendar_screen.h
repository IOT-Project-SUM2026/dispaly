#ifndef CALENDAR_SCREEN_H
#define CALENDAR_SCREEN_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../time_date/time_date.h"
#include "calendar.h"

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
    hint = "Add the calendar ID and API key\nto secrets.h (see README)";
  } else if (!timeSynced() || calendarState == CALENDAR_LOADING) {
    bool online = WiFi.status() == WL_CONNECTED;
    title = online ? "Loading events..." : "No WiFi";
    hint = online ? nullptr : "Events load when WiFi is back";
  } else if (calendarState == CALENDAR_FAILED) {
    title = "Couldn't load events";
    if (calendarError == 404) hint = "Check CALENDAR_ID, and that\nthe calendar is public";
    else if (calendarError == 400 || calendarError == 403) hint = "Check CALENDAR_API_KEY";
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
  }
  return scr;
}

// ============================================================
// Home screen card: the next event
// ============================================================
lv_obj_t* createHomeCalendarCard(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t onClick) {
  lv_obj_t* card = createCard(parent, x, y, w, h, onClick);
  const char* message;
  const char* hint;
  bool hasEvents = !calendarStatusText(message, hint);

  time_t now = time(NULL);
  struct tm local;
  localtime_r(&now, &local);
  long today = dayNumber(local);
  const CalendarEvent* next = nullptr;
  int laterToday = 0;
  for (int i = 0; hasEvents && i < calendarEventCount; i++) {
    const CalendarEvent& e = calendarEvents[i];
    if (calendarEventOver(e, now, today)) continue;
    if (!next) next = &e;
    else if (max(e.startDay, today) == today) laterToday++;
  }
  bool isNow = next && !next->allDay && next->start <= now;

  lv_obj_t* title = createCaption(card, isNow ? LV_SYMBOL_BELL "  NOW" : LV_SYMBOL_BELL "  NEXT");
  lv_obj_set_pos(title, 12, 10);

  if (!hasEvents || !next) {
    const char* text = !hasEvents ? (calendarState == CALENDAR_NOT_SET_UP ? "Not set up"
                                     : calendarState == CALENDAR_FAILED   ? "Can't load"
                                     : WiFi.status() != WL_CONNECTED      ? "No WiFi"
                                                                          : "Loading...")
                                  : "No events";
    lv_obj_t* label = createLabel(card, text, &stSecondary);
    lv_obj_set_pos(label, 12, 34);
    return card;
  }

  // "Today 18:00" / "Tomorrow" / "Now", then the title on up to 2 lines
  char day[20], timeText[12], when[36], titleText[CALENDAR_TITLE_LEN];
  calendarDayLabel(max(next->startDay, today), today, day, sizeof(day));
  calendarTimeLabel(*next, now, timeText, sizeof(timeText));
  if (isNow) {
    struct tm end;
    localtime_r(&next->end, &end);
    char endText[8], ampm[4];
    formatClock(end, endText, sizeof(endText), ampm, sizeof(ampm));
    snprintf(when, sizeof(when), "Until %s%s%s", endText, ampm[0] ? " " : "", ampm);
  }
  else if (next->allDay) snprintf(when, sizeof(when), "%s", day);
  else snprintf(when, sizeof(when), "%s %s", day, timeText);
  lv_obj_t* whenLabel = createLabel(card, when, &stAccent);
  lv_obj_set_pos(whenLabel, 12, 30);

  toScreenText(next->title, titleText, sizeof(titleText));
  lv_obj_t* titleLabel = createLabel(card, titleText, &stBody);
  makeSingleLine(titleLabel, w - 24, 2);
  lv_obj_set_pos(titleLabel, 12, 50);

  if (laterToday > 0) {
    char more[24];
    snprintf(more, sizeof(more), "+%d more today", laterToday);
    lv_obj_t* moreLabel = createLabel(card, more, &stSmall);
    lv_obj_set_pos(moreLabel, 12, h - 24);
  }
  return card;
}
#endif
