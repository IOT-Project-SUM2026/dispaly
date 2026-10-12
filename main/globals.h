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
// Layout grid (design system, see Documentation/ui_design.md)
// Spacing scale: 4 / 8 / 12 / 16.
// ============================================================
#define MARGIN     12   // left/right screen margin
#define GAP        8    // between cards
#define HEADER_H   44   // header bar on every screen except Home
#define CONTENT_Y  48   // content starts below the header
#define TOUCH_MIN  44   // smallest tap target (resistive touch)
#define ROW_H      44   // list / settings row height
#define COL_W      ((SCREEN_WIDTH - 2 * MARGIN - GAP) / 2)  // 144: half-width card

// ============================================================
// Colors: one struct per theme (src/theme/theme.h). Screens never
// use colors directly; they use the shared styles in theme.h,
// which read these.
// ============================================================
struct Theme {
  uint32_t bg, surface, surfaceHi, border, text, secondary, accent, accentPressed, onAccent, danger;
  uint32_t sun, cloud;  // weather icons
};
extern const Theme* theme;

extern int brightness;

// Forward declarations
void applyBrightness(int value);  // settings.h, also used by idle.h
void requestRefresh();            // display_ui.h: rebuild the open screen after the current touch

#endif
