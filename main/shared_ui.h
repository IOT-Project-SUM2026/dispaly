#ifndef SHARED_UI_H
#define SHARED_UI_H
#include "globals.h"
#include "src/theme/theme.h"
#include <WiFi.h>
#include <lvgl_private.h>  // lv_text_get_width() (makeSingleLine)

// ============================================================
// Building blocks shared by all screens. They only attach the
// shared styles from theme.h (no per-object colors), so a theme
// switch never needs a rebuild.
// ============================================================

// The fonts have English letters, digits, symbols and Hebrew (font_hebrew_*.c).
// Copies `text` into `out`, replacing any other character (e.g. emoji, Arabic)
// with '?' so it doesn't show as an empty box.
void toScreenText(const char* text, char* out, size_t outSize) {
  size_t n = 0;
  const uint8_t* p = (const uint8_t*)text;
  while (*p && n + 1 < outSize) {
    if (*p < 0x80) {
      out[n++] = (*p < 32 || *p == 127) ? ' ' : (char)*p;
      p++;
      continue;
    }
    // multi-byte UTF-8 character: decode it
    int len = (*p >= 0xF0) ? 4 : (*p >= 0xE0) ? 3 : (*p >= 0xC0) ? 2 : 1;
    uint32_t code = (len == 2) ? (*p & 0x1F) : (len == 3) ? (*p & 0x0F) : (*p & 0x07);
    int i = 1;
    for (; i < len && (p[i] & 0xC0) == 0x80; i++) code = (code << 6) | (p[i] & 0x3F);
    bool hebrew = (code >= 0x05D0 && code <= 0x05EA) || code == 0x05BE || code == 0x05F3 || code == 0x05F4 || code == 0x20AA;
    if (hebrew && i == len && n + len < outSize) {
      memcpy(out + n, p, len);
      n += len;
    } else if (code >= 0x0591 && code <= 0x05C7) {
      // Hebrew vowel marks (nikud): skip, the letters are enough
    } else {
      out[n++] = '?';
    }
    p += i;
  }
  out[n] = '\0';
}

// Keeps a label to `lines` lines of `width` px, ending in "..." when the text is too long.
// One line: shortened here instead of by LVGL, because LVGL puts the "..." on the
// wrong side of Hebrew (right-to-left) text. Call it after the text is set.
void makeSingleLine(lv_obj_t* label, int width, int lines = 1) {
  const lv_font_t* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
  int lineSpace = lv_obj_get_style_text_line_space(label, LV_PART_MAIN);
  lv_obj_set_width(label, width);
  lv_obj_set_height(label, lines * lv_font_get_line_height(font) + (lines - 1) * lineSpace);
  if (lines > 1) {
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    return;
  }
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
  lv_text_attributes_t attr;
  lv_text_attributes_init(&attr);
  attr.letter_space = lv_obj_get_style_text_letter_space(label, LV_PART_MAIN);
  const lv_text_attributes_t* letterSpace = &attr;
  const char* text = lv_label_get_text(label);
  uint32_t len = strlen(text);
  if (lv_text_get_width(text, len, font, letterSpace) <= width) return;

  // drop characters from the end (whole UTF-8 characters) until "text..." fits
  int dotsWidth = lv_text_get_width("...", 3, font, letterSpace);
  char buf[96];
  len = min<uint32_t>(len, sizeof(buf) - 4);
  memcpy(buf, text, len);
  while (len > 0) {
    len--;
    while (len > 0 && ((uint8_t)buf[len] & 0xC0) == 0x80) len--;  // start of that character
    if (lv_text_get_width(buf, len, font, letterSpace) + dotsWidth <= width) break;
  }
  while (len > 0 && buf[len - 1] == ' ') len--;
  strcpy(buf + len, "...");  // at the end of the text: on the left for Hebrew
  lv_label_set_text(label, buf);
}

// Objects that only show something: not clickable, pass taps to their parent
static void passClicks(lv_obj_t* obj) {
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
}

// Empty screen with the app background
lv_obj_t* createScreen() {
  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_add_style(scr, &stScreen, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

lv_obj_t* createLabel(lv_obj_t* parent, const char* text, lv_style_t* style) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_add_style(label, style, 0);
  return label;
}

// Invisible box for laying things out (no background, no padding)
lv_obj_t* createBox(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_pos(box, x, y);
  lv_obj_set_size(box, w, h);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  passClicks(box);
  return box;
}

// Vertical scrolling list (rows stacked with `gap` px between them)
lv_obj_t* createScrollList(lv_obj_t* parent, int x, int y, int w, int h, int gap) {
  lv_obj_t* list = lv_obj_create(parent);
  lv_obj_remove_style_all(list);
  lv_obj_set_pos(list, x, y);
  lv_obj_set_size(list, w, h);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, gap, 0);
  lv_obj_set_style_pad_bottom(list, 8, 0);  // the last row doesn't touch the screen edge
  lv_obj_set_style_pad_right(list, 6, 0);   // room for the scrollbar next to the rows
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);  // shown only when the content is longer
  lv_obj_add_style(list, &stScrollbar, LV_PART_SCROLLBAR);
  return list;
}

// Rounded card; tappable (darker while pressed) when a callback is given
lv_obj_t* createCard(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t onClick, lv_style_t* style = &stCard) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_remove_style_all(card);  // only our styles (no default-theme borders/padding)
  lv_obj_add_style(card, style, 0);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  if (onClick) {
    lv_obj_add_style(card, &stPressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(card, onClick, LV_EVENT_CLICKED, NULL);
  } else {
    passClicks(card);
  }
  return card;
}

lv_obj_t* createDivider(lv_obj_t* parent, int x, int y, int w) {
  lv_obj_t* line = createBox(parent, x, y, w, 1);
  lv_obj_add_style(line, &stDivider, 0);
  return line;
}

// Small label above a group ("TODAY", "SHOPPING")
lv_obj_t* createCaption(lv_obj_t* parent, const char* text) {
  return createLabel(parent, text, &stCaption);
}

// Message for an empty / loading / error state: centered title + hint
lv_obj_t* createMessage(lv_obj_t* parent, const char* symbol, const char* title, const char* hint, int centerY) {
  lv_obj_t* box = lv_obj_create(parent);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, SCREEN_WIDTH - 2 * MARGIN, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(box, 6, 0);
  passClicks(box);
  if (symbol) createLabel(box, symbol, &stTitleAccent);
  createLabel(box, title, &stTitle);
  if (hint) {
    lv_obj_t* h = createLabel(box, hint, &stSecondary);
    lv_obj_set_width(h, LV_PCT(100));  // wraps instead of running off the screen
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
  }
  lv_obj_update_layout(box);
  lv_obj_set_pos(box, MARGIN, centerY - lv_obj_get_height(box) / 2);
  return box;
}

// ============================================================
// Icon button (back, gear): 44x44 tap area, circle only while pressed
// ============================================================
lv_obj_t* createIconButton(lv_obj_t* parent, const char* symbol, int x, int y, lv_event_cb_t onClick) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_add_style(btn, &stIconButton, 0);
  lv_obj_add_style(btn, &stPressed, LV_STATE_PRESSED);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, TOUCH_MIN, TOUCH_MIN);
  lv_obj_add_event_cb(btn, onClick, LV_EVENT_CLICKED, NULL);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, symbol);
  lv_obj_center(label);
  return btn;
}

// ============================================================
// Status: WiFi icon (every screen) and the small clock in the
// header. Updated in place by connection_status.h / time_date.h.
// ============================================================
lv_obj_t* wifiIcon = NULL;
lv_obj_t* statusClock = NULL;  // header clock (not on Home, which has the big clock)

void updateWiFiIcon(bool connected) {
  if (wifiIcon == NULL) return;
  lv_label_set_text(wifiIcon, connected ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING " Offline");
  if (connected) lv_obj_remove_state(wifiIcon, LV_STATE_USER_1);
  else lv_obj_add_state(wifiIcon, LV_STATE_USER_1);  // danger color (style below)
}

// The new screen is built before the old one is deleted: only forget a
// status label if the deleted one is the current one
static void onStatusDeleted(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target_obj(e);
  if (obj == wifiIcon) wifiIcon = NULL;
  if (obj == statusClock) statusClock = NULL;
}

lv_obj_t* createWiFiIcon(lv_obj_t* parent) {
  wifiIcon = createLabel(parent, "", &stSecondary);
  lv_obj_add_style(wifiIcon, &stDanger, LV_STATE_USER_1);
  lv_obj_add_event_cb(wifiIcon, onStatusDeleted, LV_EVENT_DELETE, NULL);
  updateWiFiIcon(WiFi.status() == WL_CONNECTED);
  return wifiIcon;
}

void updateStatusClock();  // time_date.h

// ============================================================
// Header bar (every screen except Home):
// [<]  Title                                  11:30  wifi
// ============================================================
lv_obj_t* createHeader(lv_obj_t* scr, const char* title, lv_event_cb_t onBack) {
  createIconButton(scr, LV_SYMBOL_LEFT, 0, 0, onBack);
  lv_obj_t* titleLabel = NULL;
  if (title) {
    titleLabel = createLabel(scr, title, &stTitle);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 48, 11);
  }

  // right side: clock, WiFi (laid out right to left)
  lv_obj_t* status = lv_obj_create(scr);
  lv_obj_remove_style_all(status);
  lv_obj_set_size(status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(status, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(status, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(status, 8, 0);
  passClicks(status);
  statusClock = createLabel(status, "", &stSecondary);
  lv_obj_add_event_cb(statusClock, onStatusDeleted, LV_EVENT_DELETE, NULL);
  createWiFiIcon(status);
  lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -MARGIN, 14);
  updateStatusClock();
  return titleLabel;
}

// ============================================================
// Dropdown in the app style ("\n"-separated options, kept by the caller)
// ============================================================
lv_obj_t* createDropdown(lv_obj_t* parent, const char* options, int selected, lv_event_cb_t onChange) {
  lv_obj_t* dd = lv_dropdown_create(parent);
  lv_dropdown_set_options_static(dd, options);
  lv_dropdown_set_selected(dd, selected);
  lv_obj_add_style(dd, &stDropdown, 0);
  lv_obj_add_style(dd, &stPressed, LV_STATE_PRESSED);
  lv_obj_set_ext_click_area(dd, 6);
  lv_obj_add_event_cb(dd, onChange, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t* list = lv_dropdown_get_list(dd);
  lv_obj_add_style(list, &stDropdownList, 0);
  lv_obj_add_style(list, &stDropdownSelected, LV_PART_SELECTED | LV_STATE_CHECKED);
  lv_obj_add_style(list, &stPressed, LV_PART_SELECTED | LV_STATE_PRESSED);
  lv_obj_add_style(list, &stScrollbar, LV_PART_SCROLLBAR);
  return dd;
}

// ============================================================
// Weather icons: ONE object each, drawn with LVGL's draw API
// (arcs, circles, lines) instead of 3-10 shape objects.
// Also used as the brightness icon in Settings.
// ============================================================
enum WeatherIcon : uint8_t { ICON_SUN, ICON_PARTLY, ICON_CLOUD, ICON_FOG, ICON_RAIN, ICON_SNOW, ICON_STORM };

static void fillCircle(lv_layer_t* layer, int cx, int cy, int r, lv_color_t c) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c;
  d.radius = LV_RADIUS_CIRCLE;
  lv_area_t a = {cx - r, cy - r, cx + r, cy + r};
  lv_draw_rect(layer, &d, &a);
}

static void drawLine(lv_layer_t* layer, int x1, int y1, int x2, int y2, int w, lv_color_t c) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = x1; d.p1.y = y1; d.p2.x = x2; d.p2.y = y2;
  d.width = w;
  d.color = c;
  d.round_start = d.round_end = 1;
  lv_draw_line(layer, &d);
}

static void drawSun(lv_layer_t* layer, int cx, int cy, int r, int s, lv_color_t c) {
  fillCircle(layer, cx, cy, r, c);
  int w = s / 14 + 1;
  for (int i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    float ca = cosf(a), sa = sinf(a);
    drawLine(layer, cx + ca * (r + s / 10), cy + sa * (r + s / 10), cx + ca * (r + s / 5), cy + sa * (r + s / 5), w, c);
  }
}

// cloud: two circles on a rounded base, inside a box of w x h at (x, y)
static void drawCloud(lv_layer_t* layer, int x, int y, int w, int h, lv_color_t c) {
  fillCircle(layer, x + w * 3 / 10, y + h * 6 / 10, h * 3 / 10, c);
  fillCircle(layer, x + w * 6 / 10, y + h * 45 / 100, h * 45 / 100, c);
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c;
  d.radius = h * 2 / 10;
  lv_area_t a = {x, y + h * 55 / 100, x + w, y + h - 1};
  lv_draw_rect(layer, &d, &a);
}

static void onWeatherIconDraw(lv_event_t* e) {
  lv_obj_t* obj = lv_event_get_target_obj(e);
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(obj, &a);
  int s = lv_area_get_width(&a), x = a.x1, y = a.y1;
  lv_color_t sun = lv_color_hex(theme->sun), cloud = lv_color_hex(theme->cloud);
  lv_color_t accent = lv_color_hex(theme->accent);
  int lw = s / 14 + 1;

  switch ((WeatherIcon)(uintptr_t)lv_obj_get_user_data(obj)) {
    case ICON_SUN:
      drawSun(layer, x + s / 2, y + s / 2, s / 4, s, sun);
      break;
    case ICON_PARTLY:
      drawSun(layer, x + s * 36 / 100, y + s * 36 / 100, s / 6, s * 2 / 3, sun);
      drawCloud(layer, x + s * 2 / 10, y + s * 4 / 10, s * 8 / 10 - 1, s * 5 / 10, cloud);
      break;
    case ICON_CLOUD:
      drawCloud(layer, x, y + s * 2 / 10, s - 1, s * 6 / 10, cloud);
      break;
    case ICON_FOG:
      drawCloud(layer, x, y + s / 10, s - 1, s * 5 / 10, cloud);
      for (int i = 0; i < 2; i++) drawLine(layer, x + s / 8, y + s * (7 + 2 * i) / 10, x + s * 7 / 8, y + s * (7 + 2 * i) / 10, lw, cloud);
      break;
    case ICON_RAIN:
    case ICON_SNOW:
    case ICON_STORM: {
      drawCloud(layer, x, y, s - 1, s * 6 / 10, cloud);
      WeatherIcon type = (WeatherIcon)(uintptr_t)lv_obj_get_user_data(obj);
      if (type == ICON_STORM) {  // lightning bolt
        drawLine(layer, x + s * 55 / 100, y + s * 6 / 10, x + s * 42 / 100, y + s * 8 / 10, lw + 1, sun);
        drawLine(layer, x + s * 42 / 100, y + s * 8 / 10, x + s * 58 / 100, y + s * 8 / 10, lw + 1, sun);
        drawLine(layer, x + s * 58 / 100, y + s * 8 / 10, x + s * 45 / 100, y + s - 1, lw + 1, sun);
      } else {
        for (int i = 0; i < 3; i++) {
          int dx = x + s * (25 + 25 * i) / 100;
          if (type == ICON_RAIN) drawLine(layer, dx, y + s * 7 / 10, dx - s / 12, y + s * 93 / 100, lw, accent);
          else fillCircle(layer, dx, y + s * 82 / 100, lw + 1, cloud);
        }
      }
      break;
    }
  }
}

lv_obj_t* createWeatherIcon(lv_obj_t* parent, WeatherIcon type, int size) {
  lv_obj_t* icon = lv_obj_create(parent);
  lv_obj_remove_style_all(icon);
  lv_obj_set_size(icon, size, size);
  lv_obj_set_user_data(icon, (void*)(uintptr_t)type);
  lv_obj_add_event_cb(icon, onWeatherIconDraw, LV_EVENT_DRAW_MAIN, NULL);
  passClicks(icon);
  return icon;
}

// Change the picture of an existing icon (e.g. new weather) without recreating it
void setWeatherIcon(lv_obj_t* icon, WeatherIcon type) {
  if ((WeatherIcon)(uintptr_t)lv_obj_get_user_data(icon) == type) return;
  lv_obj_set_user_data(icon, (void*)(uintptr_t)type);
  lv_obj_invalidate(icon);
}

// Sets a label's text only if it changed (avoids redrawing it). Returns true if it changed.
bool setLabelIfChanged(lv_obj_t* label, const char* text) {
  if (label == NULL || strcmp(lv_label_get_text(label), text) == 0) return false;
  lv_label_set_text(label, text);
  return true;
}
#endif
