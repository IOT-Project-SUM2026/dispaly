# UI design

The rules every screen follows. Screen: 320 × 240, viewed from about arm's length, resistive touch.

## Layout grid

| Rule | Value | Why |
|---|---|---|
| Side margin | 12 px | |
| Gap between cards | 8 px | half-width card = (320 − 2·12 − 8) / 2 = 144 px |
| Spacing scale | 4 / 8 / 12 / 16 | |
| Header bar | 44 px; content starts at y 48 | back button, title, time, WiFi on every screen except Home |
| Smallest tap area | 44 × 44 px | resistive touch needs a firm press and drifts a few pixels |
| List / settings row | 44 px | calendar rows are 36 px: events aren't tapped |
| Corner radius | 12 card · 10 row · full pill | |

Constants: `MARGIN`, `GAP`, `HEADER_H`, `CONTENT_Y`, `TOUCH_MIN`, `ROW_H`, `COL_W` in `main/globals.h`.

## Type

| Role | Font | Used for |
|---|---|---|
| Display | Ubuntu Medium 38 px, digits only (`font_display.c`) | clock, current temperature |
| Value | Montserrat 28 | list counts, home temperature |
| Title | Montserrat 20 + Hebrew | screen titles, greeting, list items |
| Body | Montserrat 14 + Hebrew | event titles, settings labels, card text |
| Caption | Montserrat 12, letter-spaced capitals | section labels: TODAY, DISPLAY, LISTS |
| Small | Montserrat 12 | hints, "Updated 10:30", AM/PM |

Hebrew: `font_hebrew_14.c` / `font_hebrew_20.c` hold only the Hebrew letters and fall back to
Montserrat for everything else. `LV_USE_BIDI=1` (build_opt.h) shows Hebrew right to left; text
stays left-aligned so mixed lists line up.

## Colors

Both palettes are in `main/src/theme/theme.h`. Text contrast is at least 4.5:1 (WCAG AA) in both.
Light mode is all white with outlined cards: a tinted background looked grey on the board's TN screen.

| Color | Dark | Light |
|---|---|---|
| Background | `#0B0F14` | `#FFFFFF` |
| Cards, rows | `#171F2A` | `#FFFFFF`, outlined |
| Pressed, tracks | `#222C39` | `#EEF2F6` |
| Borders, dividers | `#2A3747` | `#CDD5DF` |
| Text | `#E8ECF2` (15.6:1) | `#141A22` (17.5:1) |
| Secondary text | `#8B95A7` (5.6:1) | `#586274` (6.2:1) |
| Accent | `#00C2FF` (8.2:1) | `#0072B5` (5.2:1) |
| Text on accent | `#04121A` | `#FFFFFF` |
| Danger / offline | `#FF6B75` | `#C8323A` |
| Weather sun / cloud | `#FFC24B` / `#C5CDD8` | `#F0A202` / `#9AA4B3` |

## Rules for the code

- **Shared styles only.** Every color comes from a `lv_style_t` in `theme.h` (`stCard`, `stTitle`,
  `stAccent`, ...), never from a per-object color. Dark mode rewrites those styles and calls
  `lv_obj_report_style_change(NULL)`: nothing is rebuilt.
- **States instead of rebuilds.** A ticked list item is `LV_STATE_CHECKED` (styles for that state
  cross it out); a night dial is `LV_STATE_USER_1`; offline text is `LV_STATE_USER_1` on the WiFi label.
- **Rebuild only what changed.** New weather: the weather content only. New events: the calendar.
  Home: `refreshHomeCard()` rebuilds one card. Labels that change often (clocks) are updated in place.
- **Draw buffer:** 1/10 of the screen (15 KB), one buffer. `lv_tft_espi` sends pixels without DMA,
  so a second buffer wouldn't help; a bigger one would take RAM the secure connections need.
- **Motion:** only the voice-command message slides in (180 ms). Screen changes are instant.

## Screens

| Screen | Layout |
|---|---|
| Home | grey greeting on the top row (WiFi and gear on the right); clock (left, x 12) with the date in blue under it, lined up with the weather card (164, 48, 144 × 68); Lists card (12, 124) and Next event card (164, 124), 144 × 108 |
| Lists | Shopping / To-do switch in the top bar; 44 px rows with 20 px text; 4 rows visible; tap = tick off; last row "Clear list" (tap twice) |
| Calendar | events grouped under TODAY / TOMORROW, 36 px rows, 14 px titles; the event that is on now has a blue bar |
| Weather | city button (pin, name, arrow, blue outline) with "Updated 10:30" next to it; icon, temperature, condition, high / low; 4 forecast cards |
| World Clock | 2 × 3 tiles: `lv_scale` dial (light by day, dark at night), city, time |
| Settings | folding sections DISPLAY (brightness, dark mode), CLOCK (24-hour time), POWER SAVING |
