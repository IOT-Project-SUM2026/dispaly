# dispaly

ESP32 touchscreen firmware for the Smart Display: home screen, weather + forecast,
settings (brightness), WiFi status, and shopping / to-do lists updated by voice
commands from the phone app (through Firebase).

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
    ├── main.ino, *.h         general code: setup/loop, globals, shared UI drawing
    ├── secrets.example.h     template for secrets.h (Firebase login, not in git)
    ├── build_opt.h           screen configuration
    ├── sketch.yaml           pinned versions for Arduino CLI
    └── src/                  one folder per feature
        ├── home/             home screen
        ├── touch/            display + touch init, touch zones, screen switching
        ├── weather/          weather fetch + weather screen
        ├── settings/         settings screen + brightness
        ├── wifi/             WiFi setup portal, connection status
        ├── lists/            shopping + to-do lists, saved in flash
        └── firebase/         voice commands from the app via Firebase
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

The display starts right away and works offline. Until WiFi is set up it shows the offline WiFi icon and runs a setup portal in the background:

1. On your phone, join the WiFi network **`SmartDisplay-Setup`**.
2. A setup page opens (otherwise browse to `192.168.4.1`) → *Configure WiFi* →
   pick your network and enter the password.
3. The board connects (the WiFi icon updates within ~2 s) and the weather appears. It remembers
   the network, so later boots skip this step.

## Firebase login (voice commands)

The display signs in to Firebase as its own user, so it needs a `main/secrets.h` file.
It is in `.gitignore`: **never upload it to GitHub** (also not by drag-and-drop on the website).

1. Copy `main/secrets.example.h` to `main/secrets.h`.
2. Fill in the four values. All come from the Firebase console, project `smart-display-5f14b`:
   - `FIREBASE_DATABASE_URL`: already filled in
   - `FIREBASE_API_KEY`: Project settings → General → Your apps → `apiKey`
   - `FIREBASE_USER_EMAIL` / `FIREBASE_USER_PASSWORD`: the display's user from
     Authentication → Users (ask the teammate who created it)
3. Upload as usual. The Serial Monitor shows `[Firebase] Signed in`, then
   `[Firebase] Listening for commands`.

Firebase setup this relies on (done once, in the console):
- Authentication → Sign-in method → **Email/Password** enabled, and a user for the display.
- Realtime Database → Rules: `{ "rules": { ".read": "auth != null", ".write": "auth != null" } }`.
  The phone app's backend uses the Admin SDK, which ignores these rules.

### How voice commands reach the display

The app writes `/command` = `{ "type": "add" | "remove", "item": "milk", "list": "shopping" | "todo", "runningCount": 5 }`.
`runningCount` goes up by 1 with every command. The display checks `/command` every second
and, when the number changes, updates the list and writes
`/status` = `{ "runningCount": 5, "success": true, "text": "Added milk to your shopping list" }`.
The app waits up to 60 s for a `/status` with the same number and shows the result.

- Removing an item that isn't on the list, or adding to a full list (20 items), sends `success: false`.
- Adding an item that is already there sends `success: true` ("milk is already on your shopping list").
- At startup the display skips the command already in `/command`: it was sent while the display
  was off, and the app has already told the user it wasn't confirmed.
- Firebase runs in its own FreeRTOS task on core 0, so checking the database never freezes the
  touchscreen (`loop()` runs on core 1).
- The lists exist only on the display (Firebase holds just the latest command), saved in flash.
  A list screen reads them with `listSize()` / `listItem()` from `src/lists/lists.h` and can
  remove a tapped item with `removeItemAt()`. Until there is one, the Serial Monitor prints the
  list after every change.

### Test without the phone app

In the Firebase console, open Realtime Database → Data and edit `/command`: set `type`, `item`,
`list`, and change `runningCount` to a number it hasn't had yet. Within about a second the
Serial Monitor shows the command and the updated list, and `/status` changes in the console.

Logs: Tools → Serial Monitor at **115200** baud. See [errors_and_states.md](Documentation/errors_and_states.md) for the log messages.

## Project files

| File | Purpose |
|---|---|
| `main/main.ino` | `setup()` / `loop()` |
| `main/globals.h` | screen size, colors, UI positions |
| `main/shared_ui.h` | WiFi icon, gear icon, back button drawing |
| `main/src/home/home.h` | home screen |
| `main/src/touch/display_ui.h` | display + touch init, all touch zones, current screen, `handleTouch()` / `refreshScreen()` |
| `main/src/weather/weather.h` | weather fetch (Open-Meteo), home weather card, forecast screen |
| `main/src/settings/settings.h` | settings screen, brightness slider + backlight PWM |
| `main/src/wifi/wifi_setup.h` | WiFiManager setup portal (`SmartDisplay-Setup`), non-blocking |
| `main/src/wifi/connection_status.h` | polls WiFi every 2 s, logs changes and updates the icon on any screen |
| `main/src/lists/lists.h` | shopping + to-do lists in memory, saved to flash; thread-safe add/remove/read for screens |
| `main/src/firebase/firebase_sync.h` | signs in to Firebase, checks `/command` every second in a background task, writes `/status` |
| `main/secrets.example.h` | template for `main/secrets.h` (Firebase login, not in git) |
| `main/build_opt.h` | **TFT_eSPI screen configuration** (driver + pins), passed to the compiler automatically |
| `main/sketch.yaml` | pinned board/library versions for Arduino CLI |

Each feature's loop function is named `handle<Feature>()` (`handleWiFi`, `handleConnectionStatus`,
`handleTouch`, `handleWeather`, `handleFirebase`). Only `handleTouch()` changes the open screen.

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
| Colors wrong / image mirrored | Your board may use an ST7789 screen (common on 2-USB-port CYDs): in `build_opt.h` replace `-DILI9341_2_DRIVER=1` with `-DST7789_DRIVER=1`. |
| No COM port | Install the CH340 driver; try another USB cable (some are charge-only). |
| `Missing main/secrets.h` | Create it from `secrets.example.h` (see Firebase login). |
| `Sketch too big` | The firmware uses about 94% of the default program space. Tools → Partition Scheme → **Huge APP (3MB No OTA/1MB SPIFFS)**. |

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
