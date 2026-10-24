#pragma once
typedef int* SemaphoreHandle_t;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { static int m; return &m; }
inline int xSemaphoreTake(SemaphoreHandle_t, int) { return 1; }
inline int xSemaphoreGive(SemaphoreHandle_t) { return 1; }
