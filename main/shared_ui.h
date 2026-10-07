#ifndef SHARED_UI_H
#define SHARED_UI_H
#include "globals.h"
#include <WiFi.h>

// ============================================================
// Building blocks shared by all screens (LVGL widgets)
// ============================================================

// Empty screen with the app background
lv_obj_t* createScreen() {
  lv_obj_t* scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, UI_BG, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

lv_obj_t* createLabel(lv_obj_t* parent, const char* text, const lv_font_t* font, lv_color_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  return label;
}

// Plain filled shape (circle/rect) for icons; passes clicks to its parent
lv_obj_t* createShape(lv_obj_t* parent, int x, int y, int w, int h, int radius, lv_color_t color) {
  lv_obj_t* shape = lv_obj_create(parent);
  lv_obj_remove_style_all(shape);
  lv_obj_set_pos(shape, x, y);
  lv_obj_set_size(shape, w, h);
  lv_obj_set_style_radius(shape, radius, 0);
  lv_obj_set_style_bg_color(shape, color, 0);
  lv_obj_set_style_bg_opa(shape, LV_OPA_COVER, 0);
  lv_obj_remove_flag(shape, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(shape, LV_OBJ_FLAG_EVENT_BUBBLE);
  return shape;
}

// Rounded card; clickable (with press feedback) when a callback is given
lv_obj_t* createCard(lv_obj_t* parent, int x, int y, int w, int h, int radius, lv_event_cb_t onClick) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, w, h);
  lv_obj_set_style_radius(card, radius, 0);
  lv_obj_set_style_bg_color(card, UI_SURFACE, 0);
  lv_obj_set_style_border_color(card, UI_TRACK, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  if (onClick) {
    lv_obj_set_style_bg_color(card, UI_TRACK, LV_STATE_PRESSED);
    lv_obj_add_event_cb(card, onClick, LV_EVENT_CLICKED, NULL);
  } else {
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);
  }
  return card;
}

// Sun icon (ring + core), used by weather and the brightness setting
lv_obj_t* createSunIcon(lv_obj_t* parent, int size, lv_color_t color) {
  lv_obj_t* box = createShape(parent, 0, 0, size, size, 0, color);
  lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
  lv_obj_t* ring = createShape(box, 0, 0, size, size, LV_RADIUS_CIRCLE, color);
  lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(ring, color, 0);
  lv_obj_set_style_border_width(ring, size / 10 + 1, 0);
  createShape(box, size / 4, size / 4, size / 2, size / 2, LV_RADIUS_CIRCLE, color);
  return box;
}

// ============================================================
// WiFi icon - created on every screen, updated by connection_status.h
// ============================================================
lv_obj_t* wifiIcon = NULL;

void updateWiFiIcon(bool connected) {
  if (wifiIcon == NULL) return;
  lv_obj_set_style_text_color(wifiIcon, connected ? UI_TEXT : UI_OFFLINE, 0);
  lv_label_set_text(wifiIcon, connected ? LV_SYMBOL_WIFI : LV_SYMBOL_WIFI " " LV_SYMBOL_CLOSE);
}

void createWiFiIcon(lv_obj_t* parent) {
  wifiIcon = createLabel(parent, "", &lv_font_montserrat_14, UI_TEXT);
  lv_obj_align(wifiIcon, LV_ALIGN_TOP_LEFT, WIFI_ICON_X - 10, WIFI_ICON_Y - 8);
  updateWiFiIcon(WiFi.status() == WL_CONNECTED);
}

// ============================================================
// Round icon button (back / gear), with press feedback
// ============================================================
lv_obj_t* createIconButton(lv_obj_t* parent, const char* symbol, int cx, int cy, int r, lv_event_cb_t onClick) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_pos(btn, cx - r, cy - r);
  lv_obj_set_size(btn, 2 * r, 2 * r);
  lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btn, UI_SURFACE, 0);
  lv_obj_set_style_bg_color(btn, UI_ACCENT_DARK, LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_add_event_cb(btn, onClick, LV_EVENT_CLICKED, NULL);

  lv_obj_t* label = createLabel(btn, symbol, &lv_font_montserrat_14, UI_TEXT);
  lv_obj_center(label);
  return btn;
}

lv_obj_t* createBackButton(lv_obj_t* parent, lv_event_cb_t onClick) {
  return createIconButton(parent, LV_SYMBOL_LEFT, BACK_CX, BACK_CY, BACK_R, onClick);
}

lv_obj_t* createGearButton(lv_obj_t* parent, lv_event_cb_t onClick) {
  lv_obj_t* btn = createIconButton(parent, LV_SYMBOL_SETTINGS, GEAR_X, GEAR_Y, 16, onClick);
  lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);                 // plain icon like before,
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);   // circle only while pressed
  return btn;
}
#endif
