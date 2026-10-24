#ifndef HOME_LAYOUT_H
#define HOME_LAYOUT_H
#include <stdint.h>

// ============================================================
// Home screen widgets: which are shown and which one is big.
// Set in Settings > HOME SCREEN, saved in flash.
//
// The widgets keep their order (Clock, Weather / Lists, Next event).
// Hidden widgets leave no gap: the others grow into the space.
// A Big widget gets a whole row (full width) and more height.
// No drawing here (home.h draws), so the layout can be tested on a computer.
// ============================================================

enum HomeWidget { HW_CLOCK, HW_WEATHER, HW_LISTS, HW_NEXT, HW_COUNT };
const char* const homeWidgetNames[HW_COUNT] = {"Clock", "Weather", "Lists", "Next event"};

// The area below the top row (greeting, WiFi, gear): x 12..308, y 48..232
#define HOME_AREA_X     12
#define HOME_AREA_Y     48
#define HOME_AREA_W     296
#define HOME_AREA_H     184
#define HOME_GAP        8
#define HOME_MIN_ROW_H  48      // the smallest version of every widget fits in 48 px
#define HOME_HALF_W     ((HOME_AREA_W - HOME_GAP) / 2)  // 144
// Greeting off: the top row's left part (x 12..156) is free; the WiFi icon
// and the gear stay on its right. A half-width widget in the top-left
// corner grows up into it.
#define HOME_TOP_FREE_Y 8

uint8_t homeShownMask = 0x0F;   // bit per HomeWidget; all shown
int8_t homeBigWidget = -1;      // -1 = none
bool homeGreetingShown = true;  // "Good morning" / "Good afternoon" in the top row

struct WidgetBox {
  bool shown;
  int16_t x, y, w, h;
};

bool homeWidgetShown(int widget) {
  return widget >= 0 && widget < HW_COUNT && (homeShownMask & (1 << widget));
}

bool homeAnyWidgetShown() {
  return (homeShownMask & 0x0F) != 0;
}

// Row heights are shared out by weight: the top widgets (clock, weather) need
// less height than the bottom ones (lists, next event). With everything shown
// and nothing big, this gives the original layout: rows of 68 and 108 px.
static int homeRowWeight(int widget, bool big) {
  int base = (widget == HW_CLOCK || widget == HW_WEATHER) ? 68 : 108;
  return big ? base * 3 : base;  // big: 3 x, so it clearly gets most of the space
}

void computeHomeLayout(WidgetBox out[HW_COUNT]) {
  for (int i = 0; i < HW_COUNT; i++) out[i] = {false, 0, 0, 0, 0};
  int big = homeWidgetShown(homeBigWidget) ? homeBigWidget : -1;

  // Rows of 1 or 2 widgets. A big widget is alone in its row; two normal
  // widgets of the same group (top: clock + weather, bottom: lists + next) share a row.
  int8_t rowWidgets[HW_COUNT][2];
  uint8_t rowCount[HW_COUNT];
  int weight[HW_COUNT];
  int rows = 0;
  const int groups[2][2] = {{HW_CLOCK, HW_WEATHER}, {HW_LISTS, HW_NEXT}};
  for (int g = 0; g < 2; g++) {
    int pending = -1;
    for (int k = 0; k < 2; k++) {
      int w = groups[g][k];
      if (!homeWidgetShown(w)) continue;
      if (w == big) {
        if (pending >= 0) {  // the widget before it gets its own (full-width) row
          rowWidgets[rows][0] = pending; rowCount[rows] = 1; weight[rows++] = homeRowWeight(pending, false);
          pending = -1;
        }
        rowWidgets[rows][0] = w; rowCount[rows] = 1; weight[rows++] = homeRowWeight(w, true);
      } else if (pending >= 0) {
        rowWidgets[rows][0] = pending; rowWidgets[rows][1] = w; rowCount[rows] = 2;
        weight[rows++] = homeRowWeight(w, false);
        pending = -1;
      } else {
        pending = w;
      }
    }
    if (pending >= 0) {
      rowWidgets[rows][0] = pending; rowCount[rows] = 1; weight[rows++] = homeRowWeight(pending, false);
    }
  }
  if (rows == 0) return;

  // heights: by weight, at least HOME_MIN_ROW_H, all of the area used
  int available = HOME_AREA_H - HOME_GAP * (rows - 1);
  int totalWeight = 0;
  for (int r = 0; r < rows; r++) totalWeight += weight[r];
  int height[HW_COUNT];
  int used = 0;
  for (int r = 0; r < rows; r++) {
    height[r] = available * weight[r] / totalWeight;
    if (height[r] < HOME_MIN_ROW_H) height[r] = HOME_MIN_ROW_H;
    used += height[r];
  }
  // too much (minimums pushed it over): take it from the tallest rows
  while (used > available) {
    int tallest = 0;
    for (int r = 1; r < rows; r++) if (height[r] > height[tallest]) tallest = r;
    height[tallest]--;
    used--;
  }
  height[rows - 1] += available - used;  // rounding leftovers go to the last row

  int y = HOME_AREA_Y;
  for (int r = 0; r < rows; r++) {
    for (int k = 0; k < rowCount[r]; k++) {
      WidgetBox& b = out[rowWidgets[r][k]];
      b.shown = true;
      b.y = y;
      b.h = height[r];
      b.w = rowCount[r] == 2 ? HOME_HALF_W : HOME_AREA_W;
      b.x = HOME_AREA_X + (k == 1 ? HOME_HALF_W + HOME_GAP : 0);
    }
    y += height[r] + HOME_GAP;
  }

  if (!homeGreetingShown && rowCount[0] == 2) {
    WidgetBox& b = out[rowWidgets[0][0]];  // top-left widget, half width
    b.h += b.y - HOME_TOP_FREE_Y;
    b.y = HOME_TOP_FREE_Y;
  }
}
#endif
