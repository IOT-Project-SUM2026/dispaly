# dispaly

ESP32 touchscreen firmware for the Smart Display: home screen, weather + forecast,
settings (brightness), WiFi status.

## Overview

Part of **Smart Display** (IoT project course 236333, Technion, group 5) by
Shada Nujeidat, Yuval Carmelli and Liat Rokach. A smart display that shows personal
information (weather, time, calendar, lists) and receives voice commands sent from a
phone app through Gemini and Firebase. This repo is the ESP32 display firmware.

## Folder description

```
dispaly/
├── Documentation/            install instructions, pinout, states & errors
└── main/                     Arduino sketch (open main/main.ino)
    ├── main.ino, *.h         general code: setup/loop, globals, display & touch, shared UI
    ├── build_opt.h           screen configuration
    ├── sketch.yaml           pinned versions for Arduino CLI
    └── src/                  one folder per feature
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

## One-time setup (per computer)

1. Install **Arduino IDE 2** from arduino.cc.
2. **Board package:** Tools → Board → Boards Manager → search `esp32` → install
   **esp32 by Espressif Systems**, version **3.3.12**.
3. **Libraries:** Tools → Manage Libraries, install these exact versions:

   | Library | Author | Version |
   |---|---|---|
   | TFT_eSPI | Bodmer | 2.5.43 |
   | XPT2046_Touchscreen | Paul Stoffregen | 1.4.0 |
   | WiFiManager | tzapu | 2.0.17 |
   | ArduinoJson | Benoit Blanchon | 7.4.3 |

4. **USB driver:** plug in the board. If no new COM port appears under Tools → Port,
   install the **CH340** driver (the CYD's USB chip) and replug.

## Build and upload

1. Open **`main/main.ino`** (always open it from `main/`, see Troubleshooting).
2. Tools → Board → esp32 → **ESP32 Dev Module**.
3. Tools → Port → the board's COM port.
4. Click **Upload**. If it hangs on `Connecting...`, hold the **BOOT** button on the
   board until the upload starts.

The first build takes several minutes because the whole ESP32 core is compiled.
Later builds only recompile your changes and are much faster.

## First boot: WiFi

On first boot, before connecting to WiFi, the screen shows **static noise**. That is expected.

1. On your phone, join the WiFi network **`SmartDisplay-Setup`**.
2. A setup page opens (otherwise browse to `192.168.4.1`) → *Configure WiFi* →
   pick your network and enter the password.
3. The board connects, fetches the weather and shows the home screen. It remembers
   the network, so later boots skip this step.

Logs: Tools → Serial Monitor at **115200** baud.

## Project files

| File | Purpose |
|---|---|
| `main/main.ino` | `setup()` / `loop()` |
| `main/globals.h` | screen size, colors, UI positions, screen enum |
| `main/display_ui.h` | display + touch init, home screen, touch dispatch |
| `main/shared_ui.h` | WiFi icon, gear icon, back button |
| `main/src/weather/weather.h` | weather fetch (Open-Meteo), home weather card, forecast screen |
| `main/src/settings/settings.h` | settings screen, brightness slider + backlight PWM |
| `main/src/wifi/wifi_setup.h` | WiFiManager setup portal (`SmartDisplay-Setup`) |
| `main/src/wifi/connection_status.h` | older WiFi status icon, currently not included (replaced by `shared_ui.h`) |
| `main/build_opt.h` | **TFT_eSPI screen configuration** (driver + pins), passed to the compiler automatically |
| `main/sketch.yaml` | pinned board/library versions for Arduino CLI |

Feature folders must stay under `main/src/`: Arduino only compiles the sketch folder and
its `src/` subfolder. Files in `src/` don't open as tabs in Arduino IDE. Open them with
File → Open, or open the repo folder in VS Code.

Do **not** edit the TFT_eSPI library's `User_Setup.h`: `build_opt.h` replaces it, so
the configuration travels with the repo. Changing `build_opt.h` triggers a full rebuild.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `TFT_eSPI.h: No such file or directory` | Libraries not installed (setup step 3). |
| `'FS' was not declared in this scope` | `#include <FS.h>` must stay above `#include <TFT_eSPI.h>` in `globals.h`. |
| Screen stays **white** | Screen not configured: `build_opt.h` wasn't used. Make sure you opened `main/main.ino` from this repo. |
| Screen shows **static noise** | Waiting for WiFi setup, see *First boot*. |
| Colors wrong / image mirrored | Your board may use an ST7789 screen (common on 2-USB-port CYDs): in `build_opt.h` replace `-DILI9341_2_DRIVER=1` with `-DST7789_DRIVER=1`. |
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
