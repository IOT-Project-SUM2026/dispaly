# Parameters

Every value that is fixed in the code (hard coded) and changing it needs a new upload.
Values the user can change on the screen are listed at the end (they are saved in flash,
no upload needed). Secrets (Firebase login, calendar key) are not here: they are in
`main/secrets.h`, made from the template `main/secrets.example.h`.

File paths are relative to `main/`.

## Hardware pins and screen

| Parameter | Value | File | What it does |
|---|---|---|---|
| `TFT_MISO` / `TFT_MOSI` / `TFT_SCLK` | 12 / 13 / 14 | `build_opt.h` | screen SPI pins (HSPI) |
| `TFT_CS` / `TFT_DC` / `TFT_RST` | 15 / 2 / -1 | `build_opt.h` | screen chip select, data/command, reset (not connected) |
| `ILI9341_2_DRIVER` | 1 | `build_opt.h` | screen driver. Boards with an ST7789 panel: use `ST7789_DRIVER` |
| `TFT_INVERSION_ON` | 1 | `build_opt.h` | this panel needs color inversion |
| `SPI_FREQUENCY` | 55 MHz | `build_opt.h` | screen SPI speed (full redraw ≈ 22 ms) |
| `SPI_TOUCH_FREQUENCY` | 2.5 MHz | `build_opt.h` | touch SPI speed |
| `XPT2046_IRQ` / `MOSI` / `MISO` / `CLK` / `CS` | 36 / 32 / 39 / 25 / 33 | `src/touch/display_ui.h` | touch controller pins (VSPI) |
| `BACKLIGHT_PIN` | 21 | `src/settings/settings.h` | backlight PWM pin |
| `PWM_FREQ` / `PWM_RES` | 5000 Hz / 8 bit | `src/settings/settings.h` | backlight PWM (0-255) |
| `SCREEN_WIDTH` / `SCREEN_HEIGHT` | 320 / 240 | `globals.h` | landscape screen size |
| `DRAW_BUF_SIZE` | 1/10 screen (15 KB) | `src/touch/display_ui.h` | LVGL draw buffer; bigger costs RAM that TLS needs |
| Partition scheme | Huge APP (3 MB) | `sketch.yaml`, README | the firmware (1.61 MB) does not fit the default 1.3 MB |

## Touch calibration

| Parameter | Value | File | What it does |
|---|---|---|---|
| raw X range | 200 … 3700 → 1 … 320 | `src/touch/display_ui.h` (`readTouch`) | converts the touch controller's X reading to pixels |
| raw Y range | 240 … 3800 → 1 … 240 | `src/touch/display_ui.h` (`readTouch`) | converts the Y reading to pixels |

**How to calibrate:** run `unit_tests/test_touch`, tap the 5 crosses and read the raw
values it prints for the corners. Put the raw values of the left/right edges into the X
range and of the top/bottom edges into the Y range, upload, and run the test again: every
tap should be within 20 px.

## Layout (design system)

| Parameter | Value | File | What it does |
|---|---|---|---|
| `MARGIN` | 12 px | `globals.h` | left/right screen margin |
| `GAP` | 8 px | `globals.h` | space between cards |
| `HEADER_H` / `CONTENT_Y` | 44 / 48 px | `globals.h` | header bar height, where content starts |
| `TOUCH_MIN` / `ROW_H` | 44 / 44 px | `globals.h` | smallest tap target, list and settings row height |
| `COL_W` | 144 px | `globals.h` | half-width card |
| `HOME_*` | see file | `src/home/home.h` | home screen card positions |
| Colors (dark and light palette) | see file | `src/theme/theme.h` | every color and text style, see `Documentation/ui_design.md` |

## Network and services

| Parameter | Value | File | What it does |
|---|---|---|---|
| Setup portal name | `SmartDisplay-Setup` | `src/wifi/wifi_setup.h` | WiFi network the display opens for setup |
| Connect timeout | 10 s | `src/wifi/wifi_setup.h` | per attempt to join the saved network |
| Portal re-opens after | 60 s offline | `src/wifi/wifi_setup.h` | lets the user pick a new network |
| Retry saved network | every 15 s (60 s while the portal is open) | `src/wifi/wifi_setup.h` | reconnect while offline |
| WiFi status check | every 2 s | `src/wifi/connection_status.h` | updates the WiFi / Offline indicator |
| `LOCAL_TZ` | `IST-2IDT,M3.4.4/26,M10.5.0` | `src/time_date/time_date.h` | Israel time zone with summer time |
| NTP servers | ntp.technion.ac.il, pool.ntp.org, time.google.com | `src/time_date/time_date.h` | Technion first: other servers are blocked on campus |
| `WORLD_CITY_COUNT`, `worldCities[]` | 6 cities | `src/time_date/time_date.h` | world clock cities and their time zone rules |
| `WEATHER_CITY_COUNT`, `weatherCities[]` | 5 cities (Jerusalem, Tel Aviv, Haifa, Beer Sheba, Eilat) | `src/weather/weather.h` | cities and coordinates, all fetched in one request |
| `FORECAST_DAYS` | 4 | `src/weather/weather.h` | forecast days on the weather screen |
| Weather refresh | every 10 min (every 30 s until the first success, one immediate retry) | `src/weather/weather.h` | |
| `CALENDAR_REFRESH_MS` / `CALENDAR_RETRY_MS` | 10 min / 60 s | `src/calendar/calendar.h` | calendar refresh, retry after an error |
| Calendar check on open | at most every 15 s | `src/calendar/calendar.h` | opening the Calendar screen checks for new events |
| `CALENDAR_MAX_EVENTS` / `CALENDAR_TITLE_LEN` | 8 / 48 bytes | `src/calendar/calendar.h` | events kept, title length |
| TLS handshake timeout | 8 s (Firebase 15 s) | `weather.h`, `calendar.h`, `firebase_sync.h` | limits the freeze if WiFi drops mid-request (default 120 s) |
| `FIREBASE_POLL_MS` | 1000 ms | `src/firebase/firebase_sync.h` | how often `/command` is checked |
| `FIREBASE_RETRY_MS` | 30 s | `src/firebase/firebase_sync.h` | wait after a sign-in or database setup error |
| `FIREBASE_TASK_STACK` | 12288 bytes | `src/firebase/firebase_sync.h` | Firebase task stack; raise if the log shows "Stack canary" |
| Login renewal | 5 min before expiry | `src/firebase/firebase_sync.h` | Firebase tokens last 1 hour |
| Command queue | 4 | `src/firebase/firebase_sync.h` | voice commands waiting to be shown |

## Lists and voice commands

| Parameter | Value | File | What it does |
|---|---|---|---|
| `LIST_COUNT` | 2 | `src/lists/lists.h` | shopping + to-do |
| `LIST_MAX_ITEMS` | 20 | `src/lists/lists.h` | items per list ("is full" after that) |
| `LIST_ITEM_LEN` | 48 bytes | `src/lists/lists.h` | item length incl. terminator (a Hebrew letter is 2 bytes) |
| `LISTS_VERSION` | 1 | `src/lists/lists.h` | raise when the saved list format changes, so old data is ignored |
| `TICK_OFF_DELAY_MS` | 2000 ms | `src/lists/lists_screen.h` | a ticked item is removed after this (tap again to keep) |
| `VOICE_SCREEN_MS` | 30 s | `src/touch/display_ui.h` | a list opened by a voice command goes back home after this untouched |
| Pop-up time | 3 s | `src/touch/display_ui.h` | result of a voice command at the top of the screen |

## Power saving

| Parameter | Value | File | What it does |
|---|---|---|---|
| `idleTimeoutMinutes[]` | 1, 2, 5, 10, 15, 30 | `src/idle/idle.h` | choices for "Idle after" |
| `idleDimPercents[]` | 5, 10, 20, 30, 50 % | `src/idle/idle.h` | choices for "Dim to" |
| Brightness minimum | 10 / 255 | `src/settings/settings.h` | the slider can't turn the screen fully dark |

## Settings the user changes on the screen (saved in flash)

Saved in the NVS namespace `display` (lists in namespace `lists`). A saved value outside its
range is replaced by the nearest valid one when loaded.

| Setting (screen) | Key | Default | Range |
|---|---|---|---|
| Brightness (Settings → Display) | `brightness` | 200 | 10 … 255 |
| Dark mode (Settings → Display) | `darkMode` | on | on / off |
| 24-hour time (Settings → Clock) | `use24h` | on | on / off |
| Power saving (Settings → Power saving) | `idleOn` | off | on / off |
| Idle after | `idleAfter` | 5 min | 1 … 30 min |
| When idle | `idleAction` | dim | dim / screen off |
| Dim to | `idleDim` | 10 % | 5 … 50 % |
| Weather city (Weather screen) | `city` | Haifa | 5 cities |
| WiFi network (setup portal) | ESP32 WiFi settings | - | any 2.4 GHz network |
