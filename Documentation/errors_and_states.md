# System states and errors

What the display shows in each state, what the Serial log says (115200 baud), and what to do.
The full list of edge cases and how each is handled is in the Edge Cases Design document.

## States

| State | What the screen shows | What to do |
|---|---|---|
| No WiFi configured / not reachable | Red "⚠ Offline" at the top; the weather card says "No WiFi - Join the WiFi network SmartDisplay-Setup"; the portal runs in the background | Join `SmartDisplay-Setup` from a phone and choose a network |
| Online | Home screen, normal WiFi icon | — |
| Offline | Red "⚠ Offline" instead of the WiFi icon (updated within ~2 s on any screen); "Updated 10:30" on the Weather screen turns red | Check the router; the last loaded data stays on screen |
| Clock not synced yet | "Hello", `--:--` and "Waiting for WiFi" / "Getting the time..." on Home; `--:--` and no hands on the World Clock | Connect WiFi; NTP syncs within seconds. After that the clock keeps time even offline |
| Weather not loaded | "Loading..." (or "No WiFi") on the home card and the Weather screen | Waits for WiFi, then retries every 30 s until it succeeds (afterwards every 10 min) |
| Power saving (idle) | Screen dimmed or backlight off after the chosen time without touch | Touch the screen; the first touch only wakes it |
| Voice commands: waiting for WiFi | Nothing on screen; no `[Firebase]` lines after `Started` | Firebase starts by itself once WiFi connects |
| Voice commands: listening | `[Firebase] Listening for commands` in the log | Speak a command in the app; the display replies within about a second |
| Voice commands: login or database setup error | `[Firebase] Sign-in failed` or `Database error` in the log, retried every 30 s | See Errors below |

Saved across reboots: both lists, weather city, 24-hour time, brightness, dark mode, power saving on/off, idle time, dim/off, dim level (see `parameters.md`).

## Serial log (115200 baud)

| Tag | Example |
|---|---|
| `[Boot]` | `Smart Display starting` |
| `[WiFi]` | `Connected, IP: …` · `Disconnected` · `Not connected - setup portal "SmartDisplay-Setup" is open` · `Continuing offline` · `Retrying saved network` |
| `[Time]` | `NTP started` · `Synced: 2026-10-09 14:32` |
| `[Weather]` | `City -> Eilat` · `Updated (Eilat) in 1450 ms` · `Retrying once` · `Skipped - no WiFi` · `HTTP error: <code> (after … ms)` · `JSON parse failed` |
| `[UI]` | `Home -> Settings` · `Home -> Weather` · `Home -> Time` · `Settings -> Home` |
| `[Settings]` | `Brightness 78%` · `24-hour time off` · `Power saving on` · `Idle after 5 min` · `When idle: dim` · `Dim to 10%` |
| `[Idle]` | `Entering idle (dim to 10%)` · `Entering idle (screen off)` · `Wake` |
| `[Storage]` | `city = 4` (a setting was saved) |
| `[Lists]` | `Loaded: 2 shopping, 0 to-do items` · `Starting empty: 0 shopping, 0 to-do items` · `Shopping list (2): milk, eggs` · `Could not save to flash` |
| `[Firebase]` | `Started` · `Signed in (free memory: …)` · `Listening for commands` · `Skipped command #4 from before startup` · `Command #5: add "milk" (shopping) -> Added milk to your shopping list` · `Login renewed` · `Reconnected` |

## Errors

| Error | Cause | Fix |
|---|---|---|
| `[Weather] HTTP error: <code>` | Open-Meteo unreachable or no internet behind the WiFi (`-11` = no response within 5 s) | Retried once right away, then every 30 s |
| `[Firebase] Sign-in failed (400): INVALID_LOGIN_CREDENTIALS` | Wrong email or password in `secrets.h`, or the user doesn't exist | Check Authentication → Users in the Firebase console |
| `[Firebase] Sign-in failed (400): OPERATION_NOT_ALLOWED` | Email/Password sign-in is turned off | Authentication → Sign-in method → Email/Password → Enable |
| `[Firebase] Sign-in failed (400): API key not valid…` | Wrong `FIREBASE_API_KEY` in `secrets.h` | Copy `apiKey` from Project settings → General → Your apps |
| `[Firebase] Sign-in failed (400): TOO_MANY_ATTEMPTS_TRY_LATER` | Too many failed sign-ins | Fix the login, wait a few minutes |
| `[Firebase] Database error 401: … Permission denied …` | Database rules don't allow signed-in users | Realtime Database → Rules → `".read": "auth != null", ".write": "auth != null"` |
| `[Firebase] Database error 404: …` | Wrong `FIREBASE_DATABASE_URL` | Copy the link from Realtime Database → Data |
| `[Firebase] No connection to the database (-1)` | Internet down behind the WiFi | Retried every second; logged once |
| `[Firebase] Could not write /status (…), retrying` | Reply to the app failed | Retried every second until it works or a newer command arrives |
| `Stack canary watchpoint triggered (firebase)` (board restarts) | The Firebase task ran out of stack | Raise `FIREBASE_TASK_STACK` in `firebase_sync.h` |
| App says "not confirmed" | Display off, offline, or not signed in, for more than 60 s | Check the log; commands sent while the display was off are skipped at startup |

## Lists, voice commands, calendar

| Log / state | Meaning | What to do |
|---|---|---|
| `[Lists] Ticked off "milk" (removed in 2 s unless tapped again)` · `Kept "milk"` · `Removed "milk" (tapped)` | Item tapped on the Lists screen | Tap again within 2 s to keep it |
| `[Firebase] Lists saved to /lists` | Lists copied to Firebase (after a change, or once WiFi is back after an offline change) | — |
| Pop-up at the top, e.g. "Added milk to your shopping list" (red if it failed) | A voice command arrived; the list it changed is open | Goes back home after 30 s untouched |
| `[Calendar] Not set up: add CALENDAR_ID and CALENDAR_API_KEY to secrets.h` | No calendar configured | README → Google Calendar |
| `[Calendar] Updated: 3 upcoming events` | Events loaded (every 10 min) | — |
| `[Calendar] Calendar screen opened: checking for new events` | The Calendar screen was opened: events are checked right away (at most every 15 s) | — |
| `[Calendar] Error 404: Not Found` | Wrong Calendar ID or calendar not public | Copy the ID again; Make available to public |
| `[Calendar] Error 400/403: API key not valid / API disabled` | Wrong key, or Google Calendar API not enabled | Credentials / Library in console.cloud.google.com |
| Items or events show `???` | Characters the fonts don't have (emoji, Arabic...). English and Hebrew are supported | Use English or Hebrew |
