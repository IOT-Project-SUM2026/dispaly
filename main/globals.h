#ifndef GLOBALS_H
#define GLOBALS_H
#include <FS.h>  // must come before TFT_eSPI, which hides the global FS that WebServer (WiFiManager) needs
#include <TFT_eSPI.h>

// ============================================================
// Screen dimensions (existing names, kept consistent everywhere)
// ============================================================
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// ============================================================
// Color palette (deep navy + electric cyan)
// ============================================================
#define UI_BG          0x08A2   // #0B0F14
#define UI_SURFACE     0x12C4   // #141A22
#define UI_TEXT        0xE75E   // #E6EAF1
#define UI_SECONDARY   0x44D4   // #8993A4
#define UI_ACCENT      0x05BF   // #00C2FF
#define UI_ACCENT_DARK 0x03F5   // #007FA8
#define UI_TRACK       0x29A9   // #273445
#define UI_OFFLINE     0xF2CD   // #FF5C67

// ============================================================
// Gear icon (top-right, opens Settings)
// ============================================================
#define GEAR_X 300
#define GEAR_Y 20
#define GEAR_R 8

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
#define SLIDER_H       8
#define SLIDER_MIN_X   56
#define SLIDER_MAX_X   288

// ============================================================
// Screen state
// ============================================================
enum Screen { HOME, SETTINGS, WEATHER };

extern TFT_eSPI tft;
extern Screen currentScreen;
extern int brightness;

// Forward declarations (implemented in display_ui.h / settings.h / weather.h)
void drawHomeScreen();
void drawSettingsScreen(int brightness);
void drawWeatherScreen();
void initBrightness();
void drawWiFiIcon(int cx, int cy, bool connected);
void drawBackButton();

#endif
