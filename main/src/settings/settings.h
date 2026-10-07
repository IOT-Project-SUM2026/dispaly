#ifndef SETTINGS_H
#define SETTINGS_H
#include "../../globals.h"
#include "../../shared_ui.h"

#define BACKLIGHT_PIN 21
#define PWM_FREQ 5000
#define PWM_RES 8

void applyBrightness(int value) {
  ledcWrite(BACKLIGHT_PIN, value);
}

void initBrightness() {
  ledcAttach(BACKLIGHT_PIN, PWM_FREQ, PWM_RES);
  applyBrightness(brightness);
}

// ============================================================
// Settings screen
// ============================================================
lv_obj_t* brightnessValueLabel = NULL;

void showBrightnessValue(int value) {
  lv_label_set_text_fmt(brightnessValueLabel, "%d%%", (int)map(value, 0, 255, 0, 100));
}

void onBrightnessChanged(lv_event_t* e) {
  brightness = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
  applyBrightness(brightness);
  showBrightnessValue(brightness);

  // log at most every 300 ms so a drag doesn't flood the serial monitor
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 300) {
    lastLog = millis();
    Serial.printf("[Settings] Brightness %d%%\n", (int)map(brightness, 0, 255, 0, 100));
  }
}

lv_obj_t* buildSettingsScreen(lv_event_cb_t onBack) {
  lv_obj_t* scr = createScreen();

  createWiFiIcon(scr);
  createBackButton(scr, onBack);

  lv_obj_t* title = createLabel(scr, "Brightness", &lv_font_montserrat_28, UI_TEXT);
  lv_obj_set_pos(title, 20, 90);

  brightnessValueLabel = createLabel(scr, "", &lv_font_montserrat_14, UI_TEXT);
  lv_obj_align(brightnessValueLabel, LV_ALIGN_TOP_RIGHT, -10, 98);
  showBrightnessValue(brightness);

  lv_obj_t* sun = createSunIcon(scr, 16, UI_TEXT);
  lv_obj_set_pos(sun, 20, SLIDER_Y - 8);

  lv_obj_t* slider = lv_slider_create(scr);
  lv_obj_set_pos(slider, SLIDER_X, SLIDER_Y - 4);
  lv_obj_set_size(slider, SLIDER_W, 8);
  lv_slider_set_range(slider, 0, 255);
  lv_slider_set_value(slider, brightness, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider, UI_TRACK, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, UI_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, UI_TEXT, LV_PART_KNOB);
  lv_obj_set_style_border_color(slider, UI_ACCENT, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 4, LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);
  lv_obj_set_ext_click_area(slider, 20);  // easier to grab on a resistive screen
  lv_obj_add_event_cb(slider, onBrightnessChanged, LV_EVENT_VALUE_CHANGED, NULL);

  return scr;
}
#endif
