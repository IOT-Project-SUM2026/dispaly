# System states and errors

What the display shows in each state, what the Serial log says (115200 baud), and what to do.
The full list of edge cases and how each is handled is in the Edge Cases Design document.

## States

| State | What the screen shows | What to do |
|---|---|---|
| No WiFi configured / not reachable | Red "⚠ No WiFi" at the top; the weather card says "No WiFi" with "WiFi setup: Settings > Device" (the setup network name and password are shown there); the portal runs in the background | Join `SmartDisplay-Setup` from a phone and choose a network |
| Online | Home screen, normal WiFi icon | — |
| All Home widgets hidden | "All widgets are hidden - Turn them on in Settings > Home screen" | Settings (⚙) → Home screen |
| No WiFi | Red "⚠ No WiFi" instead of the WiFi icon (updated within ~2 s on any screen); on Home the greeting row says "⚠ No WiFi"; "Updated 10:30" on the Weather screen turns red | Check the router; the last loaded data stays on screen |
| WiFi works, no internet | Red "⚠ No internet" (on Home in the greeting row), after 20 s in which no server answered; weather / calendar messages say "No internet" | Check the router's internet connection; everything recovers by itself |
| Clock not synced yet | "Hello", `--:--` and "Waiting for WiFi" / "Getting the time..." on Home; `--:--` and no hands on the World Clock | Connect WiFi; NTP syncs within seconds. After that the clock keeps time even offline |
| Weather not loaded | "Loading..." (or "No WiFi") on the home card and the Weather screen | Waits for WiFi, then retries every 30 s until it succeeds (afterwards every 10 min) |
| Power saving (idle) | Screen dimmed or backlight off after the chosen time without touch | Touch the screen; the first touch only wakes it |
| Voice commands: not set up | Settings → Device: Voice commands "Not set up"; empty lists say "Voice commands are off" | Fill in the Setup page (Settings → Device → Setup page) |
| Voice commands: waiting for WiFi | Settings → Device: "Waiting for WiFi" | Firebase starts by itself once WiFi connects |
| Voice commands: listening | `[Firebase] Listening for commands` in the log | Speak a command in the app; the display replies within about a second |
| Voice commands: login or database setup error | Settings → Device: "Sign-in failed" / "Database error"; `[Firebase] Sign-in failed` or `Database error` in the log, retried every 30 s | See Errors below |
| Setup page open | Settings → Device → Setup page shows `http://<address>` | Open it on a phone on the same WiFi; closes by itself after 10 min |
| Settings saved on the Setup page | Pop-up "Settings saved. Restarting...", then a restart | — |

Saved across reboots: both lists, Home widgets (shown / big), weather city, 24-hour time, brightness, dark mode, power saving on/off, idle time, dim/off, dim level (see `parameters.md`).

## Serial log (115200 baud)

| Tag | Example |
|---|---|
| `[Boot]` | `Smart Display starting` |
| `[Health]` | `Crashed 3 times right after starting: using default settings` (display settings, saved weather and calendar are cleared; WiFi, setup page and lists are kept) · `Last restart: Power on` (or `Watchdog (froze)`, `Crash`, `Weak power supply`) · `Watchdog on (60 s)` · every 10 min: `Up 3 h 12 min, free memory … (lowest …, largest block …), WiFi -55 dBm` · `Stack left: loop …, firebase …, net … bytes` (under 1000 = raise that task's stack) · `Restarting to free memory` |
| `[Config]` | `Voice commands set up, calendar NOT set up` · `Setup page saved: changed, restarting` · `Could not save <key> to flash` |
| `[WiFi]` | `Connected, IP: …` · `Disconnected` · `Not connected - setup portal "SmartDisplay-Setup" is open` · `Continuing offline` · `Retrying saved network` · `Connected, but no internet (no server answered for 20 s)` · `Internet is back` · `Setup page open at http://… for 10 min` · `Setup page closed` |
| `[Time]` | `NTP started` · `Synced: 2026-10-09 14:32` |
| `[Weather]` | `Last saved weather loaded (shown until the next download)` · `City -> Eilat` · `Downloaded 5 cities in 1450 ms` · `Updated (Eilat)` · `Retrying once` · `Skipped - no WiFi` · `HTTP error: <code> (after … ms)` · `JSON parse failed` |
| `[Calendar]` | `2 saved events loaded` (at start, before the first download) · `Updated: 3 upcoming events` |
| `[UI]` | `Home -> Settings` · `Home -> Weather` · `Home -> Time` · `Settings -> Home` |
| `[Settings]` | `Reset display: erasing saved data and WiFi, restarting` · `Brightness 78%` · `24-hour time off` · `Power saving on` · `Idle after 5 min` · `When idle: dim` · `Dim to 10%` |
| `[Idle]` | `Entering idle (dim to 10%)` · `Entering idle (screen off)` · `Wake` |
| `[Storage]` | `city = 4` (a setting was saved) |
| `[Lists]` | `Loaded: 2 shopping, 0 to-do items` · `Starting empty: 0 shopping, 0 to-do items` · `Shopping list (2): milk, eggs` · `Could not save to flash, retrying` · `Saved to flash (after a failed try)` |
| `[Firebase]` | `Started` · `Signed in (free memory: …)` · `Listening for commands` · `Command -OAb…: add "milk" (shopping) -> Added milk to your shopping list` (`[app already answered]` when it was sent while the display was offline) · `Login renewed` · `Reconnected` |

## Errors

| Error | Cause | Fix |
|---|---|---|
| `[Weather] HTTP error: <code>` | Open-Meteo unreachable or no internet behind the WiFi (`-11` = no response within 5 s) | Retried once right away, then every 30 s |
| `[Firebase] Sign-in failed (400): INVALID_LOGIN_CREDENTIALS` | Wrong email or password on the Setup page (or in `secrets.h`), or the user doesn't exist | Check Authentication → Users in the Firebase console |
| `[Firebase] Sign-in failed (400): OPERATION_NOT_ALLOWED` | Email/Password sign-in is turned off | Authentication → Sign-in method → Email/Password → Enable |
| `[Firebase] Sign-in failed (400): API key not valid…` | Wrong Web API key on the Setup page (or `FIREBASE_API_KEY` in `secrets.h`) | Copy `apiKey` from Project settings → General → Your apps |
| `[Firebase] Sign-in failed (400): TOO_MANY_ATTEMPTS_TRY_LATER` | Too many failed sign-ins | Fix the login, wait a few minutes |
| `[Firebase] Database error 401: … Permission denied …` | Database rules don't allow signed-in users | Realtime Database → Rules → `".read": "auth != null", ".write": "auth != null"` |
| `[Firebase] Database error 404: …` | Wrong `FIREBASE_DATABASE_URL` | Copy the link from Realtime Database → Data |
| `[Firebase] No connection to the database (-1)` | Internet down behind the WiFi | Retried every 2 s; logged once |
| `[Firebase] Could not confirm a command (…), retrying` | Writing `/status/<id>` and removing the command from `/commands` failed | Retried every 2 s; the command is not applied twice |
| `Stack canary watchpoint triggered (firebase)` / `(net)` (board restarts) | The Firebase task / background download task ran out of stack | Raise `FIREBASE_TASK_STACK` in `firebase_sync.h` / `NET_TASK_STACK` in `net/net.h` |
| `[Weather] Connection failed: … (TLS <n>: …), free memory …, largest block …` (also `[Calendar]`, `[Firebase]`) | A secure connection failed; the TLS text says why | See the next rows |
| `… (TLS -32512: SSL - Memory allocation failed)` | Not enough free memory in one piece for a secure connection | After 5 in a row: `[Net] Out of memory for secure connections 5 times in a row: restarting`, then `[Health] Restarting to free memory`; lists and settings come back from flash |
| `[Net] Certificate check keeps failing: continuing without it (connections stay encrypted)` | 2 certificate errors in a row (old board package, or a network that inspects HTTPS) | Automatic until the next restart; to turn the check off for good set `TLS_VERIFY_CERTS` to 0 in `src/net/net.h` |
| `[Net] This ESP32 board package can't check certificates: update it to 3.3.12` | Older ESP32 board package | Tools → Board → Boards Manager → esp32 3.3.12 |
| Board restarts, then Settings → Device → Last restart: "Watchdog (froze)" | `loop()` didn't run for 60 s | The log before the restart shows the last thing that ran; everything comes back from flash |
| App answered, but the display didn't change yet | Display off or offline: the app's server answered from the `/lists` copy and the command waits in `/commands` | Applied by itself when the display is back online |
| `… was changed on the display after this request` | A queued voice command is older than a tap on the screen for the same item | Expected: the later change wins (see README, Offline changes) |

## Lists, voice commands, calendar

| Log / state | Meaning | What to do |
|---|---|---|
| `[Lists] Cleared Shopping list (6 items)` | "Clear list" confirmed with a second tap (a pop-up says "Shopping list cleared") | Add items again by voice or in the app |
| `[Lists] Ticked off "milk" (removed in 3 s unless tapped again)` · `Kept "milk"` · `Removed "milk" (tapped)` | Item tapped on the Lists screen (a pop-up says how to keep it) | Tap again within 3 s to keep it |
| `[Firebase] Command -OAb… was applied before the restart: …` | The power went out after a command was applied but before its result was sent | Answered with the saved result, not applied twice |
| `[Firebase] Command -OAb… skipped: more than a week old` | The display was away for more than a week | The app's user is told it was skipped |
| `[Lists] Clock synced: times of offline changes fixed` | A tap was made before the clock synced; its time is now known | — |
| `[Lists] Converted from the old format` | First boot after updating from the version without change times; items are kept | — |
| `[Firebase] Lists saved to /lists` | Lists copied to Firebase (after a change, or once WiFi is back after an offline change) | — |
| Pop-up at the top, e.g. "Added milk to your shopping list" (red if it failed) | A voice command arrived; the list it changed is open | Goes back home after 30 s untouched |
| `[Calendar] Not set up: add the calendar ID and API key on the setup page (or in secrets.h)` | No calendar configured | Settings → Device → Setup page; README → Google Calendar |
| `[Calendar] Updated: 3 upcoming events` | Events loaded (every 10 min) | — |
| `[Calendar] Calendar screen opened: checking for new events` | The Calendar screen was opened: events are checked right away (at most every 15 s) | — |
| `[Calendar] Error 404: Not Found` | Wrong Calendar ID or calendar not public | Copy the ID again; Make available to public |
| `[Calendar] Error 400/403: API key not valid / API disabled` | Wrong key, or Google Calendar API not enabled | Credentials / Library in console.cloud.google.com |
| Items or events show `???` | Characters the fonts don't have (emoji, Arabic...). English and Hebrew are supported | Use English or Hebrew |
