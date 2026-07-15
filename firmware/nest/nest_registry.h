#pragma once

#include "nest_types.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern worker_entry_t workers[MAX_WORKERS];
extern portMUX_TYPE   gLock;

// Registry mutation event, reported by findOrAddWorker so the caller can log it
// AFTER leaving the gLock critical section — Serial.printf must never run with
// interrupts disabled (it can block on the UART lock).
enum RegistryEvtKind { REG_EVT_NONE = 0, REG_EVT_EVICTED, REG_EVT_DROPPED };
struct registry_evt_t {
  RegistryEvtKind kind;
  uint8_t         evicted[6];
  uint8_t         newcomer[6];
};

int  findWorker(const uint8_t* mac);
int  findOrAddWorker(const uint8_t* mac, registry_evt_t* evt = nullptr);
int  countActiveWorkers();
void cleanRegistry();
void logRegistryEvt(const registry_evt_t& evt);
