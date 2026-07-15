#include "worker_drone.h"
#include "worker_gps.h"
#include "worker_storage.h"

cycle_slot_t* cycleBuffer = nullptr;
uint8_t writeHead = 0;

wifi_entry_t* pendingWifi = nullptr;
uint8_t pendingWifiCount = 0;
ble_entry_t* pendingBle = nullptr;
uint8_t pendingBleCount = 0;

void clearPending() {
  pendingWifiCount = 0;
  pendingBleCount = 0;
}

void commitCycle() {
  cycle_slot_t& slot = cycleBuffer[writeHead];
  if (slot.used && !slot.uploaded) {
    Serial.printf("[WARN] Overwriting unuploaded slot %u — nest unreachable? (%d pending)\n",
                  writeHead, countUnuploaded());
  }
  slot.used = true;
  slot.uploaded = false;
  slot.capturedMs = millis();

  bool hasFix = gpsOk && gps.location.isValid();
  slot.gpsFix   = hasFix;
  slot.lat      = hasFix ? (float)gps.location.lat() : 0.0f;
  slot.lon      = hasFix ? (float)gps.location.lng() : 0.0f;
  slot.altM     = (hasFix && gps.altitude.isValid()) ? (float)gps.altitude.meters() : 0.0f;
  slot.accuracy = (hasFix && gps.hdop.isValid())     ? (float)(gps.hdop.hdop() * 5.0) : 999.9f;
  String ts = gpsTimestamp();
  if (ts.isEmpty()) ts = nowTimestamp();
  ts.toCharArray(slot.timestamp, sizeof(slot.timestamp));

  slot.wifiCount = pendingWifiCount;
  slot.bleCount = pendingBleCount;
  memcpy(slot.wifi, pendingWifi, pendingWifiCount * sizeof(wifi_entry_t));
  memcpy(slot.ble, pendingBle, pendingBleCount * sizeof(ble_entry_t));

  uint8_t slotIdx = writeHead;
  writeHead = (writeHead + 1) % CYCLE_SLOTS;

  int pending = countUnuploaded();
  Serial.printf("[BUF] Slot %d committed — WiFi:%d BLE:%d  |  %d/%d slots pending\n",
                slotIdx, pendingWifiCount, pendingBleCount, pending, CYCLE_SLOTS);
  clearPending();
}

int countUnuploaded() {
  if (!cycleBuffer) return 0;
  int n = 0;
  for (int i = 0; i < CYCLE_SLOTS; i++)
    if (cycleBuffer[i].used && !cycleBuffer[i].uploaded) n++;
  return n;
}

String buildCSV(int idx) {
  const cycle_slot_t& s = cycleBuffer[idx];
  String csv;
  csv.reserve(2048);
  csv += "WigleWifi-1.6,appRelease=1,model=WASP-WarDriver_v1,release=1,device=WASP-Drone,display=,board=XIAO-ESP32C5,brand=Seeed,star=Sol,body=3,subBody=0\n";
  csv += "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type,RCOIs,MfgrId\n";

  for (int i = 0; i < s.wifiCount; i++) {
    const wifi_entry_t& w = s.wifi[i];
    char bssid[18];
    snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
             w.bssid[0], w.bssid[1], w.bssid[2], w.bssid[3], w.bssid[4], w.bssid[5]);
    String ssid = String(w.ssid);
    ssid.replace("\"", "\"\"");
    char line[200];
    snprintf(line, sizeof(line), "%s,\"%s\",%s,%s,%d,%d,%d,%.6f,%.6f,%.0f,%.1f,WIFI,,",
             bssid, ssid.c_str(), wigleAuth((wifi_auth_mode_t)w.auth),
             s.timestamp, w.channel, channelToFreq(w.channel), w.rssi,
             s.lat, s.lon, s.altM, s.accuracy);
    csv += line; csv += '\n';
  }

  for (int i = 0; i < s.bleCount; i++) {
    const ble_entry_t& b = s.ble[i];
    char addr[18];
    snprintf(addr, sizeof(addr), "%02X:%02X:%02X:%02X:%02X:%02X",
             b.addr[0], b.addr[1], b.addr[2], b.addr[3], b.addr[4], b.addr[5]);
    String name = String(b.name);
    name.replace("\"", "\"\"");
    char mfgrField[8] = "";
    if (b.hasMfgr) snprintf(mfgrField, sizeof(mfgrField), "%u", b.mfgrId);
    char line[200];
    snprintf(line, sizeof(line), "%s,\"%s\",[BLE],%s,0,,%d,%.6f,%.6f,%.0f,%.1f,BLE,,%s",
             addr, name.c_str(), s.timestamp, b.rssi,
             s.lat, s.lon, s.altM, s.accuracy, mfgrField);
    csv += line; csv += '\n';
  }
  return csv;
}
