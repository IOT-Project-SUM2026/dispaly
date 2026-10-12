// ============================================================
// HW unit test: resistive touch (XPT2046 on VSPI) + accuracy.
//
// What it checks
//   1. The touch controller answers (IRQ line and readings).
//   2. Accuracy: tap 5 targets (4 corners + center). The test
//      converts the raw reading exactly like the firmware
//      (map(x, 200, 3700, ...), map(y, 240, 3800, ...)) and
//      measures how far the tap lands from the target.
//   3. Prints the raw values, so the mapping can be re-calibrated.
//
// PASS: every target within TOLERANCE_PX (the firmware's buttons are
// at least 44 px, so 20 px of error still hits the right button).
// Use a fingernail or stylus and press firmly, like a real user.
// ============================================================
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
#define BACKLIGHT_PIN 21

#define TOLERANCE_PX 20
#define WAIT_PER_TARGET_MS 20000
#define SAMPLES 8

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen touch(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft;

struct Target { int x, y; const char* name; };
const Target targets[] = {
  {20, 20, "top-left"}, {300, 20, "top-right"}, {160, 120, "center"},
  {20, 220, "bottom-left"}, {300, 220, "bottom-right"},
};
const int TARGET_COUNT = sizeof(targets) / sizeof(targets[0]);

void drawTarget(const Target& t) {
  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(t.x, t.y, 10, TFT_WHITE);
  tft.drawFastHLine(t.x - 15, t.y, 31, TFT_RED);
  tft.drawFastVLine(t.x, t.y - 15, 31, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(String("Tap the ") + t.name + " cross", 160, t.y < 120 ? 170 : 70, 2);
}

// Waits for a press, averages SAMPLES readings, waits for release
bool readTap(int& rawX, int& rawY, int& z) {
  uint32_t start = millis();
  while (!touch.touched()) {
    if (millis() - start > WAIT_PER_TARGET_MS) return false;
    delay(5);
  }
  delay(40);  // let the reading settle
  long sx = 0, sy = 0, sz = 0;
  int n = 0;
  for (int i = 0; i < SAMPLES && touch.touched(); i++) {
    TS_Point p = touch.getPoint();
    sx += p.x; sy += p.y; sz += p.z; n++;
    delay(10);
  }
  while (touch.touched()) delay(5);
  delay(150);
  if (n == 0) return false;
  rawX = sx / n; rawY = sy / n; z = sz / n;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("[TEST] Touch: XPT2046 (VSPI) accuracy");

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.invertDisplay(true);
  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);

  int worst = 0, failed = 0;
  for (int i = 0; i < TARGET_COUNT; i++) {
    const Target& t = targets[i];
    drawTarget(t);
    int rx, ry, z;
    if (!readTap(rx, ry, z)) {
      Serial.printf("  %-12s : no touch within %d s -> FAIL\n", t.name, WAIT_PER_TARGET_MS / 1000);
      failed++;
      continue;
    }
    // same conversion as main/src/touch/display_ui.h
    int x = constrain(map(rx, 200, 3700, 1, 320), 0, 319);
    int y = constrain(map(ry, 240, 3800, 1, 240), 0, 239);
    int err = (int)round(sqrt((double)(x - t.x) * (x - t.x) + (double)(y - t.y) * (y - t.y)));
    worst = max(worst, err);
    bool ok = err <= TOLERANCE_PX;
    if (!ok) failed++;
    Serial.printf("  %-12s : target (%3d,%3d)  tap (%3d,%3d)  raw (%4d,%4d) z=%4d  error %2d px %s\n",
                  t.name, t.x, t.y, x, y, rx, ry, z, err, ok ? "ok" : "FAIL");
    tft.fillCircle(x, y, 4, ok ? TFT_GREEN : TFT_RED);
    delay(400);
  }

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(failed ? TFT_RED : TFT_GREEN, TFT_BLACK);
  tft.drawString(failed ? "TOUCH TEST FAILED" : "TOUCH TEST PASSED", 160, 110, 4);
  Serial.printf("  worst error: %d px (limit %d px)\n", worst, TOLERANCE_PX);
  Serial.println("  If it fails: use the raw values above to adjust the map() limits in display_ui.h");
  Serial.printf("RESULT: %s\n", failed ? "FAIL" : "PASS");
}

void loop() {}
