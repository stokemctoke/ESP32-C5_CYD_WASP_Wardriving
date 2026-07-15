#include "worker_espnow.h"
#include "worker_gps.h"
#include "worker_led.h"
#include "worker_config.h"
#include <WiFi.h>
#include <esp_wifi.h>

uint32_t lastHeartbeatMs = 0;
bool     espNowOk        = false;
static uint8_t hbSeq     = 0;
static uint8_t sumSeq    = 0;

static uint32_t effectiveHeartbeatMs() {
  static uint32_t interval = 0;
  if (interval == 0) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    interval = heartbeatIntervalMs + (uint32_t)(mac[5] % 20) * 100U;
  }
  return interval;
}

void onSendResult(const wifi_tx_info_t*, esp_now_send_status_t status) {
  Serial.printf("[WORKER] ESP-NOW TX %s\n",
                status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAILED");
}

void initEspNow() {
  espNowOk = false;
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("[WORKER] ESP-NOW init FAILED");
    ledSyncFail();
    return;
  }
  esp_now_register_send_cb(onSendResult);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, nestMac, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  espNowOk = true;
}

void deinitEspNow() {
  if (!espNowOk) return;
  esp_now_deinit();
  espNowOk = false;
}

void sendHeartbeat() {
  if (!espNowOk) return;
  extern bool droneMode;
  heartbeat_t pkt = {};
  pkt.type        = WASP_PKT_HEARTBEAT;
  pkt.nodeType    = droneMode ? 1 : 0;
  pkt.firmwareVer = WASP_FIRMWARE_VER;
  pkt.seq         = ++hbSeq;
  WiFi.macAddress(pkt.workerMac);
  waspSeal(&pkt, sizeof(pkt));
  esp_now_send(nestMac, (uint8_t*)&pkt, sizeof(pkt));
  lastHeartbeatMs = millis();
  ledHeartbeat();
}

void maybeHeartbeat() {
  if (millis() - lastHeartbeatMs >= effectiveHeartbeatMs()) sendHeartbeat();
}

void sendSummary(int wifiTotal, int wifi2g, int wifi5g,
                 int bleCount, int8_t bestRssi) {
  if (!espNowOk) return;
  extern uint32_t cycleCount;
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  scan_summary_t pkt = {};
  pkt.type        = WASP_PKT_SUMMARY;
  pkt.firmwareVer = WASP_FIRMWARE_VER;
  WiFi.macAddress(pkt.workerMac);
  pkt.gpsFix     = (gpsOk && gps.location.isValid()) ? 1 : 0;
  pkt.lat        = pkt.gpsFix ? (float)gps.location.lat() : 0.0f;
  pkt.lon        = pkt.gpsFix ? (float)gps.location.lng() : 0.0f;
  pkt.altM       = (pkt.gpsFix && gps.altitude.isValid()) ? (float)gps.altitude.meters() : 0.0f;
  pkt.sats       = (gpsOk && gps.satellites.isValid()) ? (uint8_t)gps.satellites.value() : 0;
  pkt.hdop       = (gpsOk && gps.hdop.isValid())       ? gps.hdop.hdop() : 99.99f;
  pkt.wifiTotal  = (uint16_t)wifiTotal;
  pkt.wifi2g     = (uint8_t)wifi2g;
  pkt.wifi5g     = (uint8_t)wifi5g;
  pkt.bleCount   = (uint16_t)bleCount;
  pkt.bestRssi   = bestRssi;
  pkt.cycleCount = cycleCount + 1;
  pkt.seq        = ++sumSeq;
  waspSeal(&pkt, sizeof(pkt));
  esp_err_t err = esp_now_send(nestMac, (uint8_t*)&pkt, sizeof(pkt));
  if (err != ESP_OK) {
    Serial.printf("[WARN] esp_now_send summary failed: %s\n", esp_err_to_name(err));
  }
}
