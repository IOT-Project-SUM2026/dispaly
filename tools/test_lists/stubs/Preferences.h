#pragma once
#include "Arduino.h"
extern std::map<std::string, std::vector<uint8_t>> fakeFlash;
extern bool fakeFlashBroken;  // test: every write fails
struct Preferences {
  bool begin(const char*, bool) { return true; }
  void end() {}
  size_t putBytes(const char* k, const void* v, size_t n) { if (fakeFlashBroken) return 0; fakeFlash[k].assign((uint8_t*)v, (uint8_t*)v + n); return n; }
  size_t getBytesLength(const char* k) { return fakeFlash.count(k) ? fakeFlash[k].size() : 0; }
  size_t getBytes(const char* k, void* v, size_t n) { if (!fakeFlash.count(k)) return 0; size_t m = std::min(n, fakeFlash[k].size()); memcpy(v, fakeFlash[k].data(), m); return m; }
  size_t putUChar(const char* k, uint8_t v) { if (fakeFlashBroken) return 0; fakeFlash[k] = {v}; return 1; }
  uint8_t getUChar(const char* k, uint8_t d) { return fakeFlash.count(k) ? fakeFlash[k][0] : d; }
  bool remove(const char* k) { return fakeFlash.erase(k) > 0; }
};
