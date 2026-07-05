#pragma once

#include <stdint.h>

// Shared ESP-NOW constants and packet layouts (Nest + Worker must match).
#define ESPNOW_CHANNEL        1
#define WASP_PKT_SUMMARY      0x01
#define WASP_PKT_HEARTBEAT    0x02
#define WASP_FIRMWARE_VER     16

typedef struct __attribute__((packed)) {
  uint8_t type;
  uint8_t workerMac[6];
  uint8_t nodeType;      // 0 = worker (SD), 1 = drone (RAM buffer)
  uint8_t firmwareVer;
} heartbeat_t;           // 9 bytes

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
} scan_summary_t;          // 37 bytes
