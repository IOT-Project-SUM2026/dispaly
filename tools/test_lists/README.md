# List sync test (runs on a computer, not on the board)

Compiles the real `main/src/lists/lists.h` with small stand-ins for the ESP32 parts
(`stubs/`: flash, clock, mutex) and checks:

1. **State check:** every sequence of up to 6 changes to one item: voice "add" / "remove"
   sent while the display is offline (they wait in the queue) and taps on the screen,
   for an item that starts on or off the list. The expected result is the state after the
   last change in time. Taps are only allowed on an item that is shown.
2. **Specific cases:** an old voice "add" after a later tap, a re-added item shown last,
   the same command twice, a full list, restart, conversion of the old saved format,
   a tap before the clock synced, "Clear list" (one change, survives a restart, an older
   voice "add" is ignored).

Run (Linux / macOS, or Windows with MinGW / WSL):

```
g++ -std=c++17 -Istubs test_lists.cpp -o test_lists
./test_lists
```

Result on 19.10.2026:

```
State check: 573 possible sequences (up to 6 changes), 0 conflicts (1611 impossible ones skipped: tap on a hidden item)
All specific checks passed
```
