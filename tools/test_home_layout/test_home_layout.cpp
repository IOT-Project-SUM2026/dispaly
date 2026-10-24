// Host test of main/src/home/home_layout.h: every combination of shown
// widgets (16) and big widget (none or one of 4). Checks that widgets stay
// inside the area, never overlap, are at least 48 px tall, fill the whole
// area, and that the default is the original layout.
#include <cstdio>
#include "../../main/src/home/home_layout.h"

int main() {
  int fails = 0, layouts = 0;
  for (int greeting = 1; greeting >= 0; greeting--)
  for (int mask = 0; mask < 16; mask++) {
    for (int big = -1; big < HW_COUNT; big++) {
      homeShownMask = mask; homeBigWidget = big; homeGreetingShown = greeting;
      int top = greeting ? HOME_AREA_Y : HOME_TOP_FREE_Y;
      WidgetBox b[HW_COUNT];
      computeHomeLayout(b);
      layouts++;
      int area = 0;
      for (int i = 0; i < HW_COUNT; i++) {
        bool want = mask & (1 << i);
        if (b[i].shown != want) { fails++; printf("mask %d big %d: widget %d shown=%d\n", mask, big, i, b[i].shown); }
        if (!b[i].shown) continue;
        // the WiFi icon and gear: x 164..320, y 0..44 - never covered
        if (b[i].y < HOME_AREA_Y && b[i].x + b[i].w > HOME_AREA_X + HOME_HALF_W) {
          fails++; printf("greeting %d mask %d big %d: widget %d under the icons\n", greeting, mask, big, i);
        }
        if (b[i].x < HOME_AREA_X || b[i].y < top || b[i].x + b[i].w > HOME_AREA_X + HOME_AREA_W ||
            b[i].y + b[i].h > HOME_AREA_Y + HOME_AREA_H || b[i].h < HOME_MIN_ROW_H) {
          fails++; printf("mask %d big %d: widget %d out of area or too small (%d,%d %dx%d)\n", mask, big, i, b[i].x, b[i].y, b[i].w, b[i].h);
        }
        if (big == i && b[i].w != HOME_AREA_W) { fails++; printf("mask %d: big widget %d not full width\n", mask, i); }
        area += b[i].w * b[i].h;
        for (int j = 0; j < i; j++) {
          if (!b[j].shown) continue;
          bool apart = b[i].x >= b[j].x + b[j].w || b[j].x >= b[i].x + b[i].w || b[i].y >= b[j].y + b[j].h || b[j].y >= b[i].y + b[i].h;
          if (!apart) { fails++; printf("mask %d big %d: widgets %d and %d overlap\n", mask, big, i, j); }
        }
      }
      // the widgets plus the gaps cover the whole area: the bottom edge reaches y 232
      int bottom = 0;
      for (int i = 0; i < HW_COUNT; i++) if (b[i].shown && b[i].y + b[i].h > bottom) bottom = b[i].y + b[i].h;
      if (mask && bottom != HOME_AREA_Y + HOME_AREA_H) { fails++; printf("mask %d big %d: bottom %d\n", mask, big, bottom); }
      if (mask == 15 && greeting) {
        printf("all shown, big=%-10s:", big < 0 ? "none" : homeWidgetNames[big]);
        for (int i = 0; i < HW_COUNT; i++) printf("  %s %dx%d", homeWidgetNames[i], b[i].w, b[i].h);
        printf("\n");
      }
    }
  }
  homeShownMask = 15; homeBigWidget = -1; homeGreetingShown = false;
  WidgetBox g[HW_COUNT]; computeHomeLayout(g);
  printf("greeting off, default: Clock %dx%d at y %d (grew into the free top-left), Weather %dx%d at y %d\n",
         g[HW_CLOCK].w, g[HW_CLOCK].h, g[HW_CLOCK].y, g[HW_WEATHER].w, g[HW_WEATHER].h, g[HW_WEATHER].y);
  if (g[HW_CLOCK].y != HOME_TOP_FREE_Y || g[HW_WEATHER].y != HOME_AREA_Y) { fails++; printf("greeting off: top-left widget did not grow\n"); }
  homeGreetingShown = true;
  WidgetBox b[HW_COUNT]; computeHomeLayout(b);
  bool original = b[HW_CLOCK].h == 68 && b[HW_LISTS].y == 124 && b[HW_LISTS].h == 108 && b[HW_WEATHER].x == 164 && b[HW_WEATHER].w == 144;
  if (!original) { fails++; printf("default is not the original layout\n"); }
  printf("%d layouts checked, %s\n", layouts, fails ? "FAILED" : "all OK (default = original 68 + 108 px rows)");
  return fails ? 1 : 0;
}
