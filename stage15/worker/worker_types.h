#pragma once

#include <cstdint>
#include "../common/wasp_packets.h"

// ── Nest AP ───────────────────────────────────────────────────────────────────
#define NEST_UPLOAD_PORT      8080
#define WASP_DEFAULT_UPLOAD_TOKEN "waspswarm"

// ── SD card pins ──────────────────────────────────────────────────────────────
#define SD_CS    25
#define SD_SCK    8
#define SD_MISO   9
#define SD_MOSI  10

// ── LED ────────────────────────────────────────────────────────────────────────
#define LED_COUNT             1
#define GPS_DETECT_MS         2000
#define GPS_FEED_MS           500

// ── RAM buffer (drone mode) ───────────────────────────────────────────────────
#define CYCLE_SLOTS           25
#define MAX_WIFI_PER_SLOT     40
#define MAX_BLE_PER_SLOT      20
#define MAX_BLE_PER_CYCLE     64

// ── Nest MAC compile-time default (overridable via nestMac in worker.cfg) ─────
static const uint8_t NEST_MAC_DEFAULT[6] = {0xA4, 0xF0, 0x0F, 0x5D, 0x96, 0xD4};

// ── LED type enum ─────────────────────────────────────────────────────────────
enum LedType { LED_WS2812, LED_RGB4PIN };

// ── LED event descriptor ──────────────────────────────────────────────────────
// Holds colour + timing for one named LED state.
// flashes=0 in evGPSAcquire / evConnecting means "continuous toggle" (inline loops).
struct LedEvent {
  uint32_t colour;
  int      flashes;
  int      onMs;
  int      offMs;
};

// ── WiFi scan result ──────────────────────────────────────────────────────────
struct WiFiScanResult {
  int total, g2, g5;
  int8_t bestRssi;
};

// ── RAM buffer (drone mode) — fixed at compile time ──────────────────────────
struct wifi_entry_t {
  uint8_t bssid[6];
  char    ssid[33];
  uint8_t auth;
  uint8_t channel;
  int8_t  rssi;
};

struct ble_entry_t {
  uint8_t  addr[6];
  char     name[21];
  int8_t   rssi;
  uint16_t mfgrId;
  bool     hasMfgr;
};

struct cycle_slot_t {
  bool         used;
  bool         uploaded;
  uint32_t     capturedMs;
  bool         gpsFix;
  float        lat, lon, altM;
  float        accuracy;
  char         timestamp[20];   // WiGLE FirstSeen, captured at commit
  uint8_t      wifiCount;
  uint8_t      bleCount;
  wifi_entry_t wifi[MAX_WIFI_PER_SLOT];
  ble_entry_t  ble[MAX_BLE_PER_SLOT];
};

// ESP-NOW packet layouts: stage15/common/wasp_packets.h
