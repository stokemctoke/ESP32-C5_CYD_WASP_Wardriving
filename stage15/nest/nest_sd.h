#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t sdMutex;

inline void sdTake() {
  xSemaphoreTake(sdMutex, portMAX_DELAY);
}

inline void sdGive() {
  xSemaphoreGive(sdMutex);
}
