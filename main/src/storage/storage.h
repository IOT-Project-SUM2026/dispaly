#ifndef STORAGE_H
#define STORAGE_H
#include <Preferences.h>

// ============================================================
// Settings saved in flash (ESP32 NVS), kept across reboots.
// Each feature loads its own keys at startup and saves on change.
// ============================================================
Preferences prefs;

void initStorage() {
  prefs.begin("display", false);
}

int loadInt(const char* key, int defaultValue) {
  return prefs.getInt(key, defaultValue);
}

void saveInt(const char* key, int value) {
  prefs.putInt(key, value);
  Serial.printf("[Storage] %s = %d\n", key, value);
}
#endif
