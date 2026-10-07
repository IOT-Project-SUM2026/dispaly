# dispaly

ESP32 touchscreen firmware for the Smart Display: home screen, weather + forecast,
settings (brightness), WiFi status. The UI is built with **LVGL 9**, using TFT_eSPI as the display driver.

## Overview

Part of **Smart Display** (IoT project course 236333, Technion, group 5) by
Shada Nujeidat, Yuval Carmelli and Liat Rokach. A smart display that shows personal
information (weather, time, calendar, lists) and receives voice commands sent from a
phone app through Gemini and Firebase. This repo is the ESP32 display firmware.

## Folder description

```
dispaly/
├── upload.bat                one-click build + upload (Windows)
├── Documentation/            install instructions, pinout, states & errors
└── main/                     Arduino sketch (open main/main.ino)
    ├── main.ino, *.h         general code: setup/loop, globals, shared LVGL widgets
    ├── build_opt.h           screen + LVGL configuration
    ├── sketch.yaml           pinned versions for Arduino CLI
    └── src/                  one folder per feature
        ├── home/             home screen
        ├── touch/            LVGL + display + touch init, screen switching
        ├── weather/          weather fetch + weather screen
        ├── settings/         settings screen + brightness
        └── wifi/             WiFi setup portal, connection status
```

- [Documentation/install_instructions.txt](Documentation/install_instructions.txt): step-by-step setup
- [Documentation/pinout_wiring.md](Documentation/pinout_wiring.md): CYD pins and further reading
- [Documentation/errors_and_states.md](Documentation/errors_and_states.md): what the display shows in each state

## Hardware

**ESP32-2432S028R** ("Cheap Yellow Display", CYD): ESP32 + 2.8" 320×240 ILI9341 screen
+ XPT2046 resistive touch, one micro-USB port.

## Quick start (Windows): `upload.bat`

1. Install **Arduino IDE 2** from arduino.cc (the script uses the `arduino-cli` inside it).
2. Plug in the board (install the **CH340** USB driver if no COM port shows up).
3. Double-click **`upload.bat`**, type the board's COM port when asked.

The script downloads the exact ESP32 board package and library versions from
`main/sketch.yaml` on first run, builds with the right partition scheme, uploads, and
can open the serial monitor. Nothing to install or configure by hand. Close Arduino IDE's
Serial Monitor first, since it holds the COM port. Run `upload.bat clean` for a full
rebuild after changing `build_opt.h`.

Prefer Arduino IDE? Follow the two sections below instead.

## One-time setup (per computer)

1. Install **Arduino IDE 2** from arduino.cc.
2. **Board package:** Tools → Board → Boards Manager → search `esp32` → install
   **esp32 by Espressif Systems**, version **3.3.12**.
3. **Libraries:** Tools → Manage Libraries, install these exact versions:

   | Library | Author | Version |
   |---|---|---|
   | TFT_eSPI | Bodmer | 2.5.43 |
   | lvgl | kisvegabor | 9.6.0 |
   | XPT2046_Touchscreen | Paul Stoffregen | 1.4.0 |
   | WiFiManager | tzapu | 2.0.17 |
   | ArduinoJson | Benoit Blanchon | 7.4.3 |

4. **USB driver:** plug in the board. If no new COM port appears under Tools → Port,
   install the **CH340** driver (the CYD's USB chip) and replug.

## Build and upload

1. Open **`main/main.ino`** (always open it from `main/`, see Troubleshooting).
2. Tools → Board → esp32 → **ESP32 Dev Module**.
3. Tools → Partition Scheme → **Huge APP (3MB No OTA/1MB SPIFFS)**. The firmware doesn't fit the default 1.3 MB app partition with LVGL.
4. Tools → Port → the board's COM port.
5. Click **Upload**. If it hangs on `Connecting...`, hold the **BOOT** button on the
   board until the upload starts.

The first build takes several minutes because the whole ESP32 core and LVGL are compiled.
Later builds only recompile your changes and are much faster.

## First boot: WiFi

The display starts right away and works offline. Until WiFi is set up it shows the offline WiFi icon and runs a setup portal in the background:

1. On your phone, join the WiFi network **`SmartDisplay-Setup`**.
2. A setup page opens (otherwise browse to `192.168.4.1`) → *Configure WiFi* →
   pick your network and enter the password.
3. The board connects (the WiFi icon updates within ~2 s) and the weather appears. It remembers
   the network, so later boots skip this step.

Logs: Tools → Serial Monitor at **115200** baud. See [errors_and_states.md](Documentation/errors_and_states.md) for the log messages.

## Project files

| File | Purpose |
|---|---|
| `main/main.ino` | `setup()` / `loop()` |
| `main/globals.h` | screen size, colors (`lv_color_hex`), UI positions |
| `main/shared_ui.h` | shared LVGL widgets: screen, label, card, shapes/sun icon, WiFi icon, back + gear buttons |
| `main/src/home/home.h` | `buildHomeScreen()` |
| `main/src/touch/display_ui.h` | LVGL init (display via `lv_tft_espi`, touch input), current screen, button callbacks, `handleTouch()` (runs `lv_timer_handler()`) / `refreshScreen()` |
| `main/src/weather/weather.h` | weather fetch (Open-Meteo), home weather card, `buildWeatherScreen()` |
| `main/src/settings/settings.h` | `buildSettingsScreen()`, brightness `lv_slider` + backlight PWM |
| `main/src/wifi/wifi_setup.h` | WiFiManager setup portal (`SmartDisplay-Setup`), non-blocking |
| `main/src/wifi/connection_status.h` | polls WiFi every 2 s, logs changes and updates the icon on any screen |
| `main/build_opt.h` | **TFT_eSPI screen configuration** (driver + pins) and **LVGL configuration** (fonts, TFT_eSPI driver), passed to the compiler automatically |
| `main/sketch.yaml` | pinned board/library versions for Arduino CLI |

Each screen is an LVGL screen built by its feature (`build<Name>Screen()`) and rebuilt every time it opens.
Buttons only request a screen change; `handleTouch()` performs it.

Each feature's loop function is named `handle<Feature>()` (`handleWiFi`, `handleConnectionStatus`,
`handleTouch`, `handleWeather`). Only `handleTouch()` changes the open screen.

Feature folders must stay under `main/src/`: Arduino only compiles the sketch folder and
its `src/` subfolder. Files in `src/` don't open as tabs in Arduino IDE. Open them with
File → Open, or open the repo folder in VS Code.

Do **not** edit the TFT_eSPI library's `User_Setup.h` and do **not** add an `lv_conf.h`:
`build_opt.h` replaces both (`LV_CONF_SKIP`), so the configuration travels with the repo. After changing `build_opt.h`, do a clean build (see Troubleshooting).

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `TFT_eSPI.h` / `lvgl.h`: No such file or directory | Libraries not installed (setup step 3). |
| `Sketch too big` / `text section exceeds available space` | Tools → Partition Scheme → **Huge APP** (Build and upload, step 3). |
| `'FS' was not declared in this scope` | `#include <FS.h>` must stay above `#include <TFT_eSPI.h>` in `globals.h`. |
| Screen stays **white** | Screen not configured: `build_opt.h` wasn't used. Make sure you opened `main/main.ino` from this repo. |
| Colors wrong / image mirrored | Your board may use an ST7789 screen (common on 2-USB-port CYDs): in `build_opt.h` replace `-DILI9341_2_DRIVER=1` with `-DST7789_DRIVER=1`. |
| A `build_opt.h` change has no effect / old errors come back | Arduino reuses cached library builds. Close the IDE, delete `%LOCALAPPDATA%\arduino\sketches`, and build again (CLI: `arduino-cli compile --clean`). |
| No COM port | Install the CH340 driver; try another USB cable (some are charge-only). |

## Arduino CLI (optional)

`main/sketch.yaml` pins the board package and library versions. From `main/`:

```bash
arduino-cli compile
```

downloads everything listed there automatically. Upload with:

```bash
arduino-cli upload -p COM5
```

(replace `COM5` with your port).
