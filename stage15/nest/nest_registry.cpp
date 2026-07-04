#include "nest_registry.h"
#include <Arduino.h>

worker_entry_t workers[MAX_WORKERS] = {};
portMUX_TYPE   gLock = portMUX_INITIALIZER_UNLOCKED;

int findWorker(const uint8_t* mac) {
  for (int i = 0; i < MAX_WORKERS; i++)
    if (memcmp(workers[i].mac, mac, 6) == 0) return i;
  return -1;
}

int findOrAddWorker(const uint8_t* mac) {
  int idx = findWorker(mac);
  if (idx >= 0) return idx;
  for (int i = 0; i < MAX_WORKERS; i++)
    if (workers[i].lastSeenMs == 0) { memcpy(workers[i].mac, mac, 6); return i; }

  // All slots occupied — evict the oldest stale entry (timed out but not yet removed).
  uint32_t now = millis();
  int evictIdx = -1;
  uint32_t oldestSeen = UINT32_MAX;
  char evicted[18], newcomer[18];
  for (int i = 0; i < MAX_WORKERS; i++) {
    if (workers[i].lastSeenMs == 0) continue;
    uint32_t age = now - workers[i].lastSeenMs;
    if (age >= WORKER_TIMEOUT_MS && workers[i].lastSeenMs < oldestSeen) {
      oldestSeen = workers[i].lastSeenMs;
      evictIdx = i;
    }
  }
  if (evictIdx < 0) {
    snprintf(newcomer, sizeof(newcomer), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Serial.printf("[NEST] WARNING: registry full — no stale slots, dropping worker %s\n",
                  newcomer);
    return -1;
  }

  const uint8_t* ev = workers[evictIdx].mac;
  snprintf(evicted, sizeof(evicted), "%02X:%02X:%02X:%02X:%02X:%02X",
           ev[0], ev[1], ev[2], ev[3], ev[4], ev[5]);
  snprintf(newcomer, sizeof(newcomer), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("[NEST] WARNING: registry full — evicted stale worker %s for %s\n",
                evicted, newcomer);

  memset(&workers[evictIdx], 0, sizeof(workers[evictIdx]));
  memcpy(workers[evictIdx].mac, mac, 6);
  return evictIdx;
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
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
               macB[0], macB[1], macB[2], macB[3], macB[4], macB[5]);
    }
    taskEXIT_CRITICAL(&gLock);
    if (offline) Serial.printf("[NEST] Worker %s offline — no heartbeat for %u ms\n", mac, age);
    if (expired) {
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
               macB[0], macB[1], macB[2], macB[3], macB[4], macB[5]);
      Serial.printf("[NEST] Worker %s expired — slot freed\n", mac);
    }
  }
}
