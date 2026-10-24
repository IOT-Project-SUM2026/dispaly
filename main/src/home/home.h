#ifndef HOME_H
#define HOME_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../storage/storage.h"
#include "home_layout.h"
#include "../weather/weather.h"
#include "../time_date/time_date.h"
#include "../lists/lists_screen.h"
#include "../calendar/calendar_screen.h"

// ============================================================
// Home screen (no header; 12 px margins, 8 px gaps):
//
//   Good afternoon            wifi  [gear]   y 0..44   (always)
//  [ Clock        ]       [ Weather    ]   y 48..116
//  [ Lists        ]       [ Next event ]   y 124..232
//
// That is the default. In Settings > HOME SCREEN any widget can be hidden
// (the others grow into its space) and one can be Big (a whole row and most
// of the height). home_layout.h decides where each widget goes; each widget
// draws a compact, normal or large version for the size it gets.
//
// Every widget opens its screen. When data changes only that widget is
// rebuilt (refreshHomeCard), not the whole screen.
// ============================================================

enum HomeCard { HOME_WEATHER = HW_WEATHER, HOME_LISTS = HW_LISTS, HOME_CALENDAR = HW_NEXT };

lv_obj_t* homeScreen = NULL;
lv_obj_t* homeCards[HW_COUNT] = {NULL, NULL, NULL, NULL};
lv_event_cb_t homeCardClick[HW_COUNT];
WidgetBox homeBoxes[HW_COUNT];

// Loads the saved widget choices (call after initStorage)
void initHomeLayout() {
  homeShownMask = loadInt("homeShown", 0x0F) & 0x0F;
  homeBigWidget = constrain(loadInt("homeBig", -1), -1, HW_COUNT - 1);
  homeGreetingShown = loadInt("homeGreet", 1) != 0;
}

void onHomeScreenDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) != homeScreen) return;
  homeScreen = NULL;
  for (int i = 0; i < HW_COUNT; i++) homeCards[i] = NULL;
}

// (Re)creates one widget in its place. The new one is created before
// the old one is deleted, so nothing flickers.
void refreshHomeCard(HomeCard which) {
  if (homeScreen == NULL || !homeBoxes[which].shown) return;
  const WidgetBox& b = homeBoxes[which];
  lv_obj_t* old = homeCards[which];
  lv_obj_t* card = NULL;
  switch (which) {
    case HOME_WEATHER:  card = createHomeWeatherCard(homeScreen, b.x, b.y, b.w, b.h, homeCardClick[which]); break;
    case HOME_LISTS:    card = createHomeListsCard(homeScreen, b.x, b.y, b.w, b.h, homeCardClick[which]); break;
    case HOME_CALENDAR: card = createHomeCalendarCard(homeScreen, b.x, b.y, b.w, b.h, homeCardClick[which]); break;
    default: return;
  }
  homeCards[which] = card;
  if (old) lv_obj_delete(old);
}

lv_obj_t* buildHomeScreen(lv_event_cb_t onGear, lv_event_cb_t onWeatherCard, lv_event_cb_t onClock,
                          lv_event_cb_t onListsCard, lv_event_cb_t onCalendarCard) {
  lv_obj_t* scr = createScreen();
  homeScreen = scr;
  lv_obj_add_event_cb(scr, onHomeScreenDeleted, LV_EVENT_DELETE, NULL);
  homeCardClick[HW_CLOCK] = onClock;
  homeCardClick[HW_WEATHER] = onWeatherCard;
  homeCardClick[HW_LISTS] = onListsCard;
  homeCardClick[HW_NEXT] = onCalendarCard;
  for (int i = 0; i < HW_COUNT; i++) homeCards[i] = NULL;
  computeHomeLayout(homeBoxes);

  // top row: greeting (or a connection problem), WiFi, gear
  createHomeTopRow(scr);
  createIconButton(scr, LV_SYMBOL_SETTINGS, SCREEN_WIDTH - TOUCH_MIN, 0, onGear);
  // with the greeting on, a connection problem is spelled out in the greeting row;
  // with it off (that space may hold a widget), next to the icon instead
  lv_obj_t* wifi = createWiFiIcon(scr, homeGreetingShown);
  lv_obj_align(wifi, LV_ALIGN_TOP_RIGHT, -TOUCH_MIN - 4, 14);

  if (!homeAnyWidgetShown()) {
    createMessage(scr, LV_SYMBOL_HOME, "All widgets are hidden", "Turn them on in\nSettings > Home screen",
                  HOME_AREA_Y + HOME_AREA_H / 2);
    updateHomeClock();
    return scr;
  }

  const WidgetBox& c = homeBoxes[HW_CLOCK];
  if (c.shown) createHomeClock(scr, c.x, c.y, c.w, c.h, onClock);
  for (int i = HW_WEATHER; i < HW_COUNT; i++) refreshHomeCard((HomeCard)i);
  updateHomeClock();
  return scr;
}
#endif
