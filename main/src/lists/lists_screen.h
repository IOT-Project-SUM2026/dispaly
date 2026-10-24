#ifndef LISTS_SCREEN_H
#define LISTS_SCREEN_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "lists.h"
#include "../net/net.h"
#include "../home/home_layout.h"

// ============================================================
// Lists screen: Shopping / To-do switch in the header, items in a
// scrollable list (44 px rows).
//
// Tap an item to tick it off: the ring fills with a check mark and
// the text is crossed out; the item disappears after 3 seconds (a pop-up says so).
// Tap it again before that to keep it. Ticking only changes that
// row's state (no rebuild); removing deletes just that row.
// Voice commands open this screen (display_ui.h).
// Also the Lists card on the home screen.
// ============================================================

#define TICK_OFF_DELAY_MS 3000  // as long as the pop-up that says how to keep it

void showToast(const char* text, bool success);  // display_ui.h

ListId shownList = LIST_SHOPPING;

// The item ticked off by a tap, removed after TICK_OFF_DELAY_MS unless tapped again
bool tickedActive = false;
ListId tickedList = LIST_SHOPPING;
char tickedName[LIST_ITEM_LEN] = "";
unsigned long tickedAt = 0;
lv_obj_t* tickedRow = NULL;  // its row on the open Lists screen (NULL if not shown)

// "Clear list" at the end of the list: the first tap asks, a second tap within 4 s clears
#define CLEAR_CONFIRM_MS 4000
unsigned long clearArmedAt = 0;
ListId clearArmedList = LIST_SHOPPING;
lv_obj_t* clearLabel = NULL;

// The item a voice command just added: shown in the accent color
char newItemName[LIST_ITEM_LEN] = "";

lv_obj_t* listsContainer = NULL;  // the scrollable item list on the open Lists screen
lv_obj_t* listTabLabels[LIST_COUNT] = {NULL, NULL};
int listsScrollY = 0;             // kept when the screen is rebuilt

const char* listTabName(ListId list) {
  return list == LIST_TODO ? "To-do" : "Shopping";
}

// Adds/removes a state on a row and everything in it (LVGL states
// don't pass to children), so the row's styles for that state apply
static void setRowState(lv_obj_t* obj, lv_state_t state, bool on) {
  if (on) lv_obj_add_state(obj, state);
  else lv_obj_remove_state(obj, state);
  for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) setRowState(lv_obj_get_child(obj, i), state, on);
}

void updateListTabs() {
  for (int l = 0; l < LIST_COUNT; l++) {
    if (listTabLabels[l] == NULL) continue;
    char text[24];
    snprintf(text, sizeof(text), "%s  %d", listTabName((ListId)l), listSize((ListId)l));
    setLabelIfChanged(listTabLabels[l], text);
  }
}

// ============================================================
// Ticking items off
// ============================================================

// Removes the ticked item now. Returns true if a list changed.
bool finishTickedItem() {
  if (!tickedActive) return false;
  tickedActive = false;
  lv_obj_t* row = tickedRow;
  tickedRow = NULL;
  if (removeItem(tickedList, tickedName) != LIST_REMOVED) return false;  // e.g. already removed by voice
  Serial.printf("[Lists] Removed \"%s\" (tapped)\n", tickedName);
  printList(tickedList);

  // update the open Lists screen in place
  if (row) {
    lv_obj_delete(row);
    updateListTabs();
    if (listSize(shownList) == 0) requestRefresh();  // show the empty-list message
  }
  return true;
}

// In loop(). Removes a ticked item after 3 s and resets an unconfirmed "Clear list". Returns true when a list changed.
bool handleLists() {
  if (clearArmedAt && millis() - clearArmedAt > CLEAR_CONFIRM_MS) {  // nobody confirmed: back to "Clear list"
    clearArmedAt = 0;
    if (clearLabel) lv_label_set_text(clearLabel, "Clear list");
  }
  if (!tickedActive || millis() - tickedAt < TICK_OFF_DELAY_MS) return false;
  return finishTickedItem();
}

// Called when leaving the Lists screen
void leaveListsScreen() {
  finishTickedItem();
  clearArmedAt = 0;
  newItemName[0] = '\0';
  listsScrollY = 0;
}

// Opens a list, highlighting `item` if given (used by voice commands)
void selectList(ListId list, const char* item) {
  // "add milk" by voice while milk is ticked off: the voice command is newer,
  // so milk stays (the row is rebuilt without the tick)
  if (item && tickedActive && tickedList == list && strcasecmp(tickedName, item) == 0) {
    tickedActive = false;
    tickedRow = NULL;
    Serial.printf("[Lists] Kept \"%s\" (added again by voice)\n", tickedName);
  }
  if (list != shownList || !item) finishTickedItem();
  shownList = list;
  listsScrollY = 0;
  if (item && listIndexOf(list, item) >= 0) cleanItemName(newItemName, item);
  else newItemName[0] = '\0';
}

// ============================================================
// Touch (LVGL callbacks)
// ============================================================
void rememberListsScroll() {
  if (listsContainer) listsScrollY = lv_obj_get_scroll_y(listsContainer);
}

void onListTab(lv_event_t* e) {
  ListId tapped = (ListId)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_current_target(e));
  if (tapped == shownList) return;
  finishTickedItem();
  clearArmedAt = 0;
  shownList = tapped;
  newItemName[0] = '\0';
  listsScrollY = 0;
  Serial.printf("[UI] Lists: %s\n", listTabName(shownList));
  requestRefresh();  // different rows: rebuild
}

void onListItem(lv_event_t* e) {
  lv_obj_t* row = (lv_obj_t*)lv_event_get_current_target(e);
  if (row == tickedRow) {  // tapped again: keep it
    tickedActive = false;
    tickedRow = NULL;
    setRowState(row, LV_STATE_CHECKED, false);
    char message[LIST_ITEM_LEN + 8];
    snprintf(message, sizeof(message), "Kept %s", tickedName);
    showToast(message, true);
    Serial.printf("[Lists] Kept \"%s\"\n", tickedName);
    return;
  }
  finishTickedItem();  // another item was waiting: remove it now (deletes its row)

  int index = lv_obj_get_index(row);  // rows are in list order
  char name[LIST_ITEM_LEN];
  if (!listItem(shownList, index, name, sizeof(name))) return;
  tickedActive = true;
  tickedList = shownList;
  strcpy(tickedName, name);
  tickedAt = millis();
  tickedRow = row;
  setRowState(row, LV_STATE_CHECKED, true);
  char message[LIST_ITEM_LEN + 32];
  snprintf(message, sizeof(message), "%s done - tap it again to keep it", name);
  showToast(message, true);
  Serial.printf("[Lists] Ticked off \"%s\" (removed in 3 s unless tapped again)\n", name);
}

// "Clear list": tap once to ask, tap again within 4 s to remove every item
void onClearList(lv_event_t* e) {
  bool confirmed = clearArmedAt && clearArmedList == shownList && millis() - clearArmedAt <= CLEAR_CONFIRM_MS;
  if (!confirmed) {
    clearArmedAt = millis();
    clearArmedList = shownList;
    if (clearLabel) {
      char text[40];
      snprintf(text, sizeof(text), "Tap again to clear all %d", listSize(shownList));
      lv_label_set_text(clearLabel, text);
    }
    return;
  }
  clearArmedAt = 0;
  tickedActive = false;  // a ticked item is cleared with the rest
  tickedRow = NULL;
  int removed = clearList(shownList);
  Serial.printf("[Lists] Cleared %s list (%d items)\n", listTabName(shownList), removed);
  printList(shownList);
  char message[48];
  snprintf(message, sizeof(message), "%s list cleared", listTabName(shownList));
  showToast(message, true);
  listsScrollY = 0;
  requestRefresh();  // shows the empty-list message
}

// The new screen is built before the old one is deleted: only forget the
// container if the deleted one is the current one
void onListsContainerDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) != listsContainer) return;
  listsContainer = NULL;
  clearLabel = NULL;
  tickedRow = NULL;
  listTabLabels[0] = listTabLabels[1] = NULL;
}

// ============================================================
// Screen
// ============================================================

// Segmented control in the header: [ Shopping 6 | To-do 2 ]
void createListTabs(lv_obj_t* scr) {
  const int x = 48, w = SCREEN_WIDTH - x - MARGIN, h = 36;
  lv_obj_t* track = createCard(scr, x, (HEADER_H - h) / 2, w, h, NULL, &stTrackFill);
  lv_obj_set_style_radius(track, h / 2, 0);
  lv_obj_set_style_pad_all(track, 3, 0);
  lv_obj_add_flag(track, LV_OBJ_FLAG_CLICKABLE);  // children handle the taps

  for (int l = 0; l < LIST_COUNT; l++) {
    lv_obj_t* seg = lv_obj_create(track);
    lv_obj_remove_style_all(seg);
    lv_obj_set_size(seg, (w - 6) / 2, h - 6);
    lv_obj_set_pos(seg, l * (w - 6) / 2, 0);
    lv_obj_set_style_radius(seg, (h - 6) / 2, 0);
    lv_obj_add_style(seg, &stAccentFill, LV_STATE_CHECKED);
    lv_obj_add_style(seg, &stPressed, LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(seg, 4);  // 44 px tall tap area
    lv_obj_set_user_data(seg, (void*)(intptr_t)l);
    lv_obj_add_event_cb(seg, onListTab, LV_EVENT_CLICKED, NULL);

    listTabLabels[l] = createLabel(seg, "", &stBody);
    lv_obj_add_style(listTabLabels[l], &stOnAccent, LV_STATE_CHECKED);
    lv_obj_center(listTabLabels[l]);
    if (l == shownList) setRowState(seg, LV_STATE_CHECKED, true);
  }
  updateListTabs();
}

lv_obj_t* createListRow(lv_obj_t* list, const char* name, bool isNew) {
  lv_obj_t* row = createCard(list, 0, 0, LV_PCT(100), ROW_H, NULL, &stRow);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_style(row, &stPressed, LV_STATE_PRESSED);
  lv_obj_add_event_cb(row, onListItem, LV_EVENT_CLICKED, NULL);

  lv_obj_t* ring = createBox(row, 12, (ROW_H - 22) / 2, 22, 22);
  lv_obj_add_style(ring, &stCircle, 0);
  lv_obj_add_style(ring, &stCircleNew, LV_STATE_USER_1);
  lv_obj_add_style(ring, &stAccentFill, LV_STATE_CHECKED);
  lv_obj_add_style(ring, &stCircleNew, LV_STATE_CHECKED);
  lv_obj_t* check = createLabel(ring, LV_SYMBOL_OK, &stCheckHidden);
  lv_obj_add_style(check, &stCheckShown, LV_STATE_CHECKED);
  lv_obj_center(check);

  char shown[LIST_ITEM_LEN];
  toScreenText(name, shown, sizeof(shown));
  lv_obj_t* label = createLabel(row, shown, &stTitle);
  lv_obj_add_style(label, &stTitleAccent, LV_STATE_USER_1);
  lv_obj_add_style(label, &stStrike, LV_STATE_CHECKED);
  makeSingleLine(label, SCREEN_WIDTH - 2 * MARGIN - 46 - 12);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 46, 0);

  if (isNew) setRowState(row, LV_STATE_USER_1, true);
  return row;
}

lv_obj_t* buildListsScreen(lv_event_cb_t onBack) {
  lv_obj_t* scr = createScreen();
  createIconButton(scr, LV_SYMBOL_LEFT, 0, 0, onBack);
  createListTabs(scr);
  tickedRow = NULL;

  int count = listSize(shownList);
  if (count == 0) {
    listsContainer = NULL;
    clearLabel = NULL;  // no Clear row on an empty list
    // if voice commands can't work, say so instead of suggesting one
    bool voiceOff = voiceState == VOICE_NOT_SET_UP || voiceState == VOICE_LOGIN_FAILED || voiceState == VOICE_DB_ERROR;
    const char* hint = voiceOff ? "Voice commands are off:\nsee Settings > Device"
                     : shownList == LIST_TODO ? "Say \"Add call mom to my to-do list\""
                                              : "Say \"Add milk to my shopping list\"";
    createMessage(scr, LV_SYMBOL_LIST, shownList == LIST_TODO ? "Nothing to do" : "Shopping list is empty", hint,
                  CONTENT_Y + (SCREEN_HEIGHT - CONTENT_Y) / 2);
    return scr;
  }

  lv_obj_t* list = createScrollList(scr, MARGIN, CONTENT_Y, SCREEN_WIDTH - 2 * MARGIN, SCREEN_HEIGHT - CONTENT_Y, 4);
  lv_obj_add_event_cb(list, onListsContainerDeleted, LV_EVENT_DELETE, NULL);
  listsContainer = list;

  lv_obj_t* newRow = NULL;
  for (int i = 0; i < count; i++) {
    char name[LIST_ITEM_LEN];
    if (!listItem(shownList, i, name, sizeof(name))) break;
    bool isNew = newItemName[0] && strcasecmp(newItemName, name) == 0;
    lv_obj_t* row = createListRow(list, name, isNew);
    if (isNew) newRow = row;
    if (tickedActive && tickedList == shownList && strcasecmp(tickedName, name) == 0) {
      tickedRow = row;
      setRowState(row, LV_STATE_CHECKED, true);
    }
  }

  // "Clear list" as the last row (after the items, so row numbers still match the list)
  lv_obj_t* clearRow = createCard(list, 0, 0, LV_PCT(100), ROW_H, NULL, &stRow);
  lv_obj_add_flag(clearRow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(clearRow, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_style(clearRow, &stPressed, LV_STATE_PRESSED);
  lv_obj_add_event_cb(clearRow, onClearList, LV_EVENT_CLICKED, NULL);
  bool armed = clearArmedAt && clearArmedList == shownList;
  char clearText[40] = "Clear list";
  if (armed) snprintf(clearText, sizeof(clearText), "Tap again to clear all %d", count);
  clearLabel = createLabel(clearRow, clearText, &stDanger);
  lv_obj_center(clearLabel);

  // keep the scroll position; show a new item from a voice command
  lv_obj_update_layout(list);
  if (newRow) lv_obj_scroll_to_view(newRow, LV_ANIM_OFF);
  else if (listsScrollY > 0) lv_obj_scroll_to_y(list, listsScrollY, LV_ANIM_OFF);
  return scr;
}

// ============================================================
// Home lists widget (tap: Lists screen). Any size (home_layout.h):
//   compact (under 64 px): the two counts (side by side when wide)
//   normal: "LISTS" with both counts (the original card)
//   wide: Shopping and To-do side by side, with their first items
// ============================================================
static void createListCount(lv_obj_t* card, const char* name, int count, int y) {
  lv_obj_t* label = createLabel(card, name, &stBody);
  lv_obj_set_pos(label, 12, y + 8);
  char text[8];
  snprintf(text, sizeof(text), "%d", count);
  lv_obj_t* value = createLabel(card, text, &stValue);
  lv_obj_align(value, LV_ALIGN_TOP_RIGHT, -12, y - 1);
}

// "Shopping" ... "6" on one line at y (for small widgets)
static void createListCountLine(lv_obj_t* card, const char* name, int count, int x, int y, int w) {
  lv_obj_t* label = createLabel(card, name, &stBody);
  lv_obj_set_pos(label, x, y + 2);
  char text[8];
  snprintf(text, sizeof(text), "%d", count);
  lv_obj_t* value = createLabel(card, text, &stTitle);
  lv_obj_update_layout(value);
  lv_obj_set_pos(value, x + w - lv_obj_get_width(value), y);
}

// One list as a column: "SHOPPING   6", then its first items (as many as fit)
static void createListColumn(lv_obj_t* card, ListId list, int x, int w, int h) {
  int count = listSize(list);
  char caption[24];
  snprintf(caption, sizeof(caption), "%s", list == LIST_TODO ? "TO-DO" : "SHOPPING");
  lv_obj_t* title = createCaption(card, caption);
  lv_obj_set_pos(title, x, 10);
  char text[8];
  snprintf(text, sizeof(text), "%d", count);
  lv_obj_t* value = createLabel(card, text, &stTitle);
  lv_obj_update_layout(value);
  lv_obj_set_pos(value, x + w - lv_obj_get_width(value), 4);

  int rows = (h - 34 - 4) / 20;
  if (count == 0) {
    lv_obj_t* empty = createLabel(card, list == LIST_TODO ? "Nothing to do" : "Empty", &stSecondary);
    lv_obj_set_pos(empty, x, 34);
    return;
  }
  if (rows <= 1) {  // one line: "Milk, Eggs, Whole wheat bre..."
    char line[128] = "", name[LIST_ITEM_LEN];
    for (int i = 0; i < count && strlen(line) < sizeof(line) - LIST_ITEM_LEN - 3; i++) {
      if (!listItem(list, i, name, sizeof(name))) break;
      if (i > 0) strcat(line, ", ");
      strcat(line, name);
    }
    char shown[160];
    toScreenText(line, shown, sizeof(shown));
    lv_obj_t* items = createLabel(card, shown, &stBody);
    makeSingleLine(items, w);
    lv_obj_set_pos(items, x, 34);
    return;
  }
  for (int i = 0; i < rows && i < count; i++) {
    char name[LIST_ITEM_LEN], shown[LIST_ITEM_LEN + 8];
    bool last = i == rows - 1 && count > rows;
    if (last) {
      snprintf(shown, sizeof(shown), "+%d more", count - i);
    } else {
      if (!listItem(list, i, name, sizeof(name))) break;
      toScreenText(name, shown, sizeof(shown));
    }
    lv_obj_t* item = createLabel(card, shown, last ? &stSmall : &stBody);
    makeSingleLine(item, w);
    lv_obj_set_pos(item, x, 34 + i * 20 + (last ? 2 : 0));
  }
}

lv_obj_t* createHomeListsCard(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t onClick) {
  lv_obj_t* card = createCard(parent, x, y, w, h, onClick);
  bool wide = w >= HOME_AREA_W;
  int shopping = listSize(LIST_SHOPPING), todo = listSize(LIST_TODO);

  if (wide && h >= 64) {  // two columns with their items
    int colW = (w - 24 - 17) / 2;
    createListColumn(card, LIST_SHOPPING, 12, colW, h);
    lv_obj_t* line = createDivider(card, 12 + colW + 8, 10, 1);  // vertical line between the columns
    lv_obj_set_size(line, 1, h - 20);
    createListColumn(card, LIST_TODO, 12 + colW + 17, colW, h);
    return card;
  }
  if (h < 64) {
    if (wide) {  // "Shopping 6 | To-do 2" on one line
      int half = (w - 24 - 17) / 2;
      createListCountLine(card, "Shopping", shopping, 12, (h - 24) / 2, half);
      createListCountLine(card, "To-do", todo, 12 + half + 17, (h - 24) / 2, half);
    } else {  // two short lines
      createListCountLine(card, "Shopping", shopping, 12, h / 2 - 22, w - 24);
      createListCountLine(card, "To-do", todo, 12, h / 2 + 0, w - 24);
    }
    return card;
  }
  // the original card, centered when the widget is taller
  int top = h >= 100 ? (h - 108) / 2 : 0;
  if (h < 100) {  // no room for the caption
    createListCount(card, "Shopping", shopping, h / 2 - 34);
    createDivider(card, 12, h / 2, w - 24);
    createListCount(card, "To-do", todo, h / 2 + 4);
    return card;
  }
  lv_obj_t* title = createCaption(card, LV_SYMBOL_LIST "  LISTS");
  lv_obj_set_pos(title, 12, top + 10);
  createListCount(card, "Shopping", shopping, top + 32);
  createDivider(card, 12, top + 69, w - 24);
  createListCount(card, "To-do", todo, top + 72);
  return card;
}
#endif
