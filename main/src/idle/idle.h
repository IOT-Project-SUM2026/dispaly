#ifndef IDLE_H
#define IDLE_H
#include "../../globals.h"
#include "../storage/storage.h"

// ============================================================
// Idle / power saving: after N minutes without touch, dim the
// backlight or turn it off. The first touch only wakes the screen.
// ============================================================
#define IDLE_DIM 0
#define IDLE_OFF 1

const int idleTimeoutMinutes[] = {1, 2, 5, 10, 15, 30};
const int idleDimPercents[] = {5, 10, 20, 30, 50};

bool idleEnabled = false;
int idleTimeoutIndex = 2;  // 5 min
int idleAction = IDLE_DIM;
int idleDimIndex = 1;      // 10%
bool isIdle = false;

// Load the saved power saving options (call after initStorage)
void initIdle() {
  idleEnabled = loadInt("idleOn", 0) != 0;
  idleTimeoutIndex = constrain(loadInt("idleAfter", 2), 0, 5);
  idleAction = constrain(loadInt("idleAction", IDLE_DIM), IDLE_DIM, IDLE_OFF);
  idleDimIndex = constrain(loadInt("idleDim", 1), 0, 4);
}

void wakeFromIdle() {
  if (!isIdle) return;
  isIdle = false;
  applyBrightness(brightness);
  lv_display_trigger_activity(NULL);  // restart the inactivity timer
  Serial.println("[Idle] Wake");
}

// Called by the touch reader. Returns true while a touch must be ignored:
// the touch that wakes the screen does not press anything.
bool idleSwallowTouch(bool touched) {
  static bool swallowing = false;
  if (!touched) {
    swallowing = false;
    return false;
  }
  if (isIdle) {
    wakeFromIdle();
    swallowing = true;
  }
  return swallowing;
}

void handleIdle() {
  if (!idleEnabled) {
    wakeFromIdle();
    return;
  }
  if (isIdle) return;

  uint32_t timeoutMs = idleTimeoutMinutes[idleTimeoutIndex] * 60000UL;
  if (lv_display_get_inactive_time(NULL) < timeoutMs) return;

  isIdle = true;
  if (idleAction == IDLE_DIM) {
    int percent = idleDimPercents[idleDimIndex];
    applyBrightness(brightness * percent / 100);
    Serial.printf("[Idle] Entering idle (dim to %d%%)\n", percent);
  } else {
    applyBrightness(0);
    Serial.println("[Idle] Entering idle (screen off)");
  }
}
#endif
