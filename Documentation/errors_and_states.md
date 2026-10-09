# System states and errors

> Placeholder. Extend this as features are added (Firebase, lists, calendar…).

## States

| State | What the screen shows | What to do |
|---|---|---|
| No WiFi configured / not reachable | Home screen with the offline WiFi icon; the `SmartDisplay-Setup` portal runs in the background | Join `SmartDisplay-Setup` from a phone and choose a network |
| Online | Home screen, normal WiFi icon | — |
| Offline | WiFi icon in red with an X (updated within ~2 s on any screen) | Check the router; the last loaded data stays on screen |
| Clock not synced yet | `--:--` and "Waiting for time sync" on the home clock, `--:--` on the world clock | Connect WiFi; NTP syncs within seconds. After that the clock keeps time even offline |
| Weather not loaded | "Loading &lt;city&gt; weather..." on the home card, "Loading..." on the weather screen | Waits for WiFi, then retries every 30 s until it succeeds (afterwards every 10 min) |
| Power saving (idle) | Screen dimmed or backlight off after the chosen time without touch | Touch the screen; the first touch only wakes it |

Saved across reboots: weather city, 24-hour time, brightness, power saving on/off, idle time, dim/off, dim level.

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

## Errors

| Error | Cause | Fix |
|---|---|---|
| `[Weather] HTTP error: <code>` | Open-Meteo unreachable or no internet behind the WiFi (`-11` = no response within 5 s) | Retried once right away, then every 30 s |
| _TBD_ | | |
