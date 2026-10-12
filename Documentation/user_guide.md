# Smart Display: user guide

The Smart Display sits on a desk or a kitchen counter and shows the time, the weather, your
calendar and your shopping / to-do lists. You add and remove list items by speaking into the
phone app.

![Screens](../assets/screens_dark.png)

## 1. First start

1. Plug the display into a USB power adapter (5 V, 1 A). It starts in a few seconds.
2. Until it knows a WiFi network, the top of the screen shows **⚠ Offline**. On your phone,
   join the WiFi network **SmartDisplay-Setup**. A setup page opens (otherwise open
   `192.168.4.1` in the browser) → **Configure WiFi** → choose your network → enter the
   password → Save.
3. Within a few seconds the WiFi icon appears, the clock sets itself and the weather loads.
   The display remembers the network. If that network is gone for more than a minute (you
   moved the display), the setup network appears again so you can choose a new one.

iPhone hotspot: turn on **Maximize Compatibility** (the display only uses 2.4 GHz WiFi).

## 2. The screens

Tap a card on the home screen to open it; the **<** button at the top left goes back.

| Screen | What it shows | What you can do |
|---|---|---|
| **Home** | greeting, time and date, weather, list counts, the next calendar event | tap the clock, weather, Lists or event card to open it; ⚙ opens Settings |
| **Lists** | shopping list and to-do list | switch lists at the top; tap an item to tick it off (it disappears after 2 s, tap again to keep it) |
| **Calendar** | upcoming events by day; the event happening now is marked "Now" | scroll |
| **Weather** | current weather and 4-day forecast, time of the last update | tap the city button to choose Jerusalem, Tel Aviv, Haifa, Beer Sheba or Eilat |
| **World clock** | 6 cities with clock faces (dark face = night there) | - |
| **Settings** | Display, Clock, Power saving sections (tap a section title to open or close it) | brightness, dark mode, 24-hour time, power saving |

English and Hebrew text are both shown (Hebrew right to left).

## 3. Voice commands (phone app)

1. Open the Smart Display app on your phone (add it to the home screen for quick access).
2. **Hold** the button, say the command, **release**. Examples:
   - "Add milk to the shopping list" / "Buy eggs and bread"
   - "Add walking to my to-do list" / "Remove call mom"
   - Hebrew works too: "תוסיף חלב לרשימת הקניות"
3. The app shows what it understood ("Adding milk…") and, about a second later, the
   result: **Milk added successfully**, or "Your display couldn't complete this request".
4. On the display, the list opens with the new item highlighted and a message at the top
   (blue = done, red = not done) that says exactly what happened. It goes back to the home
   screen after 30 s.

| Message | Where | Meaning |
|---|---|---|
| "Added milk to your shopping list" | display | done |
| "milk is already on your shopping list" | display | nothing changed, it was there |
| "eggs isn't on your shopping list" | display | you asked to remove something that isn't on the list |
| "Your shopping list is full" | display | 20 items is the limit: tick some off first |
| "Could not understand a valid command" | app | say it again, naming the item (and the list if it isn't obvious) |
| "Not confirmed" | app | the display did not answer within a minute (off, or no WiFi). Check the list before trying again |
| "You're offline" | app | your phone has no internet |

## 4. What the indicators mean

| You see | Meaning | What to do |
|---|---|---|
| WiFi icon at the top | connected | - |
| **⚠ Offline** (red) | no WiFi. Lists, clock and settings still work; weather and calendar show the last data | wait, or set up WiFi again (section 1) |
| "Updated 10:30" in red on the Weather screen | the weather is from 10:30 and the display is offline | - |
| "Loading..." | first download after starting | wait a few seconds |
| "Waiting for WiFi" / "Getting the time..." under the clock | the clock is not set yet | connect WiFi |
| "Calendar not set up" | no calendar was configured | see the README (developer setup) |
| "Couldn't load events" | the calendar could not be read | it retries every minute; check the calendar settings if it stays |
| Dark or dim screen | power saving | touch the screen; the first touch only wakes it |

## 5. Settings

- **Brightness**: slider; it never goes fully dark.
- **Dark mode**: dark or white theme.
- **24-hour time**: 14:30 or 2:30 PM.
- **Power saving**: after 1-30 min without touch, dim the screen (5-50 %) or turn it off.

All settings and both lists are saved and come back after a power cut.

## 6. If something is wrong

| Problem | Try |
|---|---|
| Touch hits the wrong place | press with a fingernail; if it is always off in one direction, the touch needs calibrating (developer: `Documentation/parameters.md`) |
| A voice command didn't arrive | check the WiFi icon on the display; send it again only after checking the list |
| The display restarts by itself | use a 5 V / 1 A adapter instead of a laptop USB port |
| Nothing on the screen | check the cable and the adapter; press the RST button on the back |
