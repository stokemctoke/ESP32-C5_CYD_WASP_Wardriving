#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static inline void formatMacColon(const uint8_t mac[6], char* out, size_t outLen) {
  snprintf(out, outLen, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static inline void formatMac12(const uint8_t mac[6], char* out, size_t outLen) {
  snprintf(out, outLen, "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static inline void mac12ToColon(const char* mac12, char* out, size_t outLen) {
  snprintf(out, outLen, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
           mac12[0], mac12[1], mac12[2], mac12[3], mac12[4], mac12[5],
           mac12[6], mac12[7], mac12[8], mac12[9], mac12[10], mac12[11]);
}
