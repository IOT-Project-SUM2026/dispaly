#ifndef LISTS_SCREEN_H
#define LISTS_SCREEN_H
#include "../../globals.h"
#include "../../shared_ui.h"
#include "lists.h"

// ============================================================
// Lists screen: Shopping / To-do switch in the header, items in a
// scrollable list (44 px rows).
//
// Tap an item to tick it off: the ring fills with a check mark and
// the text is crossed out; the item disappears after 2 seconds.
// Tap it again before that to keep it. Ticking only changes that
// row's state (no rebuild); removing deletes just that row.
// Voice commands open this screen (display_ui.h).
// Also the Lists card on the home screen.
// ============================================================

#define TICK_OFF_DELAY_MS 2000

ListId shownList = LIST_SHOPPING;

// The item ticked off by a tap, removed after TICK_OFF_DELAY_MS unless tapped again
bool tickedActive = false;
ListId tickedList = LIST_SHOPPING;
char tickedName[LIST_ITEM_LEN] = "";
unsigned long tickedAt = 0;
lv_obj_t* tickedRow = NULL;  // its row on the open Lists screen (NULL if not shown)

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

// In loop(). Removes a ticked item after 2 s. Returns true when a list changed.
bool handleLists() {
  if (!tickedActive || millis() - tickedAt < TICK_OFF_DELAY_MS) return false;
  return finishTickedItem();
}

// Called when leaving the Lists screen
void leaveListsScreen() {
  finishTickedItem();
  newItemName[0] = '\0';
  listsScrollY = 0;
}

// Opens a list, highlighting `item` if given (used by voice commands)
void selectList(ListId list, const char* item) {
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
  Serial.printf("[Lists] Ticked off \"%s\" (removed in 2 s unless tapped again)\n", name);
}

// The new screen is built before the old one is deleted: only forget the
// container if the deleted one is the current one
void onListsContainerDeleted(lv_event_t* e) {
  if (lv_event_get_target(e) != listsContainer) return;
  listsContainer = NULL;
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
    createMessage(scr, LV_SYMBOL_LIST,
                  shownList == LIST_TODO ? "Nothing to do" : "Shopping list is empty",
                  shownList == LIST_TODO ? "Say \"Add call mom to my to-do list\"" : "Say \"Add milk to my shopping list\"",
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

  // keep the scroll position; show a new item from a voice command
  lv_obj_update_layout(list);
  if (newRow) lv_obj_scroll_to_view(newRow, LV_ANIM_OFF);
  else if (listsScrollY > 0) lv_obj_scroll_to_y(list, listsScrollY, LV_ANIM_OFF);
  return scr;
}

// ============================================================
// Home screen card: items on each list
// ============================================================
static void createListCount(lv_obj_t* card, const char* name, int count, int y) {
  lv_obj_t* label = createLabel(card, name, &stBody);
  lv_obj_set_pos(label, 12, y + 8);
  char text[8];
  snprintf(text, sizeof(text), "%d", count);
  lv_obj_t* value = createLabel(card, text, count ? &stValue : &stValue);
  lv_obj_align(value, LV_ALIGN_TOP_RIGHT, -12, y - 1);
}

lv_obj_t* createHomeListsCard(lv_obj_t* parent, int x, int y, int w, int h, lv_event_cb_t onClick) {
  lv_obj_t* card = createCard(parent, x, y, w, h, onClick);
  lv_obj_t* title = createCaption(card, LV_SYMBOL_LIST "  LISTS");
  lv_obj_set_pos(title, 12, 10);
  createListCount(card, "Shopping", listSize(LIST_SHOPPING), 32);
  createDivider(card, 12, 69, w - 24);
  createListCount(card, "To-do", listSize(LIST_TODO), 72);
  return card;
}
#endif
