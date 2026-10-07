#ifndef LISTS_H
#define LISTS_H
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ============================================================
// The shopping list and the to-do list.
//
// Firebase only holds the latest voice command, so the display is the
// only place the lists exist. They are saved to the board's flash
// after every change and loaded again at boot.
//
// No screen code here: a list screen reads the lists with listSize()
// and listItem(), and can remove a tapped item with removeItemAt().
// Every function is safe to call from loop() and from the Firebase
// task at the same time (they share listsMutex).
// ============================================================

#define LIST_COUNT      2
#define LIST_MAX_ITEMS  20
#define LIST_ITEM_LEN   48   // bytes incl. the terminator; a Hebrew letter takes 2 bytes
#define LISTS_VERSION   1    // bump if ListsData changes, so old saved data is ignored

enum ListId { LIST_INVALID = -1, LIST_SHOPPING = 0, LIST_TODO = 1 };

enum ListResult { LIST_ADDED, LIST_ALREADY_THERE, LIST_FULL, LIST_REMOVED, LIST_NOT_FOUND, LIST_BAD_ITEM };

struct ListsData {
  uint8_t version;
  uint8_t count[LIST_COUNT];
  char items[LIST_COUNT][LIST_MAX_ITEMS][LIST_ITEM_LEN];
};

ListsData listsData;
SemaphoreHandle_t listsMutex = nullptr;

// "shopping" / "todo" as written by the app -> ListId
ListId listFromName(const char* name) {
  if (strcmp(name, "shopping") == 0) return LIST_SHOPPING;
  if (strcmp(name, "todo") == 0) return LIST_TODO;
  return LIST_INVALID;
}

const char* listLabel(ListId list) {
  return list == LIST_TODO ? "to-do list" : "shopping list";
}

static bool validList(ListId list) {
  return list >= 0 && list < LIST_COUNT;
}

static void lockLists()   { xSemaphoreTake(listsMutex, portMAX_DELAY); }
static void unlockLists() { xSemaphoreGive(listsMutex); }

// Trims spaces and shortens the name to fit, without cutting a
// multi-byte UTF-8 character (e.g. a Hebrew letter) in half
static void cleanItemName(char* dst, const char* src) {
  while (*src == ' ' || *src == '\t') src++;
  size_t len = strlen(src);
  while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\t')) len--;
  if (len > LIST_ITEM_LEN - 1) {
    len = LIST_ITEM_LEN - 1;
    while (len > 0 && ((uint8_t)src[len] & 0xC0) == 0x80) len--;
  }
  memcpy(dst, src, len);
  dst[len] = '\0';
}

// Case-insensitive for English letters, so "Milk" matches "milk"
static int findItemLocked(ListId list, const char* name) {
  for (int i = 0; i < listsData.count[list]; i++) {
    if (strcasecmp(listsData.items[list][i], name) == 0) return i;
  }
  return -1;
}

static void saveListsLocked() {
  Preferences prefs;
  if (prefs.begin("lists", false)) {
    prefs.putBytes("data", &listsData, sizeof(ListsData));
    prefs.end();
  } else {
    Serial.println("[Lists] Could not save to flash");
  }
}

static void removeAtLocked(ListId list, int index) {
  for (int i = index; i < listsData.count[list] - 1; i++) {
    strcpy(listsData.items[list][i], listsData.items[list][i + 1]);
  }
  listsData.count[list]--;
  listsData.items[list][listsData.count[list]][0] = '\0';
  saveListsLocked();
}

// Prints e.g. "[Lists] Shopping list (2): milk, eggs" - the way to see
// the lists until there is a list screen
void printList(ListId list) {
  if (!validList(list)) return;
  lockLists();
  Serial.printf("[Lists] %s (%d):", list == LIST_TODO ? "To-do list" : "Shopping list", listsData.count[list]);
  for (int i = 0; i < listsData.count[list]; i++) {
    Serial.printf("%s %s", i == 0 ? "" : ",", listsData.items[list][i]);
  }
  Serial.println();
  unlockLists();
}

// Call once in setup(), before anything else uses the lists
void initLists() {
  if (listsMutex == nullptr) listsMutex = xSemaphoreCreateMutex();

  bool loaded = false;
  Preferences prefs;
  if (prefs.begin("lists", true)) {  // read-only; fails on the very first boot (nothing saved yet)
    if (prefs.getBytesLength("data") == sizeof(ListsData)) {
      prefs.getBytes("data", &listsData, sizeof(ListsData));
      loaded = listsData.version == LISTS_VERSION &&
               listsData.count[LIST_SHOPPING] <= LIST_MAX_ITEMS &&
               listsData.count[LIST_TODO] <= LIST_MAX_ITEMS;
    }
    prefs.end();
  }
  if (!loaded) {
    memset(&listsData, 0, sizeof(ListsData));
    listsData.version = LISTS_VERSION;
  }
  for (int l = 0; l < LIST_COUNT; l++) {
    for (int i = 0; i < LIST_MAX_ITEMS; i++) listsData.items[l][i][LIST_ITEM_LEN - 1] = '\0';
  }
  Serial.printf("[Lists] %s: %d shopping, %d to-do items\n", loaded ? "Loaded" : "Starting empty",
                listsData.count[LIST_SHOPPING], listsData.count[LIST_TODO]);
}

ListResult addItem(ListId list, const char* name) {
  char clean[LIST_ITEM_LEN];
  cleanItemName(clean, name);
  if (!validList(list) || clean[0] == '\0') return LIST_BAD_ITEM;

  ListResult result;
  lockLists();
  if (findItemLocked(list, clean) >= 0) {
    result = LIST_ALREADY_THERE;
  } else if (listsData.count[list] >= LIST_MAX_ITEMS) {
    result = LIST_FULL;
  } else {
    strcpy(listsData.items[list][listsData.count[list]], clean);
    listsData.count[list]++;
    saveListsLocked();
    result = LIST_ADDED;
  }
  unlockLists();
  return result;
}

ListResult removeItem(ListId list, const char* name) {
  char clean[LIST_ITEM_LEN];
  cleanItemName(clean, name);
  if (!validList(list) || clean[0] == '\0') return LIST_BAD_ITEM;

  ListResult result = LIST_NOT_FOUND;
  lockLists();
  int index = findItemLocked(list, clean);
  if (index >= 0) {
    removeAtLocked(list, index);
    result = LIST_REMOVED;
  }
  unlockLists();
  return result;
}

// For a list screen: remove the item the user tapped
bool removeItemAt(ListId list, int index) {
  if (!validList(list)) return false;
  bool removed = false;
  lockLists();
  if (index >= 0 && index < listsData.count[list]) {
    removeAtLocked(list, index);
    removed = true;
  }
  unlockLists();
  return removed;
}

int listSize(ListId list) {
  if (!validList(list)) return 0;
  lockLists();
  int n = listsData.count[list];
  unlockLists();
  return n;
}

// Copies item number `index` into `out`. Returns false if there is no such item.
bool listItem(ListId list, int index, char* out, size_t outSize) {
  if (!validList(list) || outSize == 0) return false;
  bool found = false;
  lockLists();
  if (index >= 0 && index < listsData.count[list]) {
    strncpy(out, listsData.items[list][index], outSize - 1);
    out[outSize - 1] = '\0';
    found = true;
  }
  unlockLists();
  return found;
}
#endif
