// ============================================================
// HW unit test: flash storage (NVS through Preferences), which the
// firmware uses for settings and for the shopping / to-do lists.
//
// What it checks
//   1. Write + read back an int, a string and a 2 KB block
//      (the lists are saved as one block of this size).
//   2. Write time of the block (the lists are saved after every change).
//   3. Data survives a reset: a boot counter is increased on every
//      start. Press the board's RST/EN button once and run again:
//      the counter must be one higher.
//
// Uses its own namespace ("unittest"), so the display's saved
// settings and lists are not touched.
// ============================================================
#include <Preferences.h>

#define BLOCK_SIZE 2048

Preferences prefs;
String summary;  // repeated in loop(), in case the Serial Monitor was opened late

void setup() {
  Serial.begin(115200);
  delay(3000);  // this test takes under a second: give time to open the Serial Monitor
  Serial.println();
  Serial.println("[TEST] Storage: flash (NVS / Preferences)");

  bool ok = prefs.begin("unittest", false);
  if (!ok) {
    Serial.println("  could not open NVS namespace");
    Serial.println("RESULT: FAIL");
    return;
  }

  // 3. boot counter (persistence across resets)
  uint32_t boots = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", boots);
  Serial.printf("  boot counter: %lu (press RST and check it goes up by 1)\n", boots);

  // 1. int + string
  int32_t number = (int32_t)esp_random();
  prefs.putInt("number", number);
  prefs.putString("text", "milk, eggs, \xD7\x97\xD7\x9C\xD7\x91");  // includes Hebrew (UTF-8)
  bool intOk = prefs.getInt("number", 0) == number;
  bool textOk = prefs.getString("text", "") == "milk, eggs, \xD7\x97\xD7\x9C\xD7\x91";
  Serial.printf("  int read back: %s, string (with Hebrew) read back: %s\n", intOk ? "ok" : "WRONG", textOk ? "ok" : "WRONG");

  // 1 + 2. 2 KB block, timed
  static uint8_t block[BLOCK_SIZE], back[BLOCK_SIZE];
  for (int i = 0; i < BLOCK_SIZE; i++) block[i] = (uint8_t)(i * 7 + boots);
  uint32_t start = micros();
  size_t written = prefs.putBytes("block", block, BLOCK_SIZE);
  uint32_t writeMs = (micros() - start) / 1000;
  size_t read = prefs.getBytes("block", back, BLOCK_SIZE);
  bool blockOk = written == BLOCK_SIZE && read == BLOCK_SIZE && memcmp(block, back, BLOCK_SIZE) == 0;
  Serial.printf("  %d byte block: write %lu ms, read back %s\n", BLOCK_SIZE, writeMs, blockOk ? "ok" : "WRONG");
  Serial.printf("  free NVS entries: %u\n", (unsigned)prefs.freeEntries());
  prefs.end();

  bool pass = intOk && textOk && blockOk;
  summary = String("boot counter ") + boots + ", int " + (intOk ? "ok" : "WRONG") + ", Hebrew string " +
            (textOk ? "ok" : "WRONG") + ", 2 KB block " + (blockOk ? "ok" : "WRONG") + " (write " + writeMs +
            " ms) -> RESULT: " + (pass ? "PASS" : "FAIL");
  Serial.printf("RESULT: %s (plus the reset check above)\n", pass ? "PASS" : "FAIL");
}

void loop() {
  delay(5000);
  if (summary.length()) Serial.println("  again: " + summary);
}
