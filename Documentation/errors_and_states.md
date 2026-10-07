# System states and errors

> Placeholder. Extend this as features are added (Firebase, lists, calendar…).

## States

| State | What the screen shows | What to do |
|---|---|---|
| No WiFi configured / not reachable | Home screen with the offline WiFi icon; the `SmartDisplay-Setup` portal runs in the background | Join `SmartDisplay-Setup` from a phone and choose a network |
| Online | Home screen, normal WiFi icon | — |
| Offline | WiFi icon in red with an X (updated within ~2 s on any screen) | Check the router; the last loaded data stays on screen |
| Weather not loaded | "Loading weather..." on the home card | Waits for WiFi, then retries every 30 s until it succeeds (afterwards every 10 min) |
| Voice commands: waiting for WiFi | Nothing on screen; no `[Firebase]` lines after `Started` | Firebase starts by itself once WiFi connects |
| Voice commands: listening | `[Firebase] Listening for commands` in the log | Speak a command in the app; the display replies within about a second |
| Voice commands: login or database setup error | `[Firebase] Sign-in failed` or `Database error` in the log, retried every 30 s | See Errors below |

## Serial log (115200 baud)

| Tag | Example |
|---|---|
| `[Boot]` | `Smart Display starting` |
| `[WiFi]` | `Connected, IP: …` · `Disconnected` · `Not connected - setup portal "SmartDisplay-Setup" is open` · `Continuing offline` |
| `[Weather]` | `Updated` · `Skipped - no WiFi` · `HTTP error: <code>` · `JSON parse failed` |
| `[UI]` | `Home -> Settings` · `Settings -> Home` · `Home -> Weather` · `Weather -> Home` |
| `[Settings]` | `Brightness 78%` |
| `[Lists]` | `Loaded: 2 shopping, 0 to-do items` · `Starting empty: 0 shopping, 0 to-do items` · `Shopping list (2): milk, eggs` · `Could not save to flash` |
| `[Firebase]` | `Started` · `Signed in (free memory: …)` · `Listening for commands` · `Skipped command #4 from before startup` · `Command #5: add "milk" (shopping) -> Added milk to your shopping list` · `Login renewed` · `Reconnected` |

## Errors

| Error | Cause | Fix |
|---|---|---|
| `[Weather] HTTP error: <code>` | Open-Meteo unreachable or no internet behind the WiFi | Retried automatically |
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
