#ifndef SETTINGS_H
#define SETTINGS_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../time_date/time_date.h"
#include "../idle/idle.h"
#include "../storage/storage.h"

#define BACKLIGHT_PIN 21
#define PWM_FREQ 5000
#define PWM_RES 8

void applyBrightness(int value) {
  ledcWrite(BACKLIGHT_PIN, value);
}

// Loads the saved brightness (at least 10 so a saved 0 can't leave the screen dark)
void initBrightness() {
  brightness = constrain(loadInt("brightness", 200), 10, 255);
  ledcAttach(BACKLIGHT_PIN, PWM_FREQ, PWM_RES);
  applyBrightness(brightness);
}

// ============================================================
// Settings screen - a scrollable list of rows:
// label on the left, control (slider / switch / dropdown) on the right
// ============================================================
lv_obj_t* brightnessValueLabel = NULL;
lv_obj_t* idleAfterRow = NULL;   // idle rows are shown only when idle mode is on
lv_obj_t* idleActionRow = NULL;
lv_obj_t* idleDimRow = NULL;

void showBrightnessValue(int value) {
  lv_label_set_text_fmt(brightnessValueLabel, "%d%%", (int)map(value, 0, 255, 0, 100));
}

void setRowVisible(lv_obj_t* row, bool visible) {
  if (visible) lv_obj_remove_flag(row, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
}

void updateIdleRows() {
  setRowVisible(idleAfterRow, idleEnabled);
  setRowVisible(idleActionRow, idleEnabled);
  setRowVisible(idleDimRow, idleEnabled && idleAction == IDLE_DIM);
}

// ---------- callbacks ----------
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

bool switchIsOn(lv_event_t* e) {
  return lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED);
}

int dropdownValue(lv_event_t* e) {
  return lv_dropdown_get_selected((lv_obj_t*)lv_event_get_target(e));
}

void onBrightnessReleased(lv_event_t* e) {
  saveInt("brightness", brightness);
}

void on24hChanged(lv_event_t* e) {
  use24h = switchIsOn(e);
  Serial.printf("[Settings] 24-hour time %s\n", use24h ? "on" : "off");
  saveInt("use24h", use24h);
}

void onIdleEnabledChanged(lv_event_t* e) {
  idleEnabled = switchIsOn(e);
  updateIdleRows();
  Serial.printf("[Settings] Power saving %s\n", idleEnabled ? "on" : "off");
  saveInt("idleOn", idleEnabled);
}

void onIdleAfterChanged(lv_event_t* e) {
  idleTimeoutIndex = dropdownValue(e);
  Serial.printf("[Settings] Idle after %d min\n", idleTimeoutMinutes[idleTimeoutIndex]);
  saveInt("idleAfter", idleTimeoutIndex);
}

void onIdleActionChanged(lv_event_t* e) {
  idleAction = dropdownValue(e);
  updateIdleRows();
  Serial.printf("[Settings] When idle: %s\n", idleAction == IDLE_DIM ? "dim" : "screen off");
  saveInt("idleAction", idleAction);
}

void onIdleDimChanged(lv_event_t* e) {
  idleDimIndex = dropdownValue(e);
  Serial.printf("[Settings] Dim to %d%%\n", idleDimPercents[idleDimIndex]);
  saveInt("idleDim", idleDimIndex);
}

// ---------- row building blocks ----------
lv_obj_t* createSettingsRow(lv_obj_t* list, const char* text) {
  lv_obj_t* row = createCard(list, 0, 0, LV_PCT(100), 44, 10, NULL);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_t* label = createLabel(row, text, &lv_font_montserrat_14, UI_TEXT);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 12, 0);
  return row;
}

void createSwitch(lv_obj_t* row, bool on, lv_event_cb_t onChange) {
  lv_obj_t* sw = lv_switch_create(row);
  lv_obj_set_size(sw, 44, 24);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -12, 0);
  lv_obj_set_style_bg_color(sw, UI_TRACK, LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw, UI_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw, UI_TEXT, LV_PART_KNOB);
  if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, onChange, LV_EVENT_VALUE_CHANGED, NULL);
}

void createRowDropdown(lv_obj_t* row, const char* options, int selected, lv_event_cb_t onChange) {
  lv_obj_t* dd = createDropdown(row, options, selected, onChange);
  lv_obj_set_size(dd, 112, 32);
  lv_obj_align(dd, LV_ALIGN_RIGHT_MID, -6, 0);
}

lv_obj_t* buildSettingsScreen(lv_event_cb_t onBack) {
  lv_obj_t* scr = createScreen();

  createWiFiIcon(scr);
  createBackButton(scr, onBack);

  lv_obj_t* title = createLabel(scr, "Settings", &lv_font_montserrat_20, UI_TEXT);
  lv_obj_set_pos(title, 60, 46);

  // scrollable list of rows
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_remove_style_all(list);
  lv_obj_set_pos(list, 12, 84);
  lv_obj_set_size(list, 296, 150);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_ACTIVE);

  // 1. brightness: sun icon, slider, value
  lv_obj_t* row = createSettingsRow(list, "");
  lv_obj_t* sun = createSunIcon(row, 16, UI_TEXT);
  lv_obj_align(sun, LV_ALIGN_LEFT_MID, 12, 0);
  lv_obj_t* slider = lv_slider_create(row);
  lv_obj_set_size(slider, 180, 8);
  lv_obj_align(slider, LV_ALIGN_LEFT_MID, 44, 0);
  lv_slider_set_range(slider, 0, 255);
  lv_slider_set_value(slider, brightness, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider, UI_TRACK, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, UI_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, UI_TEXT, LV_PART_KNOB);
  lv_obj_set_style_border_color(slider, UI_ACCENT, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 4, LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);
  lv_obj_set_ext_click_area(slider, 16);  // easier to grab on a resistive screen
  lv_obj_add_event_cb(slider, onBrightnessChanged, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(slider, onBrightnessReleased, LV_EVENT_RELEASED, NULL);  // save once per drag
  brightnessValueLabel = createLabel(row, "", &lv_font_montserrat_14, UI_TEXT);
  lv_obj_align(brightnessValueLabel, LV_ALIGN_RIGHT_MID, -12, 0);
  showBrightnessValue(brightness);

  // 2. 24-hour time
  row = createSettingsRow(list, "24-hour time");
  createSwitch(row, use24h, on24hChanged);

  // 3. power saving + its options
  row = createSettingsRow(list, "Power saving");
  createSwitch(row, idleEnabled, onIdleEnabledChanged);

  idleAfterRow = createSettingsRow(list, "Idle after");
  createRowDropdown(idleAfterRow, "1 min\n2 min\n5 min\n10 min\n15 min\n30 min", idleTimeoutIndex, onIdleAfterChanged);

  idleActionRow = createSettingsRow(list, "When idle");
  createRowDropdown(idleActionRow, "Dim\nScreen off", idleAction, onIdleActionChanged);

  idleDimRow = createSettingsRow(list, "Dim to");
  createRowDropdown(idleDimRow, "5%\n10%\n20%\n30%\n50%", idleDimIndex, onIdleDimChanged);

  updateIdleRows();
  return scr;
}
#endif
