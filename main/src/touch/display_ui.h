#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../home/home.h"
#include "../weather/weather.h"
#include "../settings/settings.h"
#include "../time_date/time_date.h"
#include "../idle/idle.h"
#include "../lists/lists_screen.h"
#include "../calendar/calendar_screen.h"
#include "../firebase/firebase_sync.h"  // VoiceCommand

int brightness = 200;

// The open screen - only changed by handleTouch() and showVoiceCommand().
// setup() shows HOME first.
enum Screen { HOME, SETTINGS, WEATHER, TIME, LISTS, CALENDAR };
const char* screenNames[] = {"Home", "Settings", "Weather", "Time", "Lists", "Calendar"};
Screen currentScreen = HOME;
int requestedScreen = -1;  // set by button callbacks, applied in handleTouch()

#define VOICE_SCREEN_MS 30000     // a list opened by a voice command goes back home after this long untouched
bool returnHomeWhenIdle = false;

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// LVGL draw buffer: 1/10 of the screen (320 x 24 px), 16-bit color = 15 KB.
// One buffer is enough: lv_tft_espi sends pixels without DMA, so a second
// buffer couldn't be filled while the first is sent. A bigger buffer only
// saves a few flush calls per full-screen redraw (each ~2 ms at 55 MHz SPI)
// and costs RAM the TLS connections need.
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t drawBuf[DRAW_BUF_SIZE / 4];

// ============================================================
// Touch input - LVGL calls this to read the touchscreen
// ============================================================
void readTouch(lv_indev_t* indev, lv_indev_data_t* data) {
  bool touched = touchscreen.tirqTouched() && touchscreen.touched();
  if (idleSwallowTouch(touched)) {  // this touch only wakes the screen
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  if (touched) {
    TS_Point p = touchscreen.getPoint();
    int x = constrain(map(p.x, 200, 3700, 1, SCREEN_WIDTH), 0, SCREEN_WIDTH - 1);
    int y = constrain(map(p.y, 240, 3800, 1, SCREEN_HEIGHT), 0, SCREEN_HEIGHT - 1);
    // LVGL expects panel (portrait) coordinates and rotates them by 90 itself
    data->point.x = y;
    data->point.y = SCREEN_WIDTH - 1 - x;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ============================================================
// Button callbacks - only request the switch, handleTouch() does it
// ============================================================
void onGear(lv_event_t* e) { requestedScreen = SETTINGS; }
void onWeatherCard(lv_event_t* e) { requestedScreen = WEATHER; }
void onClock(lv_event_t* e) { requestedScreen = TIME; }
void onListsCard(lv_event_t* e) { requestedScreen = LISTS; }
void onCalendarCard(lv_event_t* e) { requestedScreen = CALENDAR; }
void onBack(lv_event_t* e) { requestedScreen = HOME; }

// A callback changed something on the open screen: rebuild it after the touch
void requestRefresh() { requestedScreen = currentScreen; }

// Build the screen and replace the current one
void showScreen(Screen s) {
  lv_obj_t* scr = (s == HOME)     ? buildHomeScreen(onGear, onWeatherCard, onClock, onListsCard, onCalendarCard)
                : (s == SETTINGS) ? buildSettingsScreen(onBack)
                : (s == TIME)     ? buildTimeScreen(onBack)
                : (s == LISTS)    ? buildListsScreen(onBack)
                : (s == CALENDAR) ? buildCalendarScreen(onBack)
                                  : buildWeatherScreen(onBack);
  lv_obj_t* old = lv_screen_active();
  lv_screen_load(scr);
  if (old) lv_obj_delete(old);
}

// Switch screens (or rebuild the open one)
void switchTo(Screen next) {
  if (next != currentScreen) {
    Serial.printf("[UI] %s -> %s\n", screenNames[currentScreen], screenNames[next]);
    if (currentScreen == LISTS) leaveListsScreen();  // removes a ticked item now
    if (next == CALENDAR) requestCalendarCheck();     // new events show up without waiting 10 min
  }
  currentScreen = next;
  showScreen(next);
}

// ============================================================
// Init
// ============================================================
void initDisplay() {
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  // panel is 240x320 portrait; rotate to 320x240 landscape
  lv_display_t* disp = lv_tft_espi_create(SCREEN_HEIGHT, SCREEN_WIDTH, drawBuf, sizeof(drawBuf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);

  initStyles();  // shared styles (theme.h), after lv_init()

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, readTouch);

  initBrightness();
  showScreen(HOME);
  lv_refr_now(NULL);  // draw now - connectWiFi() can block for a few seconds
}

// ============================================================
// New data arrived: update only what shows it
// ============================================================
enum DataKind { DATA_WEATHER, DATA_CALENDAR, DATA_LISTS, DATA_ALL };

void refreshScreen(DataKind changed = DATA_ALL) {
  switch (currentScreen) {
    case HOME:  // only the card that shows the data
      if (changed == DATA_WEATHER || changed == DATA_ALL) refreshHomeCard(HOME_WEATHER);
      if (changed == DATA_LISTS || changed == DATA_ALL) refreshHomeCard(HOME_LISTS);
      if (changed == DATA_CALENDAR || changed == DATA_ALL) refreshHomeCard(HOME_CALENDAR);
      break;
    case WEATHER:  // only the content below the header (the city picker stays)
      if (changed == DATA_WEATHER || changed == DATA_ALL) buildWeatherContent(lv_screen_active());
      break;
    case CALENDAR:
      if (changed == DATA_CALENDAR || changed == DATA_ALL) showScreen(CALENDAR);
      break;
    case LISTS:  // ticking updates the rows in place; voice commands rebuild via showVoiceCommand()
      if (changed == DATA_ALL) {
        rememberListsScroll();
        showScreen(LISTS);
      }
      break;
    default:
      break;  // settings and world clock show no downloaded data
  }
}

// ============================================================
// Voice commands: wake up, open the list, show the result
// ============================================================
lv_obj_t* toast = NULL;

void onToastTimer(lv_timer_t* t) {
  if (toast) lv_obj_delete(toast);
  toast = NULL;
}

// Message at the top for 3 s, e.g. "Added milk to your shopping list".
// On the top layer, so it stays while screens change; not clickable, so
// taps go through to the screen. Slides in (180 ms: only the toast's
// own area is redrawn, no transparency).
void showToast(const char* text, bool success) {
  if (toast) lv_obj_delete(toast);
  toast = lv_obj_create(lv_layer_top());
  lv_obj_add_style(toast, success ? &stAccentFill : &stDangerFill, 0);
  lv_obj_set_size(toast, SCREEN_WIDTH - 2 * 6, LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(toast, TOUCH_MIN, 0);
  lv_obj_set_style_radius(toast, 12, 0);
  lv_obj_set_style_border_width(toast, 0, 0);
  lv_obj_set_style_pad_hor(toast, 12, 0);
  lv_obj_set_style_pad_ver(toast, 8, 0);
  lv_obj_set_flex_flow(toast, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(toast, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(toast, 10, 0);
  lv_obj_remove_flag(toast, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(toast, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* icon = lv_label_create(toast);
  lv_label_set_text(icon, success ? LV_SYMBOL_OK : LV_SYMBOL_WARNING);
  lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);  // color comes from the fill style

  char shown[80];
  toScreenText(text, shown, sizeof(shown));
  lv_obj_t* label = lv_label_create(toast);
  lv_label_set_text(label, shown);
  lv_obj_add_style(label, &stBody, 0);  // Hebrew font + direction; the color comes from the toast
  lv_obj_add_style(label, success ? &stAccentFill : &stDangerFill, 0);  // text color on the fill
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);  // up to 2 lines instead of cutting it off
  lv_obj_set_flex_grow(label, 1);

  lv_obj_set_pos(toast, 6, 4);
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, toast);
  lv_anim_set_values(&a, -60, 4);
  lv_anim_set_duration(&a, 180);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) { lv_obj_set_y((lv_obj_t*)obj, v); });
  lv_anim_start(&a);

  lv_timer_t* timer = lv_timer_create(onToastTimer, 3000, NULL);
  lv_timer_set_repeat_count(timer, 1);
}

// Called from loop() when handleFirebase() got a command
void showVoiceCommand(const VoiceCommand& command) {
  wakeFromIdle();
  lv_display_trigger_activity(NULL);  // counts as activity: no sleep, 30 s until back home
  selectList(command.list, command.onList ? command.item : nullptr);
  if (currentScreen != LISTS) returnHomeWhenIdle = true;
  switchTo(LISTS);
  showToast(command.text, command.success);
}

// ============================================================
// Touch + screen switching (LVGL runs the button callbacks here)
// ============================================================
void handleTouch() {
  lv_timer_handler();

  if (requestedScreen >= 0) {
    Screen next = (Screen)requestedScreen;
    requestedScreen = -1;
    returnHomeWhenIdle = false;  // the user is using the screen
    switchTo(next);
  }

  // a list opened by a voice command goes back home if nobody touches it
  if (returnHomeWhenIdle && lv_display_get_inactive_time(NULL) > VOICE_SCREEN_MS) {
    returnHomeWhenIdle = false;
    if (currentScreen != HOME) switchTo(HOME);
  }
}
#endif
