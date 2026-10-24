// Host test of the real main/src/lists/lists.h (stubs replace the ESP32 parts).
// 1) State check: every sequence of voice add/remove (queued while the display
//    is offline) and taps on the display, for an item that starts on or off the list.
//    Expected result = the state after the LAST change in time; checked against
//    what the display ends up with after the queue arrives.
// 2) Specific cases: re-add order, full list, old-format conversion, tombstone
//    eviction, taps before the clock synced, restart.
#include "stubs/time_override.h"
#include "../../main/src/lists/lists.h"
#include <cassert>
#include <iostream>

unsigned long fakeMillis = 0;
time_t fakeNow = 0;
FakeSerial Serial;
std::map<std::string, std::vector<uint8_t>> fakeFlash;
bool fakeFlashBroken = false;

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { fails++; std::cout << "FAIL: " << msg << "\n"; } } while (0)

static void reset() {
  fakeFlash.clear();
  fakeMillis = 0;
  fakeNow = 1760000000;  // clock synced
  initLists();
}

enum Op { V_ADD, V_REMOVE, D_REMOVE };
static const char* opName[] = { "V+", "V-", "D-" };

int main() {
  // ---------- 1) state check ----------
  int sequences = 0, conflicts = 0, skipped = 0;
  for (int len = 1; len <= 6; len++) {
    int total = 1;
    for (int i = 0; i < len; i++) total *= 3;
    for (int code = 0; code < total; code++) {
      for (int startOn = 0; startOn <= 1; startOn++) {
        std::vector<Op> ops;
        for (int i = 0, c = code; i < len; i++, c /= 3) ops.push_back((Op)(c % 3));

        reset();
        const uint32_t T0 = 1760000000;
        if (startOn) { fakeNow = T0 - 100; addItem(LIST_SHOPPING, "milk"); }

        // expected: apply every change in time order
        bool expected = startOn;
        // display offline: taps happen now, voice commands wait in the queue
        bool valid = true;
        std::vector<std::pair<Op, uint32_t>> queue;
        for (int i = 0; i < len; i++) {
          uint32_t t = T0 + 10 * (i + 1);
          if (ops[i] == D_REMOVE) {
            if (!listContains(LIST_SHOPPING, "milk")) { valid = false; break; }  // can only tap a shown item
            fakeNow = t;
            removeItem(LIST_SHOPPING, "Milk");
            expected = false;
          } else {
            queue.push_back({ops[i], t});
            expected = ops[i] == V_ADD;
          }
        }
        if (!valid) { skipped++; continue; }
        // back online: the queue arrives, oldest first
        fakeNow = T0 + 1000;
        for (auto& q : queue) applyListCommand(LIST_SHOPPING, "milk", q.first == V_ADD, q.second);

        sequences++;
        bool actual = listContains(LIST_SHOPPING, "milk");
        if (actual != expected) {
          conflicts++;
          std::cout << "CONFLICT start=" << (startOn ? "on" : "off") << ":";
          for (Op o : ops) std::cout << " " << opName[o];
          std::cout << " expected " << expected << " got " << actual << "\n";
        }
      }
    }
  }
  std::cout << "State check: " << sequences << " possible sequences (up to 6 changes), "
            << conflicts << " conflicts (" << skipped << " impossible ones skipped: tap on a hidden item)\n";
  CHECK(conflicts == 0, "state check");

  // ---------- 2) specific cases ----------
  // the mentor's case: on the list, "add" by voice (queued), tap remove later -> off
  reset();
  addItem(LIST_SHOPPING, "milk");
  uint32_t voiceTime = fakeNow + 5;
  fakeNow += 10; removeItem(LIST_SHOPPING, "milk");
  CHECK(applyListCommand(LIST_SHOPPING, "milk", true, voiceTime) == LIST_OUTDATED, "old add ignored");
  CHECK(!listContains(LIST_SHOPPING, "milk"), "milk stays removed");
  // a newer add brings it back, at the end of the list
  addItem(LIST_SHOPPING, "eggs");
  CHECK(applyListCommand(LIST_SHOPPING, "milk", true, fakeNow + 20) == LIST_ADDED, "newer add");
  char name[LIST_ITEM_LEN];
  CHECK(listItem(LIST_SHOPPING, 1, name, sizeof(name)) && strcmp(name, "milk") == 0, "re-added item is last");
  CHECK(listSize(LIST_SHOPPING) == 2, "2 visible");

  // same command twice (e.g. the display restarted before removing it from the queue)
  reset();
  CHECK(applyListCommand(LIST_TODO, "walk", true, fakeNow) == LIST_ADDED, "add");
  CHECK(applyListCommand(LIST_TODO, "walk", true, fakeNow) == LIST_ALREADY_THERE, "same add again");
  CHECK(listSize(LIST_TODO) == 1, "no duplicate");

  // full list, then removed entries are forgotten oldest first, visible items never lost
  reset();
  for (int i = 0; i < LIST_MAX_ITEMS; i++) { char n[8]; snprintf(n, 8, "i%d", i); fakeNow++; addItem(LIST_SHOPPING, n); }
  CHECK(addItem(LIST_SHOPPING, "extra") == LIST_FULL, "full");
  for (int round = 0; round < 50; round++) {
    char n[LIST_ITEM_LEN]; listItem(LIST_SHOPPING, 0, n, sizeof(n));  // tap the first item
    fakeNow++; CHECK(removeItem(LIST_SHOPPING, n) == LIST_REMOVED, "remove");
    char m[8]; snprintf(m, 8, "n%d", round);
    fakeNow++; CHECK(addItem(LIST_SHOPPING, m) == LIST_ADDED, "add after remove");
  }
  CHECK(listSize(LIST_SHOPPING) == LIST_MAX_ITEMS, "still 20 visible");
  CHECK(lists[LIST_SHOPPING].used <= LIST_SLOTS, "slots bounded");

  // restart: everything is loaded back
  int before = listSize(LIST_SHOPPING);
  initLists();
  CHECK(listSize(LIST_SHOPPING) == before, "reload after restart");

  // old format (LISTS_VERSION 1) is converted, and its flash key removed
  fakeFlash.clear();
  ListsDataV1 v1 = {};
  v1.version = 1; v1.count[0] = 2; v1.count[1] = 1;
  strcpy(v1.items[0][0], "bread"); strcpy(v1.items[0][1], "חלב"); strcpy(v1.items[1][0], "call mom");
  fakeFlash["data"].assign((uint8_t*)&v1, (uint8_t*)&v1 + sizeof(v1));
  initLists();
  CHECK(listSize(LIST_SHOPPING) == 2 && listSize(LIST_TODO) == 1, "converted counts");
  CHECK(listContains(LIST_SHOPPING, "חלב"), "Hebrew item kept");
  CHECK(!fakeFlash.count("data"), "old key removed");
  // converted items have no time: any command wins
  CHECK(applyListCommand(LIST_SHOPPING, "bread", false, 1700000001) == LIST_REMOVED, "command beats unknown time");

  // tap before the clock synced, then synced: time fixed relative to boot
  fakeFlash.clear();
  fakeNow = 0; fakeMillis = 0;
  initLists();
  fakeNow = 1760000000; addItem(LIST_SHOPPING, "milk");  // pretend it was added earlier with a real time
  fakeNow = 0; fakeMillis = 30000;                        // e.g. after a restart, no clock yet, 30 s since boot
  removeItem(LIST_SHOPPING, "milk");                      // tap at boot+30 s
  fakeMillis = 90000; fakeNow = 1760000500;               // clock syncs at boot+90 s
  listsCheckTime();
  int idx = findEntryLocked(LIST_SHOPPING, "milk");
  CHECK(idx >= 0 && lists[LIST_SHOPPING].entries[idx].changedAt == 1760000500 - 60, "boot time converted");
  CHECK(applyListCommand(LIST_SHOPPING, "milk", true, 1760000500 - 120) == LIST_OUTDATED, "add sent before the tap ignored");
  CHECK(applyListCommand(LIST_SHOPPING, "milk", true, 1760000500 - 10) == LIST_ADDED, "add sent after the tap applied");

  // flash write fails: the list stays right in memory and is saved on a later try
  fakeFlash.clear(); fakeNow = 1760000000; initLists();
  addItem(LIST_TODO, "walk");
  fakeFlashBroken = true;
  addItem(LIST_TODO, "read");
  CHECK(listSize(LIST_TODO) == 2, "in memory despite the failed write");
  fakeMillis += 10000;
  listsCheckTime();  // still broken
  fakeFlashBroken = false;
  listsCheckTime();  // too soon: retried only every 10 s
  CHECK(listsSaveFailed[LIST_TODO], "not retried within 10 s");
  fakeMillis += 10000;
  listsCheckTime();  // retried
  CHECK(!listsSaveFailed[LIST_TODO], "retried after 10 s");
  initLists();       // restart: loaded from flash
  CHECK(listSize(LIST_TODO) == 2 && listContains(LIST_TODO, "read"), "saved after the retry");

  // "Clear list" on the display: one change, survives a restart, voice commands still ordered by time
  fakeFlash.clear(); fakeNow = 1760000000; initLists();
  addItem(LIST_SHOPPING, "milk"); addItem(LIST_SHOPPING, "eggs"); addItem(LIST_TODO, "walk");
  fakeNow += 30;
  uint32_t versionBefore = listsVersion;
  CHECK(clearList(LIST_SHOPPING) == 2, "clear removes both items");
  CHECK(listSize(LIST_SHOPPING) == 0 && listSize(LIST_TODO) == 1, "only that list cleared");
  CHECK(listsVersion == versionBefore + 1, "one change for the whole clear");
  CHECK(clearList(LIST_SHOPPING) == 0, "clearing an empty list changes nothing");
  initLists();
  CHECK(listSize(LIST_SHOPPING) == 0, "stays cleared after a restart");
  CHECK(applyListCommand(LIST_SHOPPING, "milk", true, fakeNow - 5) == LIST_OUTDATED, "voice add from before the clear ignored");
  CHECK(applyListCommand(LIST_SHOPPING, "milk", true, fakeNow + 5) == LIST_ADDED, "voice add after the clear applied");

  std::cout << (fails ? "SOME CHECKS FAILED\n" : "All specific checks passed\n");
  return fails ? 1 : 0;
}
