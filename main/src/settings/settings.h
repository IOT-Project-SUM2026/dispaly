#ifndef SETTINGS_H
#define SETTINGS_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "../time_date/time_date.h"
#include "../idle/idle.h"
#include "../storage/storage.h"
#include "../theme/theme.h"
#include "../net/net.h"
#include "../wifi/wifi_setup.h"
#include "../health/health.h"
#include "../home/home_layout.h"

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
#define SETTINGS_SECTIONS 5
bool sectionOpen[SETTINGS_SECTIONS] = {true, true, true, false, true};  // DEVICE starts folded

// Loads which sections were folded (call after initStorage)
void initSettings() {
  int mask = loadInt("sections", -1);
  if (mask < 0) return;  // never changed: defaults above
  for (int k = 0; k < SETTINGS_SECTIONS; k++) sectionOpen[k] = (mask >> k) & 1;
}

static void onSectionTap(lv_event_t* e) {
  lv_obj_t* header = (lv_obj_t*)lv_event_get_current_target(e);
  int i = (int)(intptr_t)lv_obj_get_user_data(header);
  sectionOpen[i] = !sectionOpen[i];
  int mask = 0;  // remembered after a restart
  for (int k = 0; k < SETTINGS_SECTIONS; k++) mask |= sectionOpen[k] << k;
  saveInt("sections", mask);
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

void createSwitch(lv_obj_t* row, bool on, lv_event_cb_t onChange, void* userData = NULL) {
  lv_obj_t* sw = lv_switch_create(row);
  lv_obj_set_size(sw, 48, 28);
  lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -12, 0);
  lv_obj_add_style(sw, &stSwitchMain, LV_PART_MAIN);
  lv_obj_add_style(sw, &stSwitchOn, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_add_style(sw, &stSwitchKnob, LV_PART_KNOB);
  lv_obj_set_ext_click_area(sw, 8);  // 44 px tall tap area
  if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, onChange, LV_EVENT_VALUE_CHANGED, userData);
}

void createRowDropdown(lv_obj_t* row, const char* options, int selected, lv_event_cb_t onChange) {
  lv_obj_t* dd = createDropdown(row, options, selected, onChange);
  lv_obj_set_size(dd, 116, 36);
  lv_obj_set_style_pad_ver(dd, 9, 0);
  lv_obj_align(dd, LV_ALIGN_RIGHT_MID, -4, 0);
}

// ---------- HOME SCREEN section: show / hide each widget, one Big widget ----------
// Home is rebuilt when it opens again, so changes show when leaving Settings.
void onWidgetShownChanged(lv_event_t* e) {
  int widget = (int)(intptr_t)lv_event_get_user_data(e);
  if (switchIsOn(e)) homeShownMask |= (1 << widget);
  else homeShownMask &= ~(1 << widget);
  Serial.printf("[Settings] Home: %s %s\n", homeWidgetNames[widget], switchIsOn(e) ? "shown" : "hidden");
  saveInt("homeShown", homeShownMask);
}

void onGreetingChanged(lv_event_t* e) {
  homeGreetingShown = switchIsOn(e);
  Serial.printf("[Settings] Home: greeting %s\n", homeGreetingShown ? "shown" : "hidden");
  saveInt("homeGreet", homeGreetingShown);
}

void onBigWidgetChanged(lv_event_t* e) {
  homeBigWidget = dropdownValue(e) - 1;  // option 0 = None
  Serial.printf("[Settings] Home: big widget %s\n", homeBigWidget < 0 ? "none" : homeWidgetNames[homeBigWidget]);
  saveInt("homeBig", homeBigWidget);
}

void createHomeScreenSection(lv_obj_t* list) {
  lv_obj_t* group = createSettingsGroup(list, 4, "HOME SCREEN");
  lv_obj_t* greetingRow = createSettingsRow(group, "Greeting");
  createSwitch(greetingRow, homeGreetingShown, onGreetingChanged);
  for (int i = 0; i < HW_COUNT; i++) {
    lv_obj_t* row = createSettingsRow(group, homeWidgetNames[i]);
    createSwitch(row, homeWidgetShown(i), onWidgetShownChanged, (void*)(intptr_t)i);
  }
  lv_obj_t* row = createSettingsRow(group, "Big widget");
  createRowDropdown(row, "None\nClock\nWeather\nLists\nNext event", homeBigWidget + 1, onBigWidgetChanged);
  // how it works, in one line under the options
  row = createSettingsRow(group, "", 54);
  lv_obj_t* hint = createLabel(row, "Hidden widgets and their screens are off;\nthe others get their space. The big one\ngets most of it.", &stSmall);
  lv_obj_set_pos(hint, 12, 4);
}

// ---------- DEVICE section: status for the user (and for us when testing) ----------
// The values are updated every 2 s while the section is open (updateDeviceRows).
lv_obj_t* setupPageValue = NULL;
lv_obj_t* devInternetValue = NULL;
lv_obj_t* devVoiceValue = NULL;
lv_obj_t* devUptimeValue = NULL;
lv_obj_t* devMemoryValue = NULL;
lv_obj_t* devResetValue = NULL;
ConnectionState devShownConn = CONN_OFFLINE;  // the state the rows were built for
unsigned long resetArmedAt = 0;               // "Reset display" tapped once: a second tap within 5 s erases

// Value on the right of a row, e.g. "Connected" (red if it is a problem)
lv_obj_t* createRowValue(lv_obj_t* row, const char* text, bool problem = false, int width = 140) {
  lv_obj_t* value = createLabel(row, text, problem ? &stDanger : &stSecondary);
  makeSingleLine(value, width);  // long values (network name) end with "..."
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(value, LV_ALIGN_RIGHT_MID, -12, 0);
  return value;
}

static void setRowValue(lv_obj_t* value, const char* text, bool problem) {
  if (value == NULL) return;
  setLabelIfChanged(value, text);
  if (problem) lv_obj_add_style(value, &stDanger, 0);
  else lv_obj_remove_style(value, &stDanger, 0);
  lv_obj_align(value, LV_ALIGN_RIGHT_MID, -12, 0);
}

static const char* signalText(int rssi) {
  return rssi >= -60 ? "strong" : rssi >= -72 ? "good" : "weak";
}

void onSetupPageTap(lv_event_t* e) {
  String address = openSetupPage();
  if (setupPageValue == NULL) return;
  lv_label_set_text(setupPageValue, address.isEmpty() ? "Needs WiFi" : address.c_str());
}

// Erases everything saved on the display (WiFi, setup page, lists, settings), then restarts.
// For moving the display to another home or returning it.
void onResetTap(lv_event_t* e) {
  if (resetArmedAt == 0 || millis() - resetArmedAt > 5000) {
    resetArmedAt = millis();
    setRowValue(devResetValue, "Tap again to erase", true);
    return;
  }
  Serial.println("[Settings] Reset display: erasing saved data and WiFi, restarting");
  const char* spaces[] = {"config", "lists", "display", "weather", "calendar", "lastcmd"};
  for (const char* name : spaces) {
    Preferences p;
    if (p.begin(name, false)) { p.clear(); p.end(); }
  }
  wifiManager.resetSettings();
  delay(200);
  ESP.restart();
}

static void onDeviceGroupDeleted(lv_event_t* e) {
  setupPageValue = devInternetValue = devVoiceValue = devUptimeValue = devMemoryValue = devResetValue = NULL;
}

void createDeviceSection(lv_obj_t* list) {
  lv_obj_t* group = createSettingsGroup(list, 3, "DEVICE");
  lv_obj_add_event_cb(group, onDeviceGroupDeleted, LV_EVENT_DELETE, NULL);
  ConnectionState conn = connectionState();
  devShownConn = conn;
  resetArmedAt = 0;

  lv_obj_t* row = createSettingsRow(group, "Internet");
  devInternetValue = createRowValue(row, conn == CONN_OK ? "Connected" : conn == CONN_NO_INTERNET ? "No internet" : "No WiFi", conn != CONN_OK);

  row = createSettingsRow(group, "WiFi");
  char text[64];
  if (WiFi.status() == WL_CONNECTED) snprintf(text, sizeof(text), "%s, %s", WiFi.SSID().c_str(), signalText(WiFi.RSSI()));
  else snprintf(text, sizeof(text), "Not connected");
  createRowValue(row, text, WiFi.status() != WL_CONNECTED);
  if (WiFi.status() != WL_CONNECTED) {  // how to connect: the setup network and its password
    row = createSettingsRow(group, "Setup WiFi");
    createRowValue(row, SETUP_AP_NAME, false, 180);
    row = createSettingsRow(group, "Setup password");
    createRowValue(row, SETUP_AP_PASSWORD, false, 180);
  }

  row = createSettingsRow(group, "Voice commands");
  devVoiceValue = createRowValue(row, voiceStateText(), voiceState != VOICE_OK && voiceState != VOICE_STARTING);

  // Setup page: Firebase login and calendar from a phone on the same WiFi
  row = createSettingsRow(group, "Setup page");
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_style(row, &stPressed, LV_STATE_PRESSED);
  lv_obj_add_event_cb(row, onSetupPageTap, LV_EVENT_CLICKED, NULL);
  String address = setupPageOpen() ? "http://" + WiFi.localIP().toString() : String("Tap to open");
  setupPageValue = createLabel(row, address.c_str(), &stAccent);  // accent = "tap me"
  lv_obj_align(setupPageValue, LV_ALIGN_RIGHT_MID, -12, 0);
  row = createSettingsRow(group, "", 30);  // what to do with the address
  lv_obj_t* hint = createLabel(row, "Open the address on a phone on this WiFi", &stSmall);
  lv_obj_set_pos(hint, 12, 4);

  row = createSettingsRow(group, "Running for");
  uptimeText(text, sizeof(text));
  devUptimeValue = createRowValue(row, text);

  row = createSettingsRow(group, "Last restart");
  createRowValue(row, restartReasonText());

  row = createSettingsRow(group, "Free memory");
  snprintf(text, sizeof(text), "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  devMemoryValue = createRowValue(row, text);

  row = createSettingsRow(group, "Reset display");
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_style(row, &stPressed, LV_STATE_PRESSED);
  lv_obj_add_event_cb(row, onResetTap, LV_EVENT_CLICKED, NULL);
  devResetValue = createRowValue(row, "Erase all");
}

// In loop(), every 2 s: keeps the open Device section up to date
void updateDeviceRows() {
  static unsigned long lastUpdate = 0;
  if (devInternetValue == NULL || millis() - lastUpdate < 2000) return;
  lastUpdate = millis();
  ConnectionState conn = connectionState();
  if (conn != devShownConn) {  // other rows (WiFi, setup network) change too: rebuild Settings
    requestRefresh();
    return;
  }
  setRowValue(devVoiceValue, voiceStateText(), voiceState != VOICE_OK && voiceState != VOICE_STARTING);
  char text[24];
  uptimeText(text, sizeof(text));
  setRowValue(devUptimeValue, text, false);
  snprintf(text, sizeof(text), "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  setRowValue(devMemoryValue, text, false);
  if (setupPageValue && !setupPageOpen() && strncmp(lv_label_get_text(setupPageValue), "http", 4) == 0) {
    setLabelIfChanged(setupPageValue, "Tap to open");  // the page closed itself after 10 min
    lv_obj_align(setupPageValue, LV_ALIGN_RIGHT_MID, -12, 0);
  }
  if (resetArmedAt && millis() - resetArmedAt > 5000) {
    resetArmedAt = 0;
    setRowValue(devResetValue, "Erase all", false);
  }
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

  createHomeScreenSection(list);

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

  createDeviceSection(list);
  return scr;
}
#endif
