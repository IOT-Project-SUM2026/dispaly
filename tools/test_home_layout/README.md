# Home layout test (runs on a computer, not on the board)

Compiles the real `main/src/home/home_layout.h` and checks every combination of
greeting (on / off), shown widgets (16) and Big widget (none, Clock, Weather, Lists, Next event):
160 layouts. With the greeting off, no widget may cover the WiFi icon and gear (x 164..320, y 0..44).
For each: every shown widget is placed, inside the area (x 12..308, y 48..232), at least
48 px tall, no two widgets overlap, the Big widget is full width, and the widgets fill the
area down to y 232. With everything shown and nothing big, the result must be the original
layout (rows of 68 and 108 px).

```
g++ -std=c++17 test_home_layout.cpp -o test_home_layout
./test_home_layout
```

Result on 20.10.2026: `160 layouts checked, all OK (default = original 68 + 108 px rows)`
