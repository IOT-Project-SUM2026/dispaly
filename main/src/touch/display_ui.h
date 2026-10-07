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

int brightness = 200;

// The open screen - only changed by handleTouch(). setup() shows HOME first.
enum Screen { HOME, SETTINGS, WEATHER, TIME };
const char* screenNames[] = {"Home", "Settings", "Weather", "Time"};
Screen currentScreen = HOME;
int requestedScreen = -1;  // set by button callbacks, applied in handleTouch()

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// LVGL draw buffer: 1/10 of the screen, 16-bit color
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t drawBuf[DRAW_BUF_SIZE / 4];

// ============================================================
// Touch input - LVGL calls this to read the touchscreen
// ============================================================
void readTouch(lv_indev_t* indev, lv_indev_data_t* data) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
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
void onBack(lv_event_t* e) { requestedScreen = HOME; }

// Build the screen and replace the current one
void showScreen(Screen s) {
  lv_obj_t* scr = (s == HOME)     ? buildHomeScreen(onGear, onWeatherCard, onClock)
                : (s == SETTINGS) ? buildSettingsScreen(onBack)
                : (s == TIME)     ? buildTimeScreen(onBack)
                                  : buildWeatherScreen(onBack);
  lv_obj_t* old = lv_screen_active();
  lv_screen_load(scr);
  if (old) lv_obj_delete(old);
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

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, readTouch);

  initBrightness();
  showScreen(HOME);
  lv_refr_now(NULL);  // draw now - connectWiFi() can block for a few seconds
}

// ============================================================
// Rebuild the open screen (e.g. after new weather data)
// ============================================================
void refreshScreen() {
  if (currentScreen == HOME || currentScreen == WEATHER) showScreen(currentScreen);
}

// ============================================================
// Touch + screen switching (LVGL runs the button callbacks here)
// ============================================================
void handleTouch() {
  lv_timer_handler();

  if (requestedScreen >= 0) {
    Screen next = (Screen)requestedScreen;
    requestedScreen = -1;
    Serial.printf("[UI] %s -> %s\n", screenNames[currentScreen], screenNames[next]);
    currentScreen = next;
    showScreen(next);
  }
}
#endif
