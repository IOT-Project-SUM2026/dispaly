#ifndef HEALTH_H
#define HEALTH_H
#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <esp_system.h>
#include "../net/net.h"

// ============================================================
// Keeping the display healthy over days of running:
//  - watchdog: if loop() stops running for WATCHDOG_MS (a bug, a frozen
//    request), the board restarts by itself; settings and lists come back
//    from flash, so the restart is almost invisible
//  - why the board last restarted (power, watchdog, crash), shown in
//    Settings > Device and in the log
//  - a health line in the log every 10 min (free memory, lowest free
//    memory, signal), to spot slow memory loss in a long test
// ============================================================

// The longest loop() can block is a weather or calendar request that times
// out (now in the background), so 60 s only fires on a real freeze.
#define WATCHDOG_MS        60000
#define HEALTH_LOG_MS      600000  // 10 min

const char* restartReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "Power on";
    case ESP_RST_SW:       return "Restarted by the display";
    case ESP_RST_PANIC:    return "Crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "Watchdog (froze)";
    case ESP_RST_BROWNOUT: return "Weak power supply";
    case ESP_RST_EXT:      return "Reset button";
    default:               return "Other";
  }
}

// "3 d 4 h", "2 h 15 min", "12 min"
void uptimeText(char* out, size_t size) {
  unsigned long minutes = (unsigned long)(esp_timer_get_time() / 60000000ULL);  // doesn't wrap like millis()
  unsigned long days = minutes / 1440, hours = (minutes / 60) % 24, mins = minutes % 60;
  if (days > 0) snprintf(out, size, "%lu d %lu h", days, hours);
  else if (hours > 0) snprintf(out, size, "%lu h %lu min", hours, mins);
  else snprintf(out, size, "%lu min", mins);
}

// Restart-loop protection: counts crashes that happen soon after starting.
// Kept in RTC memory, which survives a restart but not a power cut.
#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif
#define CRASH_LOOP_LIMIT   3       // this many quick crashes in a row: start with default settings
#define CRASH_QUICK_MS     60000   // a crash within 1 min of starting counts as "quick"
RTC_NOINIT_ATTR static uint32_t crashMagic;
RTC_NOINIT_ATTR static uint32_t quickCrashes;

static bool restartWasCrash() {
  esp_reset_reason_t r = esp_reset_reason();
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
}

// Once at the start of setup(), before anything is loaded from flash
void initHealth() {
  Serial.printf("[Health] Last restart: %s\n", restartReasonText());
  if (crashMagic != 0x5AFE5AFE) { crashMagic = 0x5AFE5AFE; quickCrashes = 0; }  // first start after power-up
  quickCrashes = restartWasCrash() ? quickCrashes + 1 : 0;
  if (quickCrashes >= CRASH_LOOP_LIMIT) {
    // a saved setting or saved data may be what crashes the display: start from defaults
    // (WiFi, the setup page values and the lists are kept)
    Serial.printf("[Health] Crashed %d times right after starting: using default settings\n", (int)quickCrashes);
    const char* spaces[] = {"display", "weather", "calendar", "lastcmd"};
    for (const char* name : spaces) {
      Preferences p;
      if (p.begin(name, false)) { p.clear(); p.end(); }
    }
    quickCrashes = 0;
  }
}

// Once at the end of setup(): from now on loop() must call handleHealth() at least every WATCHDOG_MS
void startWatchdog() {
  esp_task_wdt_config_t wdt = {};
  wdt.timeout_ms = WATCHDOG_MS;
  wdt.idle_core_mask = 1 << 0;  // keep watching core 0's idle task, as the ESP32 core does by default
  wdt.trigger_panic = true;     // restart
  esp_task_wdt_reconfigure(&wdt);
  esp_task_wdt_add(NULL);       // this task (loop)
  Serial.printf("[Health] Watchdog on (%d s)\n", WATCHDOG_MS / 1000);
}

// In loop()
// Unused stack of a task in bytes (-1 = task not started)
static int stackLeft(TaskHandle_t task) {
  return task ? (int)uxTaskGetStackHighWaterMark(task) : -1;
}

void handleHealth() {
  esp_task_wdt_reset();
  if (quickCrashes && millis() > CRASH_QUICK_MS) quickCrashes = 0;  // ran for a minute: not a restart loop

  if (memoryRestartWanted) {  // net.h: memory too broken up for secure connections
    Serial.println("[Health] Restarting to free memory");
    delay(100);
    ESP.restart();
  }

  static unsigned long lastLog = 0;
  if (millis() - lastLog < HEALTH_LOG_MS) return;
  lastLog = millis();
  char up[24];
  uptimeText(up, sizeof(up));
  Serial.printf("[Health] Up %s, free memory %u bytes (lowest %u, largest block %u), WiFi %d dBm\n", up,
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getMaxAllocHeap(),
                WiFi.status() == WL_CONNECTED ? (int)WiFi.RSSI() : 0);
  // unused stack per task: under 1000 bytes means that task's stack should be raised
  Serial.printf("[Health] Stack left: loop %d, firebase %d, net %d bytes\n", stackLeft(xTaskGetCurrentTaskHandle()),
                stackLeft(firebaseTaskHandle), stackLeft(netTaskHandle));
}
#endif
