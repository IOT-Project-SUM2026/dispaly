#ifndef HOME_H
#define HOME_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../weather/weather.h"
#include "../time_date/time_date.h"
#include "../lists/lists_screen.h"
#include "../calendar/calendar_screen.h"

// ============================================================
// Home screen (no header; 12 px margins, 8 px gaps):
//
//   12:30                    wifi  [gear]   y 0..44
//   Sunday 11 Oct          [ weather   ]   y 48..116
//  [ Lists        ]       [ Next event ]   y 124..232
//
// Every block opens its screen. When data changes only that
// card is rebuilt (refreshHomeCard), not the whole screen.
// ============================================================
#define HOME_TOP_Y    48
#define HOME_TOP_H    68
#define HOME_BOTTOM_Y 124
#define HOME_BOTTOM_H 108
#define HOME_RIGHT_X  (MARGIN + COL_W + GAP)

enum HomeCard { HOME_WEATHER, HOME_LISTS, HOME_CALENDAR, HOME_CARD_COUNT };
lv_obj_t* homeScreen = NULL;
lv_obj_t* homeCards[HOME_CARD_COUNT] = {NULL, NULL, NULL};
lv_event_cb_t homeCardClick[HOME_CARD_COUNT];

void onHomeScreenDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) != homeScreen) return;
  homeScreen = NULL;
  for (int i = 0; i < HOME_CARD_COUNT; i++) homeCards[i] = NULL;
}

// (Re)creates one card in its slot. The new card is created before
// the old one is deleted, so nothing flickers.
void refreshHomeCard(HomeCard which) {
  if (homeScreen == NULL) return;
  lv_obj_t* old = homeCards[which];
  lv_obj_t* card = NULL;
  switch (which) {
    case HOME_WEATHER:
      card = createHomeWeatherCard(homeScreen, HOME_RIGHT_X, HOME_TOP_Y, COL_W, HOME_TOP_H, homeCardClick[which]);
      break;
    case HOME_LISTS:
      card = createHomeListsCard(homeScreen, MARGIN, HOME_BOTTOM_Y, COL_W, HOME_BOTTOM_H, homeCardClick[which]);
      break;
    case HOME_CALENDAR:
      card = createHomeCalendarCard(homeScreen, HOME_RIGHT_X, HOME_BOTTOM_Y, COL_W, HOME_BOTTOM_H, homeCardClick[which]);
      break;
    default:
      return;
  }
  homeCards[which] = card;
  if (old) lv_obj_delete(old);
}

lv_obj_t* buildHomeScreen(lv_event_cb_t onGear, lv_event_cb_t onWeatherCard, lv_event_cb_t onClock,
                          lv_event_cb_t onListsCard, lv_event_cb_t onCalendarCard) {
  lv_obj_t* scr = createScreen();
  homeScreen = scr;
  lv_obj_add_event_cb(scr, onHomeScreenDeleted, LV_EVENT_DELETE, NULL);
  homeCardClick[HOME_WEATHER] = onWeatherCard;
  homeCardClick[HOME_LISTS] = onListsCard;
  homeCardClick[HOME_CALENDAR] = onCalendarCard;
  for (int i = 0; i < HOME_CARD_COUNT; i++) homeCards[i] = NULL;

  createHomeClock(scr, onClock);

  createIconButton(scr, LV_SYMBOL_SETTINGS, SCREEN_WIDTH - TOUCH_MIN, 0, onGear);
  lv_obj_t* wifi = createWiFiIcon(scr);
  lv_obj_align(wifi, LV_ALIGN_TOP_RIGHT, -TOUCH_MIN - 4, 14);

  for (int i = 0; i < HOME_CARD_COUNT; i++) refreshHomeCard((HomeCard)i);
  return scr;
}
#endif
