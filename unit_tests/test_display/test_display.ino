// ============================================================
// HW unit test: screen (ILI9341, 320x240, HSPI) with TFT_eSPI only.
//
// What it checks
//   1. Full-screen fills in 5 colors, timed (SPI speed).
//   2. Geometry: a 1 px border on all 4 edges, corner marks,
//      center cross -> nothing cut off, not mirrored, not shifted.
//   3. Text in 3 sizes is readable.
//   4. Colors are not inverted (white looks white, red looks red).
//
// How to run: open this folder in Arduino IDE (build_opt.h next to
// this file holds the same screen config as the firmware), upload,
// open Serial Monitor at 115200 and watch the screen.
// ============================================================
#include <TFT_eSPI.h>

#define BACKLIGHT_PIN 21
#define FILL_LIMIT_MS 60  // 320x240x16 bit at 55 MHz is ~22 ms; 60 ms leaves room

TFT_eSPI tft;

struct Fill { uint16_t color; const char* name; };
const Fill fills[] = {
  {TFT_RED, "red"}, {TFT_GREEN, "green"}, {TFT_BLUE, "blue"}, {TFT_WHITE, "white"}, {TFT_BLACK, "black"},
};

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("[TEST] Display: ILI9341 320x240 (TFT_eSPI)");

  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  tft.init();
  tft.setRotation(1);       // landscape, same as the firmware
  tft.invertDisplay(true);  // this panel needs inversion for correct colors

  // 1. timed full-screen fills
  bool fillsOk = true;
  for (const Fill& f : fills) {
    uint32_t start = micros();
    tft.fillScreen(f.color);
    uint32_t ms = (micros() - start + 500) / 1000;
    Serial.printf("  fill %-5s : %lu ms %s\n", f.name, ms, ms <= FILL_LIMIT_MS ? "ok" : "TOO SLOW");
    if (ms > FILL_LIMIT_MS) fillsOk = false;
    delay(600);
  }

  // 2. geometry: border, corner marks, center cross
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, 320, 240, TFT_WHITE);                 // must touch all 4 edges
  tft.fillRect(0, 0, 20, 20, TFT_RED);                     // top-left: red
  tft.fillRect(300, 0, 20, 20, TFT_GREEN);                 // top-right: green
  tft.fillRect(0, 220, 20, 20, TFT_BLUE);                  // bottom-left: blue
  tft.fillRect(300, 220, 20, 20, TFT_YELLOW);              // bottom-right: yellow
  tft.drawFastHLine(150, 120, 21, TFT_WHITE);
  tft.drawFastVLine(160, 110, 21, TFT_WHITE);

  // 3. text in 3 sizes
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Font 2: Smart Display 0123", 28, 30, 2);
  tft.drawString("Font 4: 12:34", 28, 56, 4);
  tft.setTextDatum(BC_DATUM);
  tft.drawString("Bottom edge text", 160, 215, 4);

  Serial.println("  Check the screen now:");
  Serial.println("   - a white border touches all 4 edges (nothing cut off)");
  Serial.println("   - corners: red top-left, green top-right, blue bottom-left, yellow bottom-right");
  Serial.println("     (if red is top-right the image is mirrored; if colors differ, inversion is wrong)");
  Serial.println("   - the cross is in the middle, the text is sharp and readable");
  Serial.printf("RESULT (automatic part, fill speed): %s\n", fillsOk ? "PASS" : "FAIL");
  Serial.println("RESULT (visual part): write PASS/FAIL in the report after checking the list above");
}

void loop() {}
