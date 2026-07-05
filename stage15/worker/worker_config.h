#pragma once

#include "worker_types.h"
#include <WString.h>

// ── Nest connection ───────────────────────────────────────────────────────────
extern char nestSsid[33];
extern char nestPsk[65];
extern char nestIp[16];
extern char uploadToken[64];
extern uint8_t nestMac[6];

// ── Sync behaviour ───────────────────────────────────────────────────────────
extern int syncEvery;
extern int heartbeatIntervalMs;

// ── Scan timing ──────────────────────────────────────────────────────────────
extern int wifiChanMs;
extern int bleScanMs;
extern int cycleDelayMs;

// ── Log file ─────────────────────────────────────────────────────────────────
extern int maxLogBytes;
extern int lowHeapThreshold;

// ── Debug ────────────────────────────────────────────────────────────────────
extern bool verboseSerial;

// ── GPS ──────────────────────────────────────────────────────────────────────
extern int gpsBaud;
extern int gpsRxPin;
extern int gpsTxPin;

// ── LED pins (WS2812 data / rgb4pin R, G, B) ───────────────────────────────
extern int ledPin;
extern int ledPinG;
extern int ledPinB;

// ── Functions ────────────────────────────────────────────────────────────────
void loadWorkerConfig();
bool parseLedEvent(const String& val, LedEvent& ev);
bool parseNestMac(const String& val, uint8_t mac[6]);
