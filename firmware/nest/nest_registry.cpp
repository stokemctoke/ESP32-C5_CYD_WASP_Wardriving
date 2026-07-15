#include "nest_registry.h"
#include "../common/wasp_mac.h"
#include <Arduino.h>

worker_entry_t workers[MAX_WORKERS] = {};
portMUX_TYPE   gLock = portMUX_INITIALIZER_UNLOCKED;

int findWorker(const uint8_t* mac) {
  for (int i = 0; i < MAX_WORKERS; i++)
    if (memcmp(workers[i].mac, mac, 6) == 0) return i;
  return -1;
}

int findOrAddWorker(const uint8_t* mac, registry_evt_t* evt) {
  if (evt) evt->kind = REG_EVT_NONE;
  int idx = findWorker(mac);
  if (idx >= 0) return idx;
  for (int i = 0; i < MAX_WORKERS; i++)
    if (workers[i].lastSeenMs == 0) { memcpy(workers[i].mac, mac, 6); return i; }

  // All slots occupied — evict the oldest stale entry (timed out but not yet removed).
  // Logging is deferred to the caller via *evt: this runs inside the gLock
  // critical section, where Serial.printf is unsafe.
  uint32_t now = millis();
  int evictIdx = -1;
  uint32_t oldestSeen = UINT32_MAX;
  for (int i = 0; i < MAX_WORKERS; i++) {
    if (workers[i].lastSeenMs == 0) continue;
    uint32_t age = now - workers[i].lastSeenMs;
    if (age >= WORKER_TIMEOUT_MS && workers[i].lastSeenMs < oldestSeen) {
      oldestSeen = workers[i].lastSeenMs;
      evictIdx = i;
    }
  }
  if (evictIdx < 0) {
    if (evt) { evt->kind = REG_EVT_DROPPED; memcpy(evt->newcomer, mac, 6); }
    return -1;
  }

  if (evt) {
    evt->kind = REG_EVT_EVICTED;
    memcpy(evt->evicted, workers[evictIdx].mac, 6);
    memcpy(evt->newcomer, mac, 6);
  }
  memset(&workers[evictIdx], 0, sizeof(workers[evictIdx]));
  memcpy(workers[evictIdx].mac, mac, 6);
  return evictIdx;
}

void logRegistryEvt(const registry_evt_t& evt) {
  if (evt.kind == REG_EVT_NONE) return;
  char newcomer[18];
  formatMacColon(evt.newcomer, newcomer, sizeof(newcomer));
  if (evt.kind == REG_EVT_DROPPED) {
    Serial.printf("[NEST] WARNING: registry full — no stale slots, dropping worker %s\n",
                  newcomer);
  } else {
    char evicted[18];
    formatMacColon(evt.evicted, evicted, sizeof(evicted));
    Serial.printf("[NEST] WARNING: registry full — evicted stale worker %s for %s\n",
                  evicted, newcomer);
  }
}

int countActiveWorkers() {
  int n = 0;
  uint32_t now = millis();
  for (int i = 0; i < MAX_WORKERS; i++)
    if (workers[i].lastSeenMs > 0 && (now - workers[i].lastSeenMs) < WORKER_TIMEOUT_MS) n++;
  return n;
}

void cleanRegistry() {
  uint32_t now = millis();
  for (int i = 0; i < MAX_WORKERS; i++) {
    taskENTER_CRITICAL(&gLock);
    bool hasSeen = workers[i].lastSeenMs > 0;
    uint32_t age = hasSeen ? (now - workers[i].lastSeenMs) : 0;
    bool expired = hasSeen && age >= WORKER_REMOVE_MS;
    bool offline = hasSeen && age >= WORKER_TIMEOUT_MS && age < WORKER_REMOVE_MS && !workers[i].offlineNotified;
    uint8_t macB[6];
    char mac[18];
    if (expired) { memcpy(macB, workers[i].mac, 6); memset(&workers[i], 0, sizeof(workers[i])); }
    if (offline) {
      memcpy(macB, workers[i].mac, 6);
      workers[i].offlineNotified = true;
      formatMacColon(macB, mac, sizeof(mac));
    }
    taskEXIT_CRITICAL(&gLock);
    if (offline) Serial.printf("[NEST] Worker %s offline — no heartbeat for %u ms\n", mac, age);
    if (expired) {
      formatMacColon(macB, mac, sizeof(mac));
      Serial.printf("[NEST] Worker %s expired — slot freed\n", mac);
    }
  }
}
