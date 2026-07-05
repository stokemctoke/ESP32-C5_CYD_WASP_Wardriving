#include "nest_espnow.h"
#include "nest_registry.h"
#include "nest_led.h"
#include "../common/wasp_mac.h"
#include <Arduino.h>

static void maybeWarnFirmware(uint8_t fwVer, const uint8_t workerMac[6]) {
  if (fwVer == 0 || fwVer == WASP_FIRMWARE_VER) return;
  Serial.printf("[NEST] Firmware mismatch %02X:%02X:%02X:%02X:%02X:%02X: worker=%u nest=%u\n",
                workerMac[0], workerMac[1], workerMac[2],
                workerMac[3], workerMac[4], workerMac[5],
                fwVer, WASP_FIRMWARE_VER);
}

typedef struct {
  uint8_t  mac[6];
  uint8_t  nodeType;
  int8_t   rssi;
  uint32_t agoSec;
} espnow_hb_log_t;

typedef struct {
  scan_summary_t pkt;
  int8_t         rssi;
} espnow_sum_log_t;

static portMUX_TYPE     espnowLogLock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool    hbLogPending  = false;
static volatile bool    sumLogPending = false;
static espnow_hb_log_t  hbLog;
static espnow_sum_log_t sumLog;

void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (!info || !data || len < 1) return;
  int rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;

  if (data[0] == WASP_PKT_HEARTBEAT && len >= (int)sizeof(heartbeat_t)) {
    const heartbeat_t* pkt = (const heartbeat_t*)data;
    maybeWarnFirmware(pkt->firmwareVer, pkt->workerMac);

    ledHeartbeatFlag = true;
    uint32_t ago = 0;

    taskENTER_CRITICAL(&gLock);
    int idx = findOrAddWorker(pkt->workerMac);
    if (idx >= 0) {
      ago = (workers[idx].lastSeenMs > 0) ? (millis() - workers[idx].lastSeenMs) / 1000 : 0;
      workers[idx].lastSeenMs = millis();
      workers[idx].rssi       = (int8_t)rssi;
      workers[idx].nodeType   = pkt->nodeType;
    }
    taskEXIT_CRITICAL(&gLock);

    taskENTER_CRITICAL(&espnowLogLock);
    memcpy(hbLog.mac, pkt->workerMac, 6);
    hbLog.nodeType = pkt->nodeType;
    hbLog.rssi     = (int8_t)rssi;
    hbLog.agoSec   = ago;
    hbLogPending   = true;
    taskEXIT_CRITICAL(&espnowLogLock);
    return;
  }

  if (data[0] != WASP_PKT_SUMMARY || len < (int)sizeof(scan_summary_t)) return;

  const scan_summary_t* pkt = (const scan_summary_t*)data;
  maybeWarnFirmware(pkt->firmwareVer, pkt->workerMac);

  taskENTER_CRITICAL(&gLock);
  int idx = findOrAddWorker(pkt->workerMac);
  if (idx >= 0) {
    workers[idx].lastSeenMs    = millis();
    workers[idx].lastSummaryMs = millis();
    workers[idx].rssi          = (int8_t)rssi;
    workers[idx].gpsFix        = pkt->gpsFix;
    workers[idx].lat           = pkt->lat;
    workers[idx].lon           = pkt->lon;
    workers[idx].altM          = pkt->altM;
    workers[idx].sats          = pkt->sats;
    workers[idx].hdop          = pkt->hdop;
    workers[idx].wifiTotal     = pkt->wifiTotal;
    workers[idx].wifi2g        = pkt->wifi2g;
    workers[idx].wifi5g        = pkt->wifi5g;
    workers[idx].bleCount      = pkt->bleCount;
    workers[idx].cycleCount    = pkt->cycleCount;
  }
  taskEXIT_CRITICAL(&gLock);

  taskENTER_CRITICAL(&espnowLogLock);
  memcpy(&sumLog.pkt, pkt, sizeof(scan_summary_t));
  sumLog.rssi    = (int8_t)rssi;
  sumLogPending  = true;
  taskEXIT_CRITICAL(&espnowLogLock);
}

void processEspNowLogs() {
  if (hbLogPending) {
    espnow_hb_log_t snap;
    bool pending = false;
    taskENTER_CRITICAL(&espnowLogLock);
    if (hbLogPending) {
      snap    = hbLog;
      hbLogPending = false;
      pending = true;
    }
    taskEXIT_CRITICAL(&espnowLogLock);

    if (pending) {
      char mac[18];
      formatMacColon(snap.mac, mac, sizeof(mac));
      const char* nodeEmoji = snap.nodeType ? "🛸" : "🐝";
      char agoBuf[12];
      if (snap.agoSec == 0) snprintf(agoBuf, sizeof(agoBuf), "first");
      else                  snprintf(agoBuf, sizeof(agoBuf), "%lus ago", (unsigned long)snap.agoSec);

      taskENTER_CRITICAL(&gLock);
      int inRange = countActiveWorkers();
      taskEXIT_CRITICAL(&gLock);

      Serial.printf("🍯 [NEST]: %s💓 Detected | 🧥 %s | 📶 %d dBm | (%s)  %d in range\n",
                    nodeEmoji, mac, snap.rssi, agoBuf, inRange);
    }
  }

  if (sumLogPending) {
    espnow_sum_log_t snap;
    bool pending = false;
    taskENTER_CRITICAL(&espnowLogLock);
    if (sumLogPending) {
      snap    = sumLog;
      sumLogPending = false;
      pending = true;
    }
    taskEXIT_CRITICAL(&espnowLogLock);

    if (pending) {
      const scan_summary_t* pkt = &snap.pkt;
      char mac[18];
      formatMacColon(pkt->workerMac, mac, sizeof(mac));

      taskENTER_CRITICAL(&gLock);
      int inRange = countActiveWorkers();
      taskEXIT_CRITICAL(&gLock);

      Serial.printf("\n🐝 [NEST] Worker %s | cycle %u | RSSI link %d dBm | (%d in range)\n",
                    mac, (uint32_t)pkt->cycleCount, snap.rssi, inRange);
      if (pkt->gpsFix) {
        Serial.printf("       GPS FIX  %.6f, %.6f | alt %.1fm | sats %d | hdop %.2f\n",
                      pkt->lat, pkt->lon, pkt->altM, pkt->sats, pkt->hdop);
      } else {
        Serial.printf("       GPS NO FIX\n");
      }
      Serial.printf("       WiFi: %d total (%d x 2.4G, %d x 5G) best %d dBm\n",
                    pkt->wifiTotal, pkt->wifi2g, pkt->wifi5g, pkt->bestRssi);
      Serial.printf("       BLE:  %d device(s)\n", pkt->bleCount);
    }
  }
}
