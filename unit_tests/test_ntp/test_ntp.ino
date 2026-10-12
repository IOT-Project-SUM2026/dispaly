// ============================================================
// HW unit test: clock (NTP time sync + Israel time zone rules).
//
// What it checks
//   1. Time zone rule (no network needed): two fixed moments must
//      convert to the right Israel time, winter (UTC+2) and
//      summer (UTC+3, daylight saving).
//   2. NTP: after WiFi connects, the time syncs from the Technion
//      server (the course guide: other servers are blocked on the
//      Technion network), with pool.ntp.org / Google as backups.
//   3. Drift: the ESP32 clock is compared with the synced time
//      after 60 s (should be within 1 s).
//
// Network: same as test_wifi (empty = the display's saved network).
// ============================================================
#include <WiFi.h>
#include <time.h>

#define TEST_WIFI_SSID ""
#define TEST_WIFI_PASSWORD ""
#define LOCAL_TZ "IST-2IDT,M3.4.4/26,M10.5.0"  // same rule as the firmware
#define SYNC_LIMIT_MS 20000

bool checkZone(time_t utc, int expectHour, const char* label) {
  struct tm t;
  localtime_r(&utc, &t);
  bool ok = t.tm_hour == expectHour;
  Serial.printf("  %-28s -> %02d:%02d %s\n", label, t.tm_hour, t.tm_min, ok ? "ok" : "WRONG");
  return ok;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("[TEST] Clock: time zone rule + NTP sync");

  // 1. time zone rule
  setenv("TZ", LOCAL_TZ, 1);
  tzset();
  bool zoneOk = checkZone(1768478400, 14, "15 Jan 2026 12:00 UTC (winter)") &   // 14:00 IST
                checkZone(1784116800, 15, "15 Jul 2026 12:00 UTC (summer)");     // 15:00 IDT

  // 2. NTP
  WiFi.mode(WIFI_STA);
  if (strlen(TEST_WIFI_SSID) > 0) WiFi.begin(TEST_WIFI_SSID, TEST_WIFI_PASSWORD);
  else WiFi.begin();
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("  no WiFi - only the time zone part was tested");
    Serial.printf("RESULT: %s (time zone only)\n", zoneOk ? "PASS" : "FAIL");
    return;
  }
  configTzTime(LOCAL_TZ, "ntp.technion.ac.il", "pool.ntp.org", "time.google.com");
  start = millis();
  while (time(nullptr) < 1700000000 && millis() - start < SYNC_LIMIT_MS) delay(100);
  bool synced = time(nullptr) > 1700000000;
  if (synced) {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    char text[40];
    strftime(text, sizeof(text), "%A %d %b %Y %H:%M:%S", &t);
    Serial.printf("  synced in %lu ms: %s (%s)\n", millis() - start, text, t.tm_isdst ? "summer time" : "winter time");
  } else {
    Serial.println("  no time from any NTP server within 20 s");
  }

  // 3. drift over 60 s
  bool driftOk = false;
  if (synced) {
    time_t before = time(nullptr);
    uint32_t ms = millis();
    delay(60000);
    long counted = (millis() - ms) / 1000, passed = (long)(time(nullptr) - before);
    driftOk = labs(passed - counted) <= 1;
    Serial.printf("  60 s check: clock moved %ld s, millis() %ld s %s\n", passed, counted, driftOk ? "ok" : "DRIFT");
  }

  bool pass = zoneOk && synced && driftOk;
  Serial.printf("RESULT: %s\n", pass ? "PASS" : "FAIL");
}

void loop() {}
