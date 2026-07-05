#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Shared ESP-NOW constants and packet layouts (Nest + Worker must match).
#define ESPNOW_CHANNEL        1
#define WASP_PKT_SUMMARY      0x01
#define WASP_PKT_HEARTBEAT    0x02
#define WASP_FIRMWARE_VER     17

static inline uint8_t waspCrc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

static inline void waspSeal(void* pkt, size_t size) {
  uint8_t* b = (uint8_t*)pkt;
  b[size - 1] = waspCrc8(b, size - 1);
}

static inline bool waspVerify(const void* pkt, size_t size) {
  const uint8_t* b = (const uint8_t*)pkt;
  return b[size - 1] == waspCrc8(b, size - 1);
}

typedef struct __attribute__((packed)) {
  uint8_t type;
  uint8_t workerMac[6];
  uint8_t nodeType;      // 0 = worker (SD), 1 = drone (RAM buffer)
  uint8_t firmwareVer;
  uint8_t seq;
  uint8_t crc;
} heartbeat_t;           // 11 bytes

typedef struct __attribute__((packed)) {
  uint8_t  type;
  uint8_t  workerMac[6];
  uint8_t  gpsFix;
  float    lat, lon, altM;
  uint8_t  sats;
  float    hdop;
  uint16_t wifiTotal;
  uint8_t  wifi2g, wifi5g;
  uint16_t bleCount;
  int8_t   bestRssi;
  uint32_t cycleCount;
  uint8_t  firmwareVer;
  uint8_t  seq;
  uint8_t  crc;
} scan_summary_t;          // 39 bytes
