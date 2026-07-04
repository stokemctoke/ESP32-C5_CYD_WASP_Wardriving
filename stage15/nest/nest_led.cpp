#include "nest_led.h"
#include <Arduino.h>

LedEvent evNestBoot       = { 0xFFFFFF, 3,  50,  50 };
LedEvent evNestHeartbeat  = { 0xFF69B4, 2,  80,  80 };
LedEvent evNestChunk      = { 0x0050FF, 1,  80,   0 };
LedEvent evNestUploadAct  = { 0xFF00B4, 0,   0,   0 };
LedEvent evNestUploadOK   = { 0x64FF00, 2, 200, 200 };
LedEvent evNestUploadFail = { 0xFF0000, 3, 200, 200 };

volatile bool ledHeartbeatFlag = false;

namespace {

enum class NestLedPhase : uint8_t { Idle, On, Off };

struct {
  bool active = false;
  bool r = false;
  bool g = false;
  bool b = false;
  int flashesRemaining = 0;
  int onMs = 0;
  int offMs = 0;
  NestLedPhase phase = NestLedPhase::Idle;
  uint32_t nextMs = 0;
} ledFlash;

void nestLedStopFlash() {
  ledFlash.active = false;
  ledFlash.phase = NestLedPhase::Idle;
}

}  // namespace

void nestLedOff() {
  nestLedStopFlash();
  digitalWrite(NEST_LED_R, HIGH);
  digitalWrite(NEST_LED_G, HIGH);
  digitalWrite(NEST_LED_B, HIGH);
}

void nestLedSet(bool r, bool g, bool b) {
  nestLedStopFlash();
  digitalWrite(NEST_LED_R, r ? LOW : HIGH);
  digitalWrite(NEST_LED_G, g ? LOW : HIGH);
  digitalWrite(NEST_LED_B, b ? LOW : HIGH);
}

void nestLedFlash(bool r, bool g, bool b, int times, int onMs, int offMs) {
  if (times <= 0) {
    nestLedOff();
    return;
  }

  ledFlash.r = r;
  ledFlash.g = g;
  ledFlash.b = b;
  ledFlash.flashesRemaining = times;
  ledFlash.onMs = onMs;
  ledFlash.offMs = offMs;
  ledFlash.phase = NestLedPhase::On;
  ledFlash.nextMs = millis() + (uint32_t)onMs;
  ledFlash.active = true;
  digitalWrite(NEST_LED_R, r ? LOW : HIGH);
  digitalWrite(NEST_LED_G, g ? LOW : HIGH);
  digitalWrite(NEST_LED_B, b ? LOW : HIGH);
}

void nestLedTick() {
  if (!ledFlash.active) return;

  uint32_t now = millis();
  if ((int32_t)(now - ledFlash.nextMs) < 0) return;

  if (ledFlash.phase == NestLedPhase::On) {
    digitalWrite(NEST_LED_R, HIGH);
    digitalWrite(NEST_LED_G, HIGH);
    digitalWrite(NEST_LED_B, HIGH);
    ledFlash.flashesRemaining--;
    if (ledFlash.flashesRemaining <= 0) {
      nestLedStopFlash();
      return;
    }
    ledFlash.phase = NestLedPhase::Off;
    ledFlash.nextMs = now + (uint32_t)ledFlash.offMs;
    return;
  }

  digitalWrite(NEST_LED_R, ledFlash.r ? LOW : HIGH);
  digitalWrite(NEST_LED_G, ledFlash.g ? LOW : HIGH);
  digitalWrite(NEST_LED_B, ledFlash.b ? LOW : HIGH);
  ledFlash.phase = NestLedPhase::On;
  ledFlash.nextMs = now + (uint32_t)ledFlash.onMs;
}

void nestLedFlashEvent(const LedEvent& ev) {
  bool r = ((ev.colour >> 16) & 0xFF) > 0;
  bool g = ((ev.colour >>  8) & 0xFF) > 0;
  bool b = ( ev.colour        & 0xFF) > 0;
  if (ev.flashes == 0) { nestLedSet(r, g, b); return; }
  nestLedFlash(r, g, b, ev.flashes, ev.onMs, ev.offMs);
}
