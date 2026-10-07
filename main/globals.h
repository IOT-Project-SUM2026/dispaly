#ifndef GLOBALS_H
#define GLOBALS_H
#include <FS.h>  // must come before TFT_eSPI, which hides the global FS that WebServer (WiFiManager) needs
#include <TFT_eSPI.h>  // LVGL's display backend (lv_tft_espi)
#include <lvgl.h>

// ============================================================
// Screen dimensions (landscape)
// ============================================================
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ============================================================
// Color palette (deep navy + electric cyan)
// ============================================================
#define UI_BG          lv_color_hex(0x0B0F14)
#define UI_SURFACE     lv_color_hex(0x141A22)
#define UI_TEXT        lv_color_hex(0xE6EAF1)
#define UI_SECONDARY   lv_color_hex(0x8993A4)
#define UI_ACCENT      lv_color_hex(0x00C2FF)
#define UI_ACCENT_DARK lv_color_hex(0x007FA8)
#define UI_TRACK       lv_color_hex(0x273445)
#define UI_OFFLINE     lv_color_hex(0xFF5C67)

// ============================================================
// Gear button (top-right, opens Settings)
// ============================================================
#define GEAR_X 300
#define GEAR_Y 20

// ============================================================
// WiFi icon (top-left, all screens)
// ============================================================
#define WIFI_ICON_X 20
#define WIFI_ICON_Y 20

// ============================================================
// Back button (circular, Settings + Weather screens)
// ============================================================
#define BACK_CX 30
#define BACK_CY 58
#define BACK_R  18

// ============================================================
// Home weather card
// ============================================================
#define WEATHER_CARD_X 40
#define WEATHER_CARD_Y 158
#define WEATHER_CARD_W 240
#define WEATHER_CARD_H 50

// ============================================================
// Brightness slider (Settings screen)
// ============================================================
#define SLIDER_X       52
#define SLIDER_Y       160
#define SLIDER_W       240

extern int brightness;

#endif
