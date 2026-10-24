#ifndef LISTS_H
#define LISTS_H
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ============================================================
// The shopping list and the to-do list.
//
// The display holds the lists: they are saved to the board's flash after
// every change and loaded again at boot, so they work without WiFi.
// Firebase /lists only holds a copy (firebase_sync.h).
//
// Every item remembers WHEN it was last added or removed (changedAt).
// Changes can arrive out of order: a voice command sent while the display
// was offline waits in Firebase and arrives after a tap made later on the
// screen. The rule is simply "the later change wins": a command older
// than the item's last change is ignored. That is why a removed item is
// kept for a while as a hidden entry (exists = false): it still remembers
// when it was removed.
//
// No screen code here (lists_screen.h): screens read the lists with
// listSize() and listItem() (visible items only), and remove a tapped item
// with removeItem(). Every function is safe to call from loop() and from
// the Firebase task at the same time (they share listsMutex).
// ============================================================

#define LIST_COUNT      2
#define LIST_MAX_ITEMS  20   // visible items per list ("is full" after that)
#define LIST_SLOTS      28   // visible + removed items remembered; the oldest removed one is forgotten first
#define LIST_ITEM_LEN   48   // bytes incl. the terminator; a Hebrew letter takes 2 bytes
#define LISTS_VERSION   2    // bump if ListEntry changes (1 = names only, converted at boot)

// The board's clock is right once NTP has synced (anything after 2023)
#define LIST_TIME_VALID 1700000000UL

enum ListId { LIST_INVALID = -1, LIST_SHOPPING = 0, LIST_TODO = 1 };

enum ListResult {
  LIST_ADDED, LIST_ALREADY_THERE, LIST_FULL, LIST_REMOVED, LIST_NOT_FOUND, LIST_BAD_ITEM,
  LIST_OUTDATED  // a command older than the item's last change: ignored, the later change wins
};

struct ListEntry {
  char name[LIST_ITEM_LEN];
  uint32_t changedAt;   // Unix time (s) of the last add/remove; 0 = unknown (older than anything)
  uint8_t exists;       // 1 = on the list, 0 = removed (kept only to remember changedAt)
  uint8_t timeFromBoot; // 1 = changedAt counts seconds since boot (clock not synced yet)
};

struct ListData {
  uint8_t used;                 // entries [0, used) are in use, oldest first
  ListEntry entries[LIST_SLOTS];
};

ListData lists[LIST_COUNT];
SemaphoreHandle_t listsMutex = nullptr;
volatile uint32_t listsVersion = 1;  // +1 on every change; firebase_sync.h uploads the lists when it differs
static bool listsTimePending = false; // some changedAt still counts from boot
static volatile bool listsSaveFailed[LIST_COUNT] = {false, false};  // retried by listsCheckTime()

static const char* LIST_KEYS[LIST_COUNT] = { "shop", "todo" };

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
// multi-byte UTF-8 character (e.g. a Hebrew letter) in half.
// dst must hold LIST_ITEM_LEN bytes.
void cleanItemName(char* dst, const char* src) {
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

// ---------- time ----------

// Time for a change made on the display now. Before NTP has synced it
// counts from boot and is corrected by listsCheckTime() once the clock is right.
static uint32_t nowForChangeLocked(uint8_t& fromBoot) {
  time_t now = time(NULL);
  if ((uint32_t)now > LIST_TIME_VALID) {
    fromBoot = 0;
    return (uint32_t)now;
  }
  fromBoot = 1;
  listsTimePending = true;
  return millis() / 1000;
}

// ---------- entries (call with the lock held) ----------

// Case-insensitive for English letters, so "Milk" matches "milk". Visible and removed entries.
static int findEntryLocked(ListId list, const char* name) {
  for (int i = 0; i < lists[list].used; i++) {
    if (strcasecmp(lists[list].entries[i].name, name) == 0) return i;
  }
  return -1;
}

static int visibleCountLocked(ListId list) {
  int n = 0;
  for (int i = 0; i < lists[list].used; i++) n += lists[list].entries[i].exists;
  return n;
}

// Index of the n-th visible entry, or -1
static int visibleToSlotLocked(ListId list, int n) {
  for (int i = 0; i < lists[list].used; i++) {
    if (lists[list].entries[i].exists && n-- == 0) return i;
  }
  return -1;
}

static void deleteSlotLocked(ListId list, int index) {
  ListData& d = lists[list];
  for (int i = index; i < d.used - 1; i++) d.entries[i] = d.entries[i + 1];
  d.used--;
  memset(&d.entries[d.used], 0, sizeof(ListEntry));
}

// Writes one list to flash. Returns false if the write failed.
static bool writeListLocked(ListId list) {
  Preferences prefs;
  if (!prefs.begin("lists", false)) return false;
  bool ok = prefs.putBytes(LIST_KEYS[list], &lists[list], sizeof(ListData)) == sizeof(ListData) &&
            prefs.putUChar("ver", LISTS_VERSION) == 1;
  prefs.end();
  return ok;
}

static void saveListLocked(ListId list) {
  listsVersion = listsVersion + 1;
  if (writeListLocked(list)) {
    listsSaveFailed[list] = false;
  } else {
    // the list stays right in memory; listsCheckTime() tries again every 10 s
    if (!listsSaveFailed[list]) Serial.println("[Lists] Could not save to flash, retrying");
    listsSaveFailed[list] = true;
  }
}

// Adds a new visible entry at the end (newest). Forgets the oldest removed
// entry if all slots are in use. Returns false only if the list is full.
static bool appendLocked(ListId list, const char* name, uint32_t at, uint8_t fromBoot) {
  ListData& d = lists[list];
  if (visibleCountLocked(list) >= LIST_MAX_ITEMS) return false;
  if (d.used >= LIST_SLOTS) {
    int oldest = -1;
    for (int i = 0; i < d.used; i++) {
      if (!d.entries[i].exists && (oldest < 0 || d.entries[i].changedAt < d.entries[oldest].changedAt)) oldest = i;
    }
    deleteSlotLocked(list, oldest);  // there is one: at most LIST_MAX_ITEMS are visible
  }
  ListEntry& e = d.entries[d.used++];
  memset(&e, 0, sizeof(e));
  strcpy(e.name, name);
  e.changedAt = at;
  e.exists = 1;
  e.timeFromBoot = fromBoot;
  return true;
}

// The one place the lists change. `at` = when the change was made.
// isCommand: a voice command (may be older than the item's last change);
// otherwise a tap on the screen, which is always the newest change.
static ListResult changeLocked(ListId list, const char* name, bool add, uint32_t at,
                               uint8_t fromBoot, bool isCommand) {
  int index = findEntryLocked(list, name);
  ListEntry* e = index >= 0 ? &lists[list].entries[index] : nullptr;

  // "The later change wins". A time counted from boot can't be compared with a
  // command's real time; it is treated as older (the command wins).
  if (isCommand && e && !e->timeFromBoot && e->changedAt > at) return LIST_OUTDATED;

  if (add) {
    if (e && e->exists) {
      // already there: remember the newer time, so an older "remove" can't take it away
      if (at > e->changedAt || e->timeFromBoot) {
        e->changedAt = at;
        e->timeFromBoot = fromBoot;
        saveListLocked(list);
      }
      return LIST_ALREADY_THERE;
    }
    if (visibleCountLocked(list) >= LIST_MAX_ITEMS) return LIST_FULL;
    if (e) deleteSlotLocked(list, index);  // re-added: show it at the end, as a new item
    appendLocked(list, name, at, fromBoot);
    saveListLocked(list);
    return LIST_ADDED;
  }

  if (e && e->exists) {
    e->exists = 0;
    e->changedAt = at;
    e->timeFromBoot = fromBoot;
    saveListLocked(list);
    return LIST_REMOVED;
  }
  if (e && (at > e->changedAt || e->timeFromBoot)) {  // remember the newer remove
    e->changedAt = at;
    e->timeFromBoot = fromBoot;
    saveListLocked(list);
  }
  return LIST_NOT_FOUND;
}

// ---------- public ----------

// Prints e.g. "[Lists] Shopping list (2): milk, eggs"
void printList(ListId list) {
  if (!validList(list)) return;
  lockLists();
  Serial.printf("[Lists] %s (%d):", list == LIST_TODO ? "To-do list" : "Shopping list", visibleCountLocked(list));
  bool first = true;
  for (int i = 0; i < lists[list].used; i++) {
    if (!lists[list].entries[i].exists) continue;
    Serial.printf("%s %s", first ? "" : ",", lists[list].entries[i].name);
    first = false;
  }
  Serial.println();
  unlockLists();
}

// Saved by firmware with LISTS_VERSION 1 (names only), converted once at boot
struct ListsDataV1 {
  uint8_t version;
  uint8_t count[LIST_COUNT];
  char items[LIST_COUNT][LIST_MAX_ITEMS][LIST_ITEM_LEN];
};

static bool loadListsV1(Preferences& prefs) {
  if (prefs.getBytesLength("data") != sizeof(ListsDataV1)) return false;
  ListsDataV1* old = (ListsDataV1*)malloc(sizeof(ListsDataV1));
  if (!old) return false;
  prefs.getBytes("data", old, sizeof(ListsDataV1));
  bool ok = old->version == 1;
  for (int l = 0; ok && l < LIST_COUNT; l++) {
    if (old->count[l] > LIST_MAX_ITEMS) { ok = false; break; }
    for (int i = 0; i < old->count[l]; i++) {
      old->items[l][i][LIST_ITEM_LEN - 1] = '\0';
      appendLocked((ListId)l, old->items[l][i], 0, 0);  // time unknown: older than any new change
    }
  }
  free(old);
  return ok;
}

// Call once in setup(), before anything else uses the lists
void initLists() {
  if (listsMutex == nullptr) listsMutex = xSemaphoreCreateMutex();
  memset(lists, 0, sizeof(lists));

  const char* how = "Starting empty";
  Preferences prefs;
  if (prefs.begin("lists", false)) {  // fails only if flash is broken
    if (prefs.getUChar("ver", 0) == LISTS_VERSION) {
      how = "Loaded";
      for (int l = 0; l < LIST_COUNT; l++) {
        if (prefs.getBytesLength(LIST_KEYS[l]) == 0) continue;  // never changed: empty
        bool ok = prefs.getBytesLength(LIST_KEYS[l]) == sizeof(ListData) &&
                  prefs.getBytes(LIST_KEYS[l], &lists[l], sizeof(ListData)) == sizeof(ListData) &&
                  lists[l].used <= LIST_SLOTS;
        if (!ok) {  // damaged: start this list empty, keep the other
          memset(&lists[l], 0, sizeof(ListData));
          Serial.printf("[Lists] Saved %s was damaged, starting it empty\n", listLabel((ListId)l));
        }
      }
    } else if (loadListsV1(prefs)) {
      how = "Converted from the old format";
      for (int l = 0; l < LIST_COUNT; l++) prefs.putBytes(LIST_KEYS[l], &lists[l], sizeof(ListData));
      prefs.putUChar("ver", LISTS_VERSION);
    } else {
      memset(lists, 0, sizeof(lists));
    }
    prefs.remove("data");  // old format, frees its flash space
    prefs.end();
  }

  for (int l = 0; l < LIST_COUNT; l++) {
    for (int i = 0; i < LIST_SLOTS; i++) {
      ListEntry& e = lists[l].entries[i];
      e.name[LIST_ITEM_LEN - 1] = '\0';
      if (e.timeFromBoot) {  // made before a restart with no clock: when is unknown now
        e.changedAt = 0;
        e.timeFromBoot = 0;
      }
    }
  }
  Serial.printf("[Lists] %s: %d shopping, %d to-do items\n", how,
                visibleCountLocked(LIST_SHOPPING), visibleCountLocked(LIST_TODO));
}

// Once the clock is right, turns "seconds since boot" into real times, and
// retries a failed flash write (every 10 s). Called on every pass of loop()
// (cheap when there is nothing to do).
void listsCheckTime() {
  static unsigned long lastRetry = 0;
  bool retryNow = millis() - lastRetry >= 10000;  // a failing flash is retried every 10 s, not on every pass
  for (int l = 0; retryNow && l < LIST_COUNT; l++) {
    if (!listsSaveFailed[l]) continue;
    lastRetry = millis();
    lockLists();
    if (writeListLocked((ListId)l)) {
      listsSaveFailed[l] = false;
      Serial.println("[Lists] Saved to flash (after a failed try)");
    }
    unlockLists();
  }
  if (!listsTimePending) return;
  time_t now = time(NULL);
  if ((uint32_t)now <= LIST_TIME_VALID) return;
  uint32_t bootSec = millis() / 1000;
  lockLists();
  for (int l = 0; l < LIST_COUNT; l++) {
    bool changed = false;
    for (int i = 0; i < lists[l].used; i++) {
      ListEntry& e = lists[l].entries[i];
      if (!e.timeFromBoot) continue;
      e.changedAt = (uint32_t)now - (bootSec - e.changedAt);
      e.timeFromBoot = 0;
      changed = true;
    }
    if (changed) saveListLocked((ListId)l);
  }
  listsTimePending = false;
  unlockLists();
  Serial.println("[Lists] Clock synced: times of offline changes fixed");
}

// A change made on the display (now)
ListResult addItem(ListId list, const char* name) {
  char clean[LIST_ITEM_LEN];
  cleanItemName(clean, name);
  if (!validList(list) || clean[0] == '\0') return LIST_BAD_ITEM;
  lockLists();
  uint8_t fromBoot;
  uint32_t at = nowForChangeLocked(fromBoot);
  ListResult result = changeLocked(list, clean, true, at, fromBoot, false);
  unlockLists();
  return result;
}

ListResult removeItem(ListId list, const char* name) {
  char clean[LIST_ITEM_LEN];
  cleanItemName(clean, name);
  if (!validList(list) || clean[0] == '\0') return LIST_BAD_ITEM;
  lockLists();
  uint8_t fromBoot;
  uint32_t at = nowForChangeLocked(fromBoot);
  ListResult result = changeLocked(list, clean, false, at, fromBoot, false);
  unlockLists();
  return result;
}

// "Clear list" on the display: removes every item in one change (one flash write).
// Each item is remembered as removed with the time of the tap, like a single remove,
// so the app and a later voice command see the same result. Returns how many were removed.
int clearList(ListId list) {
  if (!validList(list)) return 0;
  lockLists();
  uint8_t fromBoot;
  uint32_t at = nowForChangeLocked(fromBoot);
  int removed = 0;
  for (int i = 0; i < lists[list].used; i++) {
    ListEntry& e = lists[list].entries[i];
    if (!e.exists) continue;
    e.exists = 0;
    e.changedAt = at;
    e.timeFromBoot = fromBoot;
    removed++;
  }
  if (removed) saveListLocked(list);
  unlockLists();
  return removed;
}

// A voice command made at `at` (Unix time in seconds, from Firebase).
// Returns LIST_OUTDATED if the item changed on the display after the command was made.
ListResult applyListCommand(ListId list, const char* name, bool add, uint32_t at) {
  char clean[LIST_ITEM_LEN];
  cleanItemName(clean, name);
  if (!validList(list) || clean[0] == '\0') return LIST_BAD_ITEM;
  lockLists();
  ListResult result = changeLocked(list, clean, add, at, 0, true);
  unlockLists();
  return result;
}

// Is the item on the list now? (same matching as addItem/removeItem)
bool listContains(ListId list, const char* name) {
  char clean[LIST_ITEM_LEN];
  cleanItemName(clean, name);
  if (!validList(list) || clean[0] == '\0') return false;
  lockLists();
  int index = findEntryLocked(list, clean);
  bool on = index >= 0 && lists[list].entries[index].exists;
  unlockLists();
  return on;
}

// For a list screen: remove the item the user tapped (visible position)
bool removeItemAt(ListId list, int index) {
  if (!validList(list)) return false;
  char name[LIST_ITEM_LEN] = "";
  lockLists();
  int slot = visibleToSlotLocked(list, index);
  if (slot >= 0) strcpy(name, lists[list].entries[slot].name);
  unlockLists();
  return name[0] && removeItem(list, name) == LIST_REMOVED;
}

// Visible position of an item (same matching as addItem/removeItem), or -1
int listIndexOf(ListId list, const char* name) {
  char clean[LIST_ITEM_LEN];
  cleanItemName(clean, name);
  if (!validList(list) || clean[0] == '\0') return -1;
  lockLists();
  int position = -1, n = 0;
  for (int i = 0; i < lists[list].used; i++) {
    if (!lists[list].entries[i].exists) continue;
    if (strcasecmp(lists[list].entries[i].name, clean) == 0) { position = n; break; }
    n++;
  }
  unlockLists();
  return position;
}

int listSize(ListId list) {
  if (!validList(list)) return 0;
  lockLists();
  int n = visibleCountLocked(list);
  unlockLists();
  return n;
}

// Copies visible item number `index` into `out`. Returns false if there is no such item.
bool listItem(ListId list, int index, char* out, size_t outSize) {
  if (!validList(list) || outSize == 0) return false;
  bool found = false;
  lockLists();
  int slot = visibleToSlotLocked(list, index);
  if (slot >= 0) {
    strncpy(out, lists[list].entries[slot].name, outSize - 1);
    out[outSize - 1] = '\0';
    found = true;
  }
  unlockLists();
  return found;
}
#endif
