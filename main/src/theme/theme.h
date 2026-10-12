#ifndef THEME_H
#define THEME_H
#include "../../globals.h"
#include "../storage/storage.h"

// ============================================================
// Design system: colors (dark + light), fonts, and the shared
// LVGL styles every screen uses.
//
// Every color on screen comes from a style in this file, never
// from a per-object (local) style. Switching dark/light mode only
// rewrites these styles and tells LVGL to redraw: no screen has
// to be rebuilt, and each style is stored once instead of on
// every object (less RAM).
//
// Contrast (WCAG): text and secondary text >= 4.5:1 on every
// background they are used on, in both themes.
// ============================================================

const Theme DARK_THEME = {
  0x0B0F14,  // bg
  0x171F2A,  // surface (cards, rows)
  0x222C39,  // surfaceHi (pressed, segmented control track)
  0x2A3747,  // border / dividers / slider track
  0xE8ECF2,  // text            15.6:1 on bg
  0x8B95A7,  // secondary text   5.6:1 on surface
  0x00C2FF,  // accent           8.2:1 on surface
  0x0098C8,  // accent, pressed
  0x04121A,  // text on accent   9.2:1
  0xFF6B75,  // danger / offline 7.0:1 on bg
  0xFFC24B,  // weather icon: sun
  0xC5CDD8,  // weather icon: cloud
};

// All white, with outlined cards and rows: a tinted background looked grey on the
// board's TN screen, which makes light colors duller than on a phone
const Theme LIGHT_THEME = {
  0xFFFFFF,  // bg
  0xFFFFFF,  // surface (cards and rows are outlined instead of filled)
  0xEEF2F6,  // surfaceHi (pressed, segmented control track)
  0xCDD5DF,  // border
  0x141A22,  // text            17.5:1 on white
  0x586274,  // secondary text   6.2:1 on white, 5.4:1 on surfaceHi
  0x0072B5,  // accent           5.2:1 on white (was 0x008CC8: 3.8:1, too low)
  0x005A8F,  // accent, pressed
  0xFFFFFF,  // text on accent   5.2:1
  0xC8323A,  // danger / offline 5.3:1 on white, white text on it 5.3:1
  0xF0A202,  // weather icon: sun
  0x9AA4B3,  // weather icon: cloud
};

bool darkMode = true;
const Theme* theme = &DARK_THEME;

// Big digits (0-9 : - degree sign) in Ubuntu Medium, ~4 KB flash
// (a full 40 px font would be ~70 KB). See font_display.c.
// Equal-width digits so the clock doesn't shift when a minute changes.
LV_FONT_DECLARE(font_display);
// Hebrew letters + Montserrat for the rest (font_hebrew_*.c), for text
// people write: list items, event titles, voice command replies.
LV_FONT_DECLARE(font_hebrew_14);
LV_FONT_DECLARE(font_hebrew_20);

// ============================================================
// Shared styles
// ============================================================
lv_style_t stScreen;       // screen background
lv_style_t stCard;         // card: surface, 12 px radius, 1 px border
lv_style_t stRow;          // list row: surface, 10 px radius, outlined in light mode
lv_style_t stPressed;      // pressed state for anything tappable
lv_style_t stFlat;         // no background/border/padding (layout boxes)
lv_style_t stDivider;      // 1 px line
lv_style_t stAccentFill;   // accent background (selected tab, toggles, toast)
lv_style_t stDangerFill;   // danger background (error toast)
lv_style_t stTrackFill;    // segmented control track
lv_style_t stIconButton;   // round icon button (back, gear): transparent until pressed
lv_style_t stScrollbar;    // thin scrollbar
lv_style_t stCircle;       // list tick circle (empty ring)
lv_style_t stCircleNew;    // ring of an item a voice command just added (state LV_STATE_USER_1)
lv_style_t stCheckHidden;  // check mark inside the ring: invisible ...
lv_style_t stCheckShown;   // ... until the item is ticked (state LV_STATE_CHECKED)
lv_style_t stNowBar;       // accent bar on the left of an event that is on now

// text styles: font + color
lv_style_t stDisplay;      // big digits (clock, temperature)
lv_style_t stValue;        // 28 px
lv_style_t stTitle;        // 20 px (screen titles, list items, event titles)
lv_style_t stTitleAccent;  // 20 px accent
lv_style_t stBody;         // 14 px
lv_style_t stSecondary;    // 14 px secondary
lv_style_t stAccent;       // 14 px accent
lv_style_t stOnAccent;     // 14 px on an accent/danger fill
lv_style_t stCaption;      // 12 px secondary, letter-spaced (UPPERCASE section labels)
lv_style_t stSmall;        // 12 px secondary (hints, "Updated 11:20")
lv_style_t stDanger;       // 14 px danger (offline)
lv_style_t stStrike;       // ticked item text (state LV_STATE_CHECKED): secondary + strikethrough

// controls
lv_style_t stSwitchMain, stSwitchOn, stSwitchKnob;
lv_style_t stSliderMain, stSliderIndicator, stSliderKnob;
lv_style_t stDropdown, stDropdownList, stDropdownSelected;
lv_style_t stCityButton;
// World clock dials (lv_scale): light face by day, dark face at night (state LV_STATE_USER_1)
lv_style_t stDialFace, stDialFaceNight, stDialTicks, stDialTicksNight, stDialHour, stDialHourNight, stDialMinute, stDialCap, stDialCapNight;  // Weather city picker: pill with an accent outline

static void textStyle(lv_style_t* s, const lv_font_t* font) {
  lv_style_init(s);
  lv_style_set_text_font(s, font);
  // Hebrew: each label finds its own direction from its text (right to left
  // for Hebrew), but text stays left-aligned so lists look the same either way
  lv_style_set_base_dir(s, LV_BASE_DIR_AUTO);
  lv_style_set_text_align(s, LV_TEXT_ALIGN_LEFT);
}

// Sets every color from the active theme (called at start and on dark/light switch)
static void setStyleColors() {
  lv_color_t bg = lv_color_hex(theme->bg), surface = lv_color_hex(theme->surface);
  lv_color_t surfaceHi = lv_color_hex(theme->surfaceHi), border = lv_color_hex(theme->border);
  lv_color_t text = lv_color_hex(theme->text), secondary = lv_color_hex(theme->secondary);
  lv_color_t accent = lv_color_hex(theme->accent), accentPressed = lv_color_hex(theme->accentPressed);
  lv_color_t onAccent = lv_color_hex(theme->onAccent), danger = lv_color_hex(theme->danger);

  lv_style_set_bg_color(&stScreen, bg);
  lv_style_set_bg_color(&stCard, surface);
  lv_style_set_border_color(&stCard, border);
  lv_style_set_bg_color(&stRow, surface);
  lv_style_set_border_width(&stRow, theme == &LIGHT_THEME ? 1 : 0);  // white rows on white need an outline
  lv_style_set_border_color(&stRow, border);
  lv_style_set_bg_color(&stPressed, surfaceHi);
  lv_style_set_bg_color(&stDivider, border);
  lv_style_set_bg_color(&stAccentFill, accent);
  lv_style_set_text_color(&stAccentFill, onAccent);
  lv_style_set_bg_color(&stDangerFill, danger);
  lv_style_set_text_color(&stDangerFill, theme == &DARK_THEME ? onAccent : lv_color_white());
  lv_style_set_bg_color(&stTrackFill, surfaceHi);
  lv_style_set_text_color(&stIconButton, text);
  lv_style_set_bg_color(&stScrollbar, secondary);
  lv_style_set_border_color(&stCircle, secondary);
  lv_style_set_border_color(&stCircleNew, accent);
  lv_style_set_text_color(&stCheckShown, onAccent);
  lv_style_set_border_color(&stNowBar, accent);

  lv_style_set_text_color(&stDisplay, text);
  lv_style_set_text_color(&stValue, text);
  lv_style_set_text_color(&stTitle, text);
  lv_style_set_text_color(&stTitleAccent, accent);
  lv_style_set_text_color(&stBody, text);
  lv_style_set_text_color(&stSecondary, secondary);
  lv_style_set_text_color(&stAccent, accent);
  lv_style_set_text_color(&stOnAccent, onAccent);
  lv_style_set_text_color(&stCaption, secondary);
  lv_style_set_text_color(&stSmall, secondary);
  lv_style_set_text_color(&stDanger, danger);
  lv_style_set_text_color(&stStrike, secondary);

  lv_style_set_bg_color(&stSwitchMain, border);
  lv_style_set_bg_color(&stSwitchOn, accent);
  lv_style_set_bg_color(&stSwitchKnob, theme == &DARK_THEME ? text : lv_color_white());
  lv_style_set_bg_color(&stSliderMain, border);
  lv_style_set_bg_color(&stSliderIndicator, accent);
  lv_style_set_bg_color(&stSliderKnob, theme == &DARK_THEME ? text : lv_color_white());
  lv_style_set_border_color(&stSliderKnob, accent);
  lv_style_set_bg_color(&stDropdown, surfaceHi);
  lv_style_set_text_color(&stDropdown, text);
  lv_style_set_bg_color(&stDropdownList, surface);
  lv_style_set_border_color(&stDropdownList, border);
  lv_style_set_text_color(&stDropdownList, text);
  lv_style_set_border_color(&stCityButton, accent);
  bool light = theme == &LIGHT_THEME;
  lv_color_t dayHands = light ? text : bg, nightFace = light ? text : surfaceHi, nightHands = light ? surface : text;
  lv_style_set_bg_color(&stDialFace, lv_color_hex(theme->cloud));
  lv_style_set_bg_color(&stDialFaceNight, nightFace);
  lv_style_set_line_color(&stDialTicks, dayHands);
  lv_style_set_line_color(&stDialTicksNight, nightHands);
  lv_style_set_line_color(&stDialHour, dayHands);
  lv_style_set_line_color(&stDialHourNight, nightHands);
  lv_style_set_line_color(&stDialMinute, accent);
  lv_style_set_bg_color(&stDialCap, dayHands);
  lv_style_set_bg_color(&stDialCapNight, nightHands);
  lv_style_set_bg_color(&stDropdownSelected, accent);
  lv_style_set_text_color(&stDropdownSelected, onAccent);
  (void)accentPressed;
}

void initStyles() {
  lv_style_init(&stScreen);
  lv_style_set_bg_opa(&stScreen, LV_OPA_COVER);

  lv_style_init(&stCard);
  lv_style_set_bg_opa(&stCard, LV_OPA_COVER);
  lv_style_set_radius(&stCard, 12);
  lv_style_set_border_width(&stCard, 1);
  lv_style_set_pad_all(&stCard, 0);
  lv_style_set_shadow_width(&stCard, 0);

  lv_style_init(&stRow);
  lv_style_set_bg_opa(&stRow, LV_OPA_COVER);
  lv_style_set_radius(&stRow, 10);
  lv_style_set_border_width(&stRow, 0);
  lv_style_set_pad_all(&stRow, 0);

  lv_style_init(&stPressed);
  lv_style_set_bg_opa(&stPressed, LV_OPA_COVER);

  lv_style_init(&stFlat);
  lv_style_set_bg_opa(&stFlat, LV_OPA_TRANSP);
  lv_style_set_border_width(&stFlat, 0);
  lv_style_set_pad_all(&stFlat, 0);
  lv_style_set_radius(&stFlat, 0);

  lv_style_init(&stDivider);
  lv_style_set_bg_opa(&stDivider, LV_OPA_COVER);

  lv_style_init(&stAccentFill);
  lv_style_set_bg_opa(&stAccentFill, LV_OPA_COVER);
  lv_style_init(&stDangerFill);
  lv_style_set_bg_opa(&stDangerFill, LV_OPA_COVER);
  lv_style_init(&stTrackFill);
  lv_style_set_bg_opa(&stTrackFill, LV_OPA_COVER);

  lv_style_init(&stIconButton);
  lv_style_set_bg_opa(&stIconButton, LV_OPA_TRANSP);
  lv_style_set_radius(&stIconButton, LV_RADIUS_CIRCLE);
  lv_style_set_shadow_width(&stIconButton, 0);
  lv_style_set_border_width(&stIconButton, 0);
  lv_style_set_pad_all(&stIconButton, 0);
  lv_style_set_text_font(&stIconButton, &lv_font_montserrat_20);

  lv_style_init(&stScrollbar);
  lv_style_set_width(&stScrollbar, 3);
  lv_style_set_bg_opa(&stScrollbar, LV_OPA_50);
  lv_style_set_radius(&stScrollbar, 2);
  lv_style_set_pad_right(&stScrollbar, 1);

  lv_style_init(&stCircle);
  lv_style_set_radius(&stCircle, LV_RADIUS_CIRCLE);
  lv_style_set_bg_opa(&stCircle, LV_OPA_TRANSP);
  lv_style_set_border_width(&stCircle, 2);
  lv_style_set_pad_all(&stCircle, 0);
  lv_style_init(&stCircleNew);
  lv_style_init(&stCheckHidden);
  lv_style_set_text_opa(&stCheckHidden, LV_OPA_TRANSP);
  lv_style_set_text_font(&stCheckHidden, &lv_font_montserrat_14);
  lv_style_init(&stCheckShown);
  lv_style_set_text_opa(&stCheckShown, LV_OPA_COVER);
  lv_style_init(&stNowBar);
  lv_style_set_border_width(&stNowBar, 4);
  lv_style_set_border_side(&stNowBar, LV_BORDER_SIDE_LEFT);

  textStyle(&stDisplay, &font_display);
  textStyle(&stValue, &lv_font_montserrat_28);
  textStyle(&stTitle, &font_hebrew_20);
  textStyle(&stTitleAccent, &font_hebrew_20);
  textStyle(&stBody, &font_hebrew_14);
  textStyle(&stSecondary, &lv_font_montserrat_14);
  textStyle(&stAccent, &lv_font_montserrat_14);
  textStyle(&stOnAccent, &lv_font_montserrat_14);
  textStyle(&stCaption, &lv_font_montserrat_12);
  lv_style_set_text_letter_space(&stCaption, 1);
  textStyle(&stSmall, &lv_font_montserrat_12);
  textStyle(&stDanger, &lv_font_montserrat_14);
  lv_style_init(&stStrike);
  lv_style_set_text_decor(&stStrike, LV_TEXT_DECOR_STRIKETHROUGH);

  lv_style_init(&stSwitchMain);
  lv_style_set_bg_opa(&stSwitchMain, LV_OPA_COVER);
  lv_style_init(&stSwitchOn);
  lv_style_set_bg_opa(&stSwitchOn, LV_OPA_COVER);
  lv_style_init(&stSwitchKnob);
  lv_style_set_bg_opa(&stSwitchKnob, LV_OPA_COVER);
  lv_style_set_pad_all(&stSwitchKnob, -3);

  lv_style_init(&stSliderMain);
  lv_style_set_bg_opa(&stSliderMain, LV_OPA_COVER);
  lv_style_set_radius(&stSliderMain, LV_RADIUS_CIRCLE);
  lv_style_init(&stSliderIndicator);
  lv_style_set_bg_opa(&stSliderIndicator, LV_OPA_COVER);
  lv_style_set_radius(&stSliderIndicator, LV_RADIUS_CIRCLE);
  lv_style_init(&stSliderKnob);
  lv_style_set_bg_opa(&stSliderKnob, LV_OPA_COVER);
  lv_style_set_border_width(&stSliderKnob, 3);
  lv_style_set_pad_all(&stSliderKnob, 7);
  lv_style_set_shadow_width(&stSliderKnob, 0);

  lv_style_init(&stDropdown);
  lv_style_set_bg_opa(&stDropdown, LV_OPA_COVER);
  lv_style_set_border_width(&stDropdown, 0);
  lv_style_set_radius(&stDropdown, 10);
  lv_style_set_text_font(&stDropdown, &lv_font_montserrat_14);
  lv_style_set_pad_hor(&stDropdown, 12);
  lv_style_set_pad_ver(&stDropdown, 11);
  lv_style_init(&stDropdownList);
  lv_style_set_text_font(&stDropdownList, &lv_font_montserrat_14);
  lv_style_set_radius(&stDropdownList, 10);
  lv_style_set_border_width(&stDropdownList, 1);
  lv_style_set_text_line_space(&stDropdownList, 18);  // ~32 px per option: easier to hit
  lv_style_init(&stDialFace);
  lv_style_set_bg_opa(&stDialFace, LV_OPA_COVER);
  lv_style_set_radius(&stDialFace, LV_RADIUS_CIRCLE);
  lv_style_set_pad_all(&stDialFace, 0);         // needles are measured from the outer edge
  lv_style_set_arc_width(&stDialFace, 0);       // no ring line
  lv_style_set_border_width(&stDialFace, 0);
  lv_style_init(&stDialFaceNight);
  lv_style_init(&stDialTicks);
  lv_style_set_line_width(&stDialTicks, 2);
  lv_style_set_length(&stDialTicks, 5);
  lv_style_set_line_rounded(&stDialTicks, true);
  lv_style_init(&stDialTicksNight);
  lv_style_init(&stDialHour);
  lv_style_set_line_width(&stDialHour, 3);
  lv_style_set_line_rounded(&stDialHour, true);
  lv_style_init(&stDialHourNight);
  lv_style_init(&stDialMinute);
  lv_style_set_line_width(&stDialMinute, 2);
  lv_style_set_line_rounded(&stDialMinute, true);
  lv_style_init(&stDialCap);
  lv_style_set_bg_opa(&stDialCap, LV_OPA_COVER);
  lv_style_set_radius(&stDialCap, LV_RADIUS_CIRCLE);
  lv_style_init(&stDialCapNight);
  lv_style_init(&stCityButton);
  lv_style_set_radius(&stCityButton, 18);
  lv_style_set_border_width(&stCityButton, 1);
  lv_style_set_pad_ver(&stCityButton, 9);
  lv_style_set_pad_right(&stCityButton, 12);
  lv_style_init(&stDropdownSelected);
  lv_style_set_bg_opa(&stDropdownSelected, LV_OPA_COVER);

  setStyleColors();
}

// Load the saved theme (call after initStorage). initDisplay() builds the styles after lv_init().
void initTheme() {
  darkMode = loadInt("darkMode", 1) != 0;
  theme = darkMode ? &DARK_THEME : &LIGHT_THEME;
}

// Switches dark/light: rewrites the shared styles and redraws everything.
// Nothing is rebuilt; the open screen keeps its scroll position and state.
void setDarkMode(bool on) {
  darkMode = on;
  theme = on ? &DARK_THEME : &LIGHT_THEME;
  saveInt("darkMode", on);
  setStyleColors();
  lv_obj_report_style_change(NULL);  // every object using any of these styles refreshes
}
#endif
