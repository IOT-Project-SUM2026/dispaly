// ============================================================
// HW unit test: backlight brightness (PWM on GPIO 21).
//
// What it checks
//   The backlight follows the PWM duty cycle used by Settings ->
//   Brightness and by power saving (dim / screen off):
//   0% (off), 4% (the firmware's minimum), 10%, 25%, 50%, 75%, 100%.
//   Each step is held for 2 s. Then a smooth fade up and down.
//
// PASS (visual): every step is visibly brighter than the one before,
// 0% is fully dark, no flicker at any level.
// Uses the same PWM settings as main/src/settings/settings.h.
// ============================================================
#define BACKLIGHT_PIN 21
#define PWM_FREQ 5000
#define PWM_RES 8  // 0..255

const int percents[] = {0, 4, 10, 25, 50, 75, 100};

void setPercent(int percent) {
  ledcWrite(BACKLIGHT_PIN, map(percent, 0, 100, 0, 255));
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("[TEST] Backlight: PWM on GPIO 21");
  if (!ledcAttach(BACKLIGHT_PIN, PWM_FREQ, PWM_RES)) {
    Serial.println("  could not attach PWM to GPIO 21");
    Serial.println("RESULT: FAIL");
    return;
  }
  for (int p : percents) {
    Serial.printf("  brightness %3d%%\n", p);
    setPercent(p);
    delay(2000);
  }
  Serial.println("  fade 0 -> 100 -> 0 % (should be smooth, no flicker)");
  for (int v = 0; v <= 255; v++) { ledcWrite(BACKLIGHT_PIN, v); delay(8); }
  for (int v = 255; v >= 0; v--) { ledcWrite(BACKLIGHT_PIN, v); delay(8); }
  setPercent(100);
  Serial.println("  Check: each step brighter than the last, 0% fully dark, fade smooth.");
  Serial.println("RESULT (visual): write PASS/FAIL in the report");
}

void loop() {}
