# Smart Display: user guide

The Smart Display sits on a desk or a kitchen counter and shows the time, the weather, your
calendar and your shopping / to-do lists. You add and remove list items by speaking into the
phone app.

![Screens](../assets/screens_dark.png)

## 1. First start

1. Plug the display into a USB power adapter (5 V, 1 A). It starts in a few seconds.
2. Until it knows a WiFi network, the top of the screen shows **⚠ No WiFi**. On your phone,
   join the WiFi network **SmartDisplay-Setup** (password **smartdisplay**; it is also shown on the display in
   Settings → Device while the display has no WiFi). A setup page opens (otherwise open
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
| **Home** | greeting, then 4 widgets: clock, weather, lists, next event (you choose which, and which one is big) | tap a widget to open its screen; ⚙ opens Settings |
| **Lists** | shopping list and to-do list | switch lists at the top; tap an item to tick it off (a pop-up says it is done; it disappears after 3 s, tap it again to keep it). **Clear list** at the end of the list: tap once, then tap again within 4 s to remove every item |
| **Calendar** | upcoming events by day; the event happening now is marked "Now" | scroll |
| **Weather** | current weather and 4-day forecast, time of the last update | tap the city button to choose Jerusalem, Tel Aviv, Haifa, Beer Sheba or Eilat |
| **World clock** | 6 cities with clock faces (dark face = night there) | - |
| **Settings** | Display, Home screen, Clock, Power saving, Device sections (tap a section title to open or close it) | brightness, dark mode, Home widgets, 24-hour time, power saving, device status |

English and Hebrew text are both shown (Hebrew right to left).

## 3. Voice commands (phone app)

1. Open the Smart Display app on your phone (add it to the home screen for quick access).
2. **Hold** the button, say the command, **release**. A command is: **add** or **remove**,
   **one item**, and **the list** (shopping or to-do). Examples:
   - "Add milk to my shopping list" / "Put eggs on the shopping list"
   - "Add walking to my to-do list" / "Remove call mom from my tasks"
   - Hebrew works too: "תוסיף חלב לרשימת הקניות", "תמחק הליכה מרשימת המשימות"
   Without a list ("add milk"), with two items ("add milk and eggs") or without "add" / "remove"
   ("milk to my list"), the app explains what to say instead and nothing is changed.
3. The app shows what it understood ("Adding milk…") and, about a second later, the
   result, e.g. **Added milk to your shopping list** or **Milk is already on your shopping list**.
   This works the same when the display is off or has no WiFi: the change is kept and
   the display updates by itself when it is back.
4. Changed your mind? Tap **Undo** under the result: it removes what you just added
   (or adds back what you removed).
5. On the display, the list opens with the new item highlighted and a message at the top
   (blue with ✓ = done, red with ⚠ = not done) that says exactly what happened. It goes back
   to the home screen after 30 s. If someone is using another screen on the display (e.g.
   Settings), only the message appears, so they aren't interrupted.

| Message | Where | Meaning |
|---|---|---|
| "Added milk to your shopping list" | app and display | done |
| "milk is already on your shopping list" | app and display | nothing changed, it was there |
| "eggs isn't on your shopping list" | app and display | you asked to remove something that isn't on the list |
| "Your shopping list is full" | app and display | 20 items is the limit: tick some off first |
| "Could not understand a valid command" | app | say it again, naming the item (and the list if it isn't obvious) |
| "The voice service is busy right now" | app | Google's speech service didn't answer in time; try again in a moment |
| "Too many requests. Wait a minute" | app | more than 15 commands in a minute (protects the service); wait a minute |
| "You're offline" | app | your phone has no internet |

## 4. What the indicators mean

| You see | Meaning | What to do |
|---|---|---|
| WiFi icon at the top | connected | - |
| **⚠ No WiFi** (red; on Home it replaces the greeting) | no WiFi. Lists, clock and settings still work; weather and calendar show the last data | wait, or set up WiFi again (section 1) |
| **⚠ No internet** (red) | WiFi works, but the internet behind it doesn't (e.g. the router lost its connection) | check the router; the display recovers by itself |
| "Updated 10:30" in red (Weather and Calendar screens), "From 10:30" in red on the Home weather | the data is old: weather not updated for 30 min, calendar for 1 hour (offline, or the service is down). After a restart without internet the last saved weather and events are shown this way | it updates by itself; if it stays red with WiFi working, check Settings → Device |
| "Loading..." | first download after starting | wait a few seconds |
| "Waiting for WiFi" / "Getting the time..." under the clock | the clock is not set yet | connect WiFi |
| "Calendar not set up" | no calendar was configured | Settings → Device → Setup page (section 5) |
| "Voice commands are off" on an empty list | the voice login isn't set up or is wrong | Settings → Device shows the reason; fix it on the Setup page |
| "Couldn't load events" | the calendar could not be read | it retries every minute; check the calendar settings if it stays |
| Dark or dim screen | power saving | touch the screen; the first touch only wakes it |

## 5. Settings

- **Brightness**: slider; it never goes fully dark.
- **Dark mode**: dark or white theme.
- **Home screen**: a switch for the greeting ("Good morning") and for each widget (Clock, Weather,
  Lists, Next event), and **Big widget**. With the greeting off, the top-left widget grows up into its
  space. Connection problems ("No internet") always show at the top: in the greeting row, or next to
  the WiFi icon when the greeting is off.
  A hidden widget leaves no gap: the others grow into its place. The big widget gets a whole
  row and most of the height (e.g. a big clock with large digits, or the lists with their items).
  Hiding Weather or Next event also stops downloading them. Voice commands still work when
  Lists is hidden: the list opens when a command arrives. Changes show when you go back Home.
- **24-hour time**: 14:30 or 2:30 PM.
- **Power saving**: after 1-30 min without touch, dim the screen (5-50 %) or turn it off.
- **Device** (folded; tap to open): internet, WiFi network and signal, whether voice commands work,
  how long the display has been running, why it last restarted, free memory (updated every 2 s).
  Without WiFi it also shows the setup network and its password.
- **Reset display** (in Device): tap it, then tap again within 5 s to erase everything saved on the
  display (WiFi, setup page, lists, settings) and restart. Use it before giving the display away.
- **Setup page** (in Device): tap it, then open the address it shows (e.g. `http://192.168.1.23`) on a
  phone on the same WiFi → **Setup**. Here you enter the voice-command login and the Google Calendar.
  Passwords and keys are never shown: leave them empty to keep them. Saving restarts the display.
  The page closes by itself after 10 minutes.

All settings and both lists are saved and come back after a power cut.

## 6. If something is wrong

| Problem | Try |
|---|---|
| Touch hits the wrong place | press with a fingernail; if it is always off in one direction, the touch needs calibrating (developer: `Documentation/parameters.md`) |
| A voice command isn't on the display yet | the display is offline: it updates by itself when WiFi is back. If you also ticked the same item on the screen, the later change wins |
| The display restarts by itself | Settings → Device → Last restart says why. "Weak power supply": use a 5 V / 1 A adapter instead of a laptop USB port. "Watchdog": the display froze and restarted itself; your lists and settings are kept |
| Nothing on the screen | check the cable and the adapter; press the RST button on the back |
