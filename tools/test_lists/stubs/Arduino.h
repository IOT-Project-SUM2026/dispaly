#pragma once
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <strings.h>
#include <map>
#include <string>
#include <vector>
extern unsigned long fakeMillis;
inline unsigned long millis() { return fakeMillis; }
struct FakeSerial { template<class... A> void printf(const char* f, A... a) {} void println(const char* = "") {} };
extern FakeSerial Serial;
