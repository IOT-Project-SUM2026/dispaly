#ifndef HOME_H
#define HOME_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../weather/weather.h"
#include "../time_date/time_date.h"

// ============================================================
// Home screen
// ============================================================
lv_obj_t* buildHomeScreen(lv_event_cb_t onGear, lv_event_cb_t onWeatherCard, lv_event_cb_t onClock) {
  lv_obj_t* scr = createScreen();

  createWiFiIcon(scr);
  createGearButton(scr, onGear);

  createHomeClock(scr, onClock);

  // title: "Smart" + accent "Display", centered
  lv_obj_t* title = lv_obj_create(scr);
  lv_obj_remove_style_all(title);
  lv_obj_set_size(title, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(title, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(title, 6, 0);
  createLabel(title, "Smart", &lv_font_montserrat_20, UI_TEXT);
  createLabel(title, "Display", &lv_font_montserrat_20, UI_ACCENT);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 100);

  createShape(scr, 141, 128, 38, 3, 1, UI_ACCENT);

  createHomeWeatherCard(scr, onWeatherCard);
  return scr;
}
#endif
