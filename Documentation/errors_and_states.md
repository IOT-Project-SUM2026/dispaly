# System states and errors

> Placeholder. Extend this as features are added (Firebase, lists, calendar…).

## States

| State | What the screen shows | What to do |
|---|---|---|
| WiFi setup | Static noise (nothing drawn yet) | Join `SmartDisplay-Setup` from a phone and choose a network |
| Online | Home screen, normal WiFi icon | — |
| Offline | WiFi icon in red with an X | Check the router; the last loaded data stays on screen |
| Weather not loaded | Default weather values | Check WiFi; weather is refetched every 10 minutes. Serial log shows `Weather HTTP error` / `Weather JSON parse failed` |

## Errors

| Error | Cause | Fix |
|---|---|---|
| WiFi setup timeout | No network configured in time | The board restarts and opens the setup portal again |
| _TBD_ | | |
