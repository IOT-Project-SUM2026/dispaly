# HW unit tests

Small stand-alone sketches that each check **one part** of the hardware or **one service**
the display depends on, without the rest of the firmware. Run them on a new board, after a
hardware problem, or when a feature stops working, to find out whether the cause is the
hardware, the network, a service, or our code.

Every test prints its measurements to the Serial Monitor and ends with one line:
`RESULT: PASS` or `RESULT: FAIL`.

## How to run a test

1. Arduino IDE → File → Open → `unit_tests/<test name>/<test name>.ino`.
2. Same board settings as the firmware: **ESP32 Dev Module**, Partition Scheme **Huge APP**,
   the board's COM port. The libraries are the ones the firmware already needs (see the main
   README), nothing extra.
3. Upload, open Tools → Serial Monitor at **115200** baud, press the board's **RST/EN**
   button to see the output from the start.

`test_display` and `test_touch` have their own `build_opt.h` with the same screen pins as
the firmware, so the screen works without editing the TFT_eSPI library.

Network tests (`test_wifi` and everything below it in the table) use the WiFi network the
display saved through its `SmartDisplay-Setup` portal. To test another network, type its
name and password into `TEST_WIFI_SSID` / `TEST_WIFI_PASSWORD` at the top of the sketch
(don't commit them).

`test_firebase` and `test_calendar` need the Firebase login / calendar key:
copy `main/secrets.h` into the test's folder. These copies are in `.gitignore`.
**Never upload `secrets.h` to GitHub**, also not by drag-and-drop on the website.

## The tests

Run them in this order: if a lower test fails, the ones above it are the first suspects.

| # | Test | Part | What it checks | PASS when |
|---|---|---|---|---|
| 1 | `test_display` | ILI9341 screen (HSPI) | full-screen fills in 5 colors (timed), 1 px border + corner marks, text in 3 sizes, colors not inverted | each fill ≤ 60 ms, all 4 edges and corners visible, colors correct |
| 2 | `test_touch` | XPT2046 touch (VSPI) | tap 5 targets (corners + center); converts like the firmware and prints the raw values | every tap within 20 px of its target (buttons are ≥ 44 px) |
| 3 | `test_backlight` | backlight PWM (GPIO 21) | 0 %, 4 %, 10 %, 25 %, 50 %, 75 %, 100 %, then a fade | every step brighter than the last, 0 % dark, no flicker (visual) |
| 4 | `test_storage` | flash (NVS) | int, Hebrew string, 2 KB block (size of the saved lists) written and read back, write time, boot counter | all values match; after RST the boot counter is one higher |
| 5 | `test_wifi` | WiFi radio | scan, connect time, signal (RSSI), DNS for every server the display uses, reconnect after a drop | connects and reconnects within 15 s, all names resolve |
| 6 | `test_ntp` | clock | Israel time zone rule (winter and summer), sync from `ntp.technion.ac.il`, drift over 60 s | both zone checks right, synced within 20 s, drift ≤ 1 s |
| 7 | `test_https_weather` | HTTPS / TLS | the real weather request (5 cities), time, memory used; a dead server must time out | HTTP 200 with 5 cities, dead server gives up in < 10 s |
| 8 | `test_firebase` | Firebase | sign-in, write / read ×5 / delete on `/unit_test` (real data untouched), read time | sign-in OK, value read back matches, delete OK |
| 9 | `test_calendar` | Google Calendar API | calendar ID + API key, fields the display uses, Hebrew titles | HTTP 200, events have a start time, Hebrew prints as Hebrew |

The parts of the board we don't use (RGB LED, light sensor, speaker, SD card) are not tested.

## Results

Fill in one row per run. The full report with procedures and the problems we found is
`HW_Unit_Tests_Report.docx` in the project's Drive folder.

| Date | Tester | Board | Test | Result | Measured |
|---|---|---|---|---|---|
| 12.10.2026 | Shada | CYD | test_display | PASS | fill 31 ms; border, corners, text correct |
| 12.10.2026 | Shada | CYD | test_touch | PASS (2nd run) | worst error 17 px (1st run failed: light presses) |
| 12.10.2026 | Shada | CYD | test_backlight | PASS | visual check correct |
| 12.10.2026 | Shada | CYD | test_storage | PASS | 2 KB write 11 ms, counter kept across restarts |
| 12.10.2026 | Shada | CYD | test_wifi | PASS (2nd run) | connect 1201 ms, RSSI -44 dBm, reconnect 1101 ms |
| 12.10.2026 | Shada | CYD | test_ntp | PASS | sync 19.6 s (home network), no drift in 60 s |
| 12.10.2026 | Shada | CYD | test_https_weather | PASS | request 1387 ms, TLS ~49 KB, dead server 8.0 s |
| 12.10.2026 | Shada | CYD | test_firebase | PASS | sign-in 1056 ms, average read 962 ms |
| 12.10.2026 | Shada | CYD | test_calendar | PASS | request 1209 ms, 5 events, Hebrew correct |
