# System states and errors

> Placeholder. Extend this as features are added (Firebase, lists, calendar…).

## States

| State | What the screen shows | What to do |
|---|---|---|
| No WiFi configured / not reachable | Home screen with the offline WiFi icon; the `SmartDisplay-Setup` portal runs in the background | Join `SmartDisplay-Setup` from a phone and choose a network |
| Online | Home screen, normal WiFi icon | — |
| Offline | WiFi icon in red with an X (updated within ~2 s on any screen) | Check the router; the last loaded data stays on screen |
| Weather not loaded | "Loading weather..." on the home card | Waits for WiFi, then retries every 30 s until it succeeds (afterwards every 10 min) |

## Serial log (115200 baud)

| Tag | Example |
|---|---|
| `[Boot]` | `Smart Display starting` |
| `[WiFi]` | `Connected, IP: …` · `Disconnected` · `Not connected - setup portal "SmartDisplay-Setup" is open` · `Continuing offline` |
| `[Weather]` | `Updated` · `Skipped - no WiFi` · `HTTP error: <code>` · `JSON parse failed` |
| `[UI]` | `Home -> Settings` · `Settings -> Home` · `Home -> Weather` · `Weather -> Home` |
| `[Settings]` | `Brightness 78%` |

## Errors

| Error | Cause | Fix |
|---|---|---|
| `[Weather] HTTP error: <code>` | Open-Meteo unreachable or no internet behind the WiFi | Retried automatically |
| _TBD_ | | |
