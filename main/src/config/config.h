#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>
#include <Preferences.h>

#if __has_include("../../secrets.h")
#include "../../secrets.h"
#endif

// ============================================================
// Firebase login and Google Calendar settings.
//
// Set from the phone, on the "Setup" page of the setup portal
// (SmartDisplay-Setup, or Settings > Device > Setup page while on WiFi).
// Saved in flash. secrets.h only gives the starting values, so a new
// user never has to edit code. Changing them restarts the display.
// ============================================================

#ifndef FIREBASE_DATABASE_URL
#define FIREBASE_DATABASE_URL ""
#endif
#ifndef FIREBASE_API_KEY
#define FIREBASE_API_KEY ""
#endif
#ifndef FIREBASE_USER_EMAIL
#define FIREBASE_USER_EMAIL ""
#endif
#ifndef FIREBASE_USER_PASSWORD
#define FIREBASE_USER_PASSWORD ""
#endif
#ifndef CALENDAR_ID
#define CALENDAR_ID ""
#endif
#ifndef CALENDAR_API_KEY
#define CALENDAR_API_KEY ""
#endif

struct DeviceConfig {
  char dbUrl[120];
  char apiKey[64];
  char email[64];
  char password[64];
  char calendarId[128];
  char calendarKey[64];
};
DeviceConfig config;

// "PASTE_..." in secrets.example.h means "not filled in"
bool configValueSet(const char* value) {
  return value[0] != '\0' && strncmp(value, "PASTE", 5) != 0;
}

bool firebaseConfigured() {
  return configValueSet(config.dbUrl) && configValueSet(config.apiKey) &&
         configValueSet(config.email) && configValueSet(config.password);
}

bool calendarConfigured() {
  return configValueSet(config.calendarId) && configValueSet(config.calendarKey);
}

static void loadConfigValue(Preferences& p, const char* key, char* out, size_t size, const char* fallback) {
  String value = p.isKey(key) ? p.getString(key, "") : String(fallback);
  value.trim();
  strncpy(out, value.c_str(), size - 1);
  out[size - 1] = '\0';
}

// Call once in setup(), before Firebase and the calendar start
void initConfig() {
  Preferences p;
  p.begin("config", true);  // read-only (fails on the very first boot: defaults are used)
  loadConfigValue(p, "dbUrl", config.dbUrl, sizeof(config.dbUrl), FIREBASE_DATABASE_URL);
  loadConfigValue(p, "apiKey", config.apiKey, sizeof(config.apiKey), FIREBASE_API_KEY);
  loadConfigValue(p, "email", config.email, sizeof(config.email), FIREBASE_USER_EMAIL);
  loadConfigValue(p, "password", config.password, sizeof(config.password), FIREBASE_USER_PASSWORD);
  loadConfigValue(p, "calId", config.calendarId, sizeof(config.calendarId), CALENDAR_ID);
  loadConfigValue(p, "calKey", config.calendarKey, sizeof(config.calendarKey), CALENDAR_API_KEY);
  p.end();
  size_t len = strlen(config.dbUrl);
  if (len > 0 && config.dbUrl[len - 1] == '/') config.dbUrl[len - 1] = '\0';
  Serial.printf("[Config] Voice commands %s, calendar %s\n",
                firebaseConfigured() ? "set up" : "NOT set up", calendarConfigured() ? "set up" : "NOT set up");
}

// Saves one value typed on the setup page. Empty = keep the saved one
// (secret fields are never shown on the page, so they are left empty
// unless the user wants to change them). Returns true if it changed.
bool saveConfigValue(const char* key, const char* typed, char* current, size_t size) {
  String value = typed;
  value.trim();
  if (value.isEmpty()) return false;
  Preferences p;
  if (!p.begin("config", false)) return false;
  // Also saved when it equals the value from secrets.h: then the display keeps
  // it in flash and secrets.h can be deleted.
  if (p.isKey(key) && p.getString(key, "") == value) {
    p.end();
    return false;
  }
  bool ok = p.putString(key, value) > 0;
  p.end();
  if (!ok) {
    Serial.printf("[Config] Could not save %s to flash\n", key);
    return false;
  }
  bool changed = value != current;
  strncpy(current, value.c_str(), size - 1);
  current[size - 1] = '\0';
  return changed;  // only a new value needs a restart
}
#endif
