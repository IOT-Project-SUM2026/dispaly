# Pinout / wiring: ESP32-2432S028R (CYD)

The screen, touch controller and backlight are built into the board, so **no external
wiring is needed**. Just connect USB.

## Screen (ILI9341, 320×240): HSPI bus

| Signal | GPIO |
|---|---|
| MISO | 12 |
| MOSI | 13 |
| SCLK | 14 |
| CS | 15 |
| DC | 2 |
| RST | not connected (-1) |
| Backlight (PWM) | 21 |

Configured in [`main/build_opt.h`](../main/build_opt.h). Brightness PWM is in
[`main/src/settings/settings.h`](../main/src/settings/settings.h).

## Touch (XPT2046): VSPI bus

| Signal | GPIO |
|---|---|
| IRQ | 36 |
| MOSI | 32 |
| MISO | 39 |
| CLK | 25 |
| CS | 33 |

Configured in [`main/display_ui.h`](../main/display_ui.h).

The screen and touch controller use separate SPI buses, which is why `build_opt.h` sets
`USE_HSPI_PORT` for the screen.

## Further reading

- [ESP32 Cheap Yellow Display community repo](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display): pinouts, board variants, examples
- [Random Nerd Tutorials: Getting started with the CYD](https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/)
