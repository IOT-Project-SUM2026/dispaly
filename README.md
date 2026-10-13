# Smart Display - ESP32 firmware (`dispaly`)

A smart display for the desk or the kitchen: time and date, a world clock, weather and a
4-day forecast for 5 Israeli cities, your Google Calendar, and shopping / to-do lists that
you update **by voice** from a phone app (through Gemini and Firebase). Touch interface with
dark and light themes, English and Hebrew text, power saving, and it keeps working offline.
This repository is the ESP32 firmware; the phone app is in its own repository (below).

<!-- Poster: add a photo of the project poster as assets/poster.jpg and remove the comment marks:
![Project poster](assets/poster.jpg)
-->

![Screens](assets/screens_dark.png)

## Team

IoT project course 236333, Technion Faculty of Computer Science, group 5א:
**Shada Nujeidat, Yuval Carmelli, Liat Rokach**.

## Project repositories

| Repository | Contents |
|---|---|
| **dispaly** (this one) | ESP32 firmware, unit tests, documentation, diagrams |
| **Smart Display app** (https://iot-smart-display.netlify.app/) | phone web app (PWA) + Node.js backend that sends the voice recording to Gemini and writes the command to Firebase. Libraries and versions: its `package.json` |

The final version of each repository is on its `main` (master) branch.

## How it works

![System overview](assets/system_overview.png)

1. The phone app records a command ("add milk to the shopping list") and sends it to the backend.
2. The backend asks Gemini to turn it into JSON `{action, item, list}`, checks it, and writes it to Firebase `/command`.
3. The display checks `/command` every second (in its own task on the second CPU core), updates the list, saves it in flash, shows it with a pop-up, and writes the result to `/status`.
4. The app shows the result. Weather (Open-Meteo), calendar (Google Calendar API) and time (NTP) are downloaded by the display directly.

## Hardware

| Part | Quantity | Notes |
|---|---|---|
| ESP32-2432S028R "Cheap Yellow Display" (ESP32 + 2.8" 320×240 ILI9341 screen + XPT2046 resistive touch) | 1 | everything is on this one board |
| Micro-USB data cable | 1 | some cables are charge-only and won't upload |
| USB power adapter, 5 V ≥ 1 A | 1 | a laptop port may be too weak with WiFi and the backlight on |
| Smartphone with a browser | - | the user's own, runs the voice app |

### Wiring diagram

All parts are on the board, so there is no external wiring: only the USB cable.
Pins: [Documentation/pinout_wiring.md](Documentation/pinout_wiring.md).

![Wiring diagram](assets/wiring_diagram.png)

## Versions

| | Version |
|---|---|
| Arduino IDE | 2.x |
| Board package: esp32 by Espressif Systems | 3.3.12 (board: ESP32 Dev Module, partition: Huge APP) |
| TFT_eSPI (Bodmer) | 2.5.43 |
| lvgl (kisvegabor) | 9.6.0 |
| XPT2046_Touchscreen (Paul Stoffregen) | 1.4.0 |
| WiFiManager (tzapu) | 2.0.17 |
| ArduinoJson (Benoit Blanchon) | 7.4.3 |

The same versions are pinned in `main/sketch.yaml`, so `upload.bat` / `arduino-cli` install exactly these.

## What's in this repository

```
dispaly/
├── README.md                 this file
├── upload.bat                one-click build + upload (Windows)
├── main/                     Arduino sketch (open main/main.ino)
│   ├── secrets.example.h     template for secrets.h (Firebase login, calendar key; secrets.h is not in git)
│   ├── main.ino, *.h         setup/loop, globals, shared LVGL widgets
│   ├── build_opt.h           screen + LVGL configuration
│   ├── sketch.yaml           pinned versions for Arduino CLI
│   └── src/                  one folder per feature
│       ├── home/             home screen (greeting, clock, weather, Lists and Next event cards)
│       ├── time_date/        NTP time, home clock, world clock screen
│       ├── idle/             power saving (dim / screen off after inactivity)
│       ├── storage/          settings saved in flash (Preferences)
│       ├── theme/            design system: colors, shared styles, clock and Hebrew fonts
│       ├── touch/            LVGL + display + touch init, screen switching, voice command pop-up
│       ├── weather/          weather fetch (all cities at once), city choice, weather screen
│       ├── settings/         settings screen (folding sections: display, clock, power saving)
│       ├── wifi/             WiFi setup portal, connection status
│       ├── lists/            shopping + to-do lists (saved in flash), lists screen
│       ├── calendar/         Google Calendar events, calendar screen
│       └── firebase/         voice commands from the app via Firebase
├── unit_tests/               one sketch per hardware part / service, see unit_tests/README.md
├── Documentation/            guides (list below)
└── assets/                   diagrams, enclosure sketch, screenshots (assets/screenshots/)
```

| Document | Contents |
|---|---|
| [Documentation/user_guide.md](Documentation/user_guide.md) | how to use the display and the app, what each indicator means |
| [Documentation/install_instructions.txt](Documentation/install_instructions.txt) | step-by-step setup for developers |
| [Documentation/parameters.md](Documentation/parameters.md) | every hard-coded parameter, saved settings, touch calibration |
| [Documentation/errors_and_states.md](Documentation/errors_and_states.md) | what the display shows in each state, log messages, errors and fixes |
| [Documentation/pinout_wiring.md](Documentation/pinout_wiring.md) | CYD pins and further reading |
| [Documentation/ui_design.md](Documentation/ui_design.md) | the design system (colors, fonts, spacing) and screen layouts |
| [unit_tests/README.md](unit_tests/README.md) | the 9 hardware / service tests and how to run them |
| [Documentation/course_submissions/](Documentation/course_submissions/) | course documents: user stories, HW unit tests report (with results), feasibility tests, edge cases design, enclosure sketch |

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
- The display holds the lists (saved in flash, so they work offline) and keeps a copy in
  Firebase `/lists`. Screens read them with `listSize()` / `listItem()` from `src/lists/lists.h`;
  every access goes through a lock, because the Firebase task changes them too.

### Test without the phone app

In the Firebase console, open Realtime Database → Data and edit `/command`: set `type`, `item`,
`list`, and change `runningCount` to a number it hasn't had yet. Within about a second the
Serial Monitor shows the command and the updated list, and `/status` changes in the console.

Logs: Tools → Serial Monitor at **115200** baud. See [errors_and_states.md](Documentation/errors_and_states.md) for the log messages.

## Voice commands on the screen

When a command arrives the display wakes up, opens the list it changed (a new item is
shown in the accent color) and shows the result at the top for 3 s, e.g. "Added milk to your
shopping list" (no transcription, per the instructor). If nobody touches it, it goes back
home after 30 s. The Firebase task never draws: it hands each command to `loop()` through a
FreeRTOS queue, because LVGL may only be used from one task.

The display also keeps a copy of both lists in Firebase at `/lists`. A change made without
WiFi is saved on the board and copied as soon as the connection is back.

## Google Calendar (optional)

The display reads a **public** Google calendar with an **API key**. Without it the calendar
shows "Not set up" and everything else works.

1. Google Calendar (computer) → Other calendars → + → Create new calendar.
2. Its Settings and sharing → Access permissions → **Make available to public** (See all event details).
3. Integrate calendar → copy the **Calendar ID**.
4. console.cloud.google.com → project `smart-display-5f14b` → APIs & Services → Library → **Google Calendar API** → Enable.
5. APIs & Services → Credentials → Create credentials → **API key**. Edit the key → API restrictions → **Restrict key** → Google Calendar API, so the key can't be used for anything else.
6. Add both to `main/secrets.h` (lines are in `secrets.example.h`): `CALENDAR_ID`, `CALENDAR_API_KEY`.

## Project files

| File | Purpose |
|---|---|
| `main/main.ino` | `setup()` / `loop()` |
| `main/globals.h` | screen size, layout grid (margins, header and row heights), `Theme` struct |
| `main/shared_ui.h` | shared LVGL widgets: screen, label, card, header bar (back, title, time, WiFi), icon buttons, dropdown, messages, weather icons; Hebrew-aware text helpers |
| `main/src/home/home.h` | `buildHomeScreen()`: greeting, clock, weather / Lists / Next event cards; `refreshHomeCard()` rebuilds only the card whose data changed |
| `main/src/theme/theme.h` | design system: dark and light palettes and every shared `lv_style_t`; Dark mode rewrites the styles in place (no rebuild) |
| `main/src/theme/font_display.c` | big digits for the clock and temperatures (Ubuntu Medium, digits only, ~4 KB) |
| `main/src/theme/font_hebrew_14.c`, `font_hebrew_20.c` | Hebrew letters, falling back to Montserrat for everything else |
| `main/src/lists/lists_screen.h` | `buildListsScreen()`: Shopping / To-do switch in the top bar, tap an item to tick it off (only that row changes; removed after 2 s, tap again to keep); home Lists card |
| `main/src/calendar/calendar.h` | Google Calendar API request and events (no drawing), checked every 10 min |
| `main/src/calendar/calendar_screen.h` | `buildCalendarScreen()` (events grouped by day, the event that is on now marked), home Next event card |
| `main/src/time_date/time_date.h` | NTP (Israel time zone with DST), home greeting and clock, header clock, `buildTimeScreen()` world clock (LVGL `lv_scale` dials), `handleTimeDate()` |
| `main/src/idle/idle.h` | power saving: `handleIdle()` dims or turns off the backlight after inactivity; the waking touch is ignored |
| `main/src/storage/storage.h` | `loadInt()` / `saveInt()` over ESP32 Preferences, so settings survive a reboot |
| `main/src/touch/display_ui.h` | LVGL init (display via `lv_tft_espi`, touch input), current screen, button callbacks, `handleTouch()` (runs `lv_timer_handler()`) / `refreshScreen()` |
| `main/src/weather/weather.h` | weather for all 5 cities in ONE request (Open-Meteo, every 10 min), saved per city so switching cities is instant; home weather card, `buildWeatherScreen()` with the city button |
| `main/src/settings/settings.h` | `buildSettingsScreen()`: folding sections (Display, Clock, Power saving); backlight PWM |
| `main/src/wifi/wifi_setup.h` | WiFiManager setup portal (`SmartDisplay-Setup`), non-blocking |
| `main/src/wifi/connection_status.h` | polls WiFi every 2 s, logs changes and updates the icon on any screen |
| `main/src/lists/lists.h` | shopping + to-do lists in memory, saved to flash; thread-safe add/remove/read for screens |
| `main/src/firebase/firebase_sync.h` | signs in to Firebase, checks `/command` every second in a background task, writes `/status`, copies the lists to `/lists` (also after an offline change, once WiFi is back); passes each command to `loop()` through a queue |
| `main/secrets.example.h` | template for `main/secrets.h` (Firebase login, optional Google Calendar; not in git) |
| `main/build_opt.h` | **TFT_eSPI screen configuration** (driver + pins) and **LVGL configuration** (fonts, TFT_eSPI driver), passed to the compiler automatically |
| `main/sketch.yaml` | pinned board/library versions for Arduino CLI |

Each screen is an LVGL screen built by its feature (`build<Name>Screen()`) when it opens. New data only
rebuilds the part that shows it (one home card, the weather content, a list row), and colors come only
from the shared styles in `theme.h`, so the dark/light switch restyles everything without a rebuild.
Buttons only request a screen change; `handleTouch()` performs it.

Each feature's loop function is named `handle<Feature>()` (`handleWiFi`, `handleConnectionStatus`,
`handleTimeDate`, `handleTouch`, `handleIdle`, `handleWeather`, `handleFirebase`). Features with saved settings load them in
`init<Feature>()` (`initWeather`, `initTimeDate`, `initIdle`) right after `initStorage()`. Only `handleTouch()` changes the open screen.

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
| `Missing main/secrets.h` | Create it from `secrets.example.h` (see Firebase login). |
| Calendar says "Not set up" | Add `CALENDAR_ID` and `CALENDAR_API_KEY` to `secrets.h` (see Google Calendar below). |
| Items or events show `???` | Characters the fonts don't have (emoji, Arabic...). English and Hebrew are supported. |

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
