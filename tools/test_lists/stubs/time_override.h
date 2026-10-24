#pragma once
#include <ctime>
extern time_t fakeNow;
#define time(x) (fakeNow)
