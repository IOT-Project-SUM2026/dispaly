#ifndef SETTINGS_H
#define SETTINGS_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../time_date/time_date.h"
#include "../idle/idle.h"
#include "../storage/storage.h"
#include "../theme/theme.h"

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

void onDarkModeChanged(lv_event_t* e) {
  setDarkMode(switchIsOn(e));  // restyles every screen in place: nothing is rebuilt
  Serial.printf("[Settings] %s mode\n", darkMode ? "Dark" : "Light");
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
// Settings are grouped in cards (DISPLAY, CLOCK, POWER SAVING); each
// card is a column of 44 px rows with a thin line between rows.
// Each section header is a tap target: tap it to fold the section away
// (the arrow turns from down to right) and tap again to open it. Only the
// section's card is hidden or shown, nothing is rebuilt. Folded sections
// stay folded while the display is on.
#define SETTINGS_SECTIONS 3
bool sectionOpen[SETTINGS_SECTIONS] = {true, true, true};

static void onSectionTap(lv_event_t* e) {
  lv_obj_t* header = (lv_obj_t*)lv_event_get_current_target(e);
  int i = (int)(intptr_t)lv_obj_get_user_data(header);
  sectionOpen[i] = !sectionOpen[i];
  lv_obj_t* group = lv_obj_get_sibling(header, 1);  // the card right after the header
  lv_obj_t* arrow = lv_obj_get_child(header, 1);
  if (sectionOpen[i]) lv_obj_remove_flag(group, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(group, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(arrow, sectionOpen[i] ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT);
}

lv_obj_t* createSettingsGroup(lv_obj_t* list, int index, const char* caption) {
  lv_obj_t* header = lv_obj_create(list);
  lv_obj_remove_style_all(header);
  lv_obj_set_size(header, LV_PCT(100), 32);
  lv_obj_set_style_radius(header, 8, 0);
  lv_obj_add_style(header, &stPressed, LV_STATE_PRESSED);
  lv_obj_set_ext_click_area(header, 6);  // 44 px tall tap area
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_user_data(header, (void*)(intptr_t)index);
  lv_obj_add_event_cb(header, onSectionTap, LV_EVENT_CLICKED, NULL);

  lv_obj_t* label = createCaption(header, caption);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_t* arrow = createLabel(header, sectionOpen[index] ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT, &stSecondary);
  lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -10, 0);

  lv_obj_t* group = createCard(list, 0, 0, LV_PCT(100), LV_SIZE_CONTENT, NULL);
  lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
  if (!sectionOpen[index]) lv_obj_add_flag(group, LV_OBJ_FLAG_HIDDEN);
  return group;
}

lv_obj_t* createSettingsRow(lv_obj_t* group, const char* text, int height = ROW_H) {
  lv_obj_t* row = lv_obj_create(group);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), height);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  if (lv_obj_get_child_count(group) > 1) createDivider(row, 12, 0, SCREEN_WIDTH - 2 * MARGIN - 24);
  lv_obj_t* label = createLabel(row, text, &stBody);
  lv_obj_set_pos(label, 12, height == ROW_H ? 14 : 12);
  return row;
}

void createSwitch(lv_obj_t* row, bool on, lv_event_cb_t onChange) {
  lv_obj_t* sw = lv_switch_create(row);
  lv_obj_set_size(sw, 48, 28);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -12, 0);
  lv_obj_add_style(sw, &stSwitchMain, LV_PART_MAIN);
  lv_obj_add_style(sw, &stSwitchOn, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_add_style(sw, &stSwitchKnob, LV_PART_KNOB);
  lv_obj_set_ext_click_area(sw, 8);  // 44 px tall tap area
  if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, onChange, LV_EVENT_VALUE_CHANGED, NULL);
}

void createRowDropdown(lv_obj_t* row, const char* options, int selected, lv_event_cb_t onChange) {
  lv_obj_t* dd = createDropdown(row, options, selected, onChange);
  lv_obj_set_size(dd, 116, 36);
  lv_obj_set_style_pad_ver(dd, 9, 0);
  lv_obj_align(dd, LV_ALIGN_RIGHT_MID, -4, 0);
}

lv_obj_t* buildSettingsScreen(lv_event_cb_t onBack) {
  lv_obj_t* scr = createScreen();
  createHeader(scr, "Settings", onBack);
  lv_obj_t* list = createScrollList(scr, MARGIN, CONTENT_Y, SCREEN_WIDTH - 2 * MARGIN, SCREEN_HEIGHT - CONTENT_Y, 4);

  // DISPLAY: brightness (label + value, slider below), dark mode
  lv_obj_t* group = createSettingsGroup(list, 0, "DISPLAY");
  lv_obj_t* row = createSettingsRow(group, "Brightness", 68);
  brightnessValueLabel = createLabel(row, "", &stSecondary);
  lv_obj_align(brightnessValueLabel, LV_ALIGN_TOP_RIGHT, -12, 12);
  showBrightnessValue(brightness);
  lv_obj_t* slider = lv_slider_create(row);
  lv_obj_set_size(slider, SCREEN_WIDTH - 2 * MARGIN - 40, 6);
  lv_obj_set_pos(slider, 20, 46);
  lv_slider_set_range(slider, 10, 255);  // never fully dark
  lv_slider_set_value(slider, brightness, LV_ANIM_OFF);
  lv_obj_add_style(slider, &stSliderMain, LV_PART_MAIN);
  lv_obj_add_style(slider, &stSliderIndicator, LV_PART_INDICATOR);
  lv_obj_add_style(slider, &stSliderKnob, LV_PART_KNOB);
  lv_obj_set_ext_click_area(slider, 18);  // easier to grab on a resistive screen
  lv_obj_add_event_cb(slider, onBrightnessChanged, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(slider, onBrightnessReleased, LV_EVENT_RELEASED, NULL);  // save once per drag

  row = createSettingsRow(group, "Dark mode");
  createSwitch(row, darkMode, onDarkModeChanged);

  // CLOCK
  group = createSettingsGroup(list, 1, "CLOCK");
  row = createSettingsRow(group, "24-hour time");
  createSwitch(row, use24h, on24hChanged);

  // POWER SAVING + its options (shown only when it's on)
  group = createSettingsGroup(list, 2, "POWER SAVING");
  row = createSettingsRow(group, "Power saving");
  createSwitch(row, idleEnabled, onIdleEnabledChanged);

  idleAfterRow = createSettingsRow(group, "Idle after");
  createRowDropdown(idleAfterRow, "1 min\n2 min\n5 min\n10 min\n15 min\n30 min", idleTimeoutIndex, onIdleAfterChanged);

  idleActionRow = createSettingsRow(group, "When idle");
  createRowDropdown(idleActionRow, "Dim\nScreen off", idleAction, onIdleActionChanged);

  idleDimRow = createSettingsRow(group, "Dim to");
  createRowDropdown(idleDimRow, "5%\n10%\n20%\n30%\n50%", idleDimIndex, onIdleDimChanged);

  updateIdleRows();
  return scr;
}
#endif
