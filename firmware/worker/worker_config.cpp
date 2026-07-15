#include "worker_config.h"
#include "worker_led.h"
#include <SD.h>
#include <Preferences.h>

bool saveNvsConfigKey(const char* key, const String& val);

// ── Nest connection ───────────────────────────────────────────────────────────
char nestSsid[33]    = "WASP-Nest";
char nestPsk[65]     = "waspswarm";
char nestIp[16]      = "192.168.4.1";
char uploadToken[64] = WASP_DEFAULT_UPLOAD_TOKEN;
uint8_t nestMac[6];

// ── Sync behaviour ───────────────────────────────────────────────────────────
int syncEvery          = 25;    // sync to Nest every N scan cycles
int heartbeatIntervalMs = 5000; // ms between ESP-NOW heartbeats

// ── Scan timing ──────────────────────────────────────────────────────────────
int wifiChanMs  =  80;   // ms spent on each WiFi channel
int bleScanMs   = 3000;  // ms for each BLE scan pass
int cycleDelayMs = 2000; // ms delay at end of each cycle

// ── Log file ─────────────────────────────────────────────────────────────────
int maxLogBytes      = 8192;  // rotate log file above this size
int lowHeapThreshold = 30000; // heap warning level in bytes
bool verboseSerial   = false;

// ── GPS ──────────────────────────────────────────────────────────────────────
int gpsBaud   = 9600;
int gpsRxPin  =   12;  // XIAO C5 = D7
int gpsTxPin  =   11;  // XIAO C5 = D6

// ── LED pins (WS2812 data / rgb4pin R, G, B) ───────────────────────────────
int ledPin  =  3;  // D0
int ledPinG = 23;  // D4
int ledPinB = 24;  // D5

bool parseLedEvent(const String& val, LedEvent& ev) {
  int c1 = val.indexOf(',');
  int c2 = val.indexOf(',', c1 + 1);
  int c3 = val.indexOf(',', c2 + 1);
  if (c1 < 0 || c2 < 0 || c3 < 0) return false;
  ev.colour  = (uint32_t)strtoul(val.substring(0, c1).c_str(), nullptr, 16);
  ev.flashes = constrain(val.substring(c1 + 1, c2).toInt(), 0, 100);
  ev.onMs    = constrain(val.substring(c2 + 1, c3).toInt(), 0, 10000);
  ev.offMs   = constrain(val.substring(c3 + 1).toInt(), 0, 10000);
  return true;
}

bool parseNestMac(const String& val, uint8_t mac[6]) {
  String s = val;
  s.trim();
  s.toUpperCase();
  if (s.length() != 17) return false;
  for (int i = 0; i < 6; i++) {
    int off = i * 3;
    if (i < 5 && s.charAt(off + 2) != ':') return false;
    char hex[3] = { (char)s.charAt(off), (char)s.charAt(off + 1), '\0' };
    char* end = nullptr;
    unsigned long byte = strtoul(hex, &end, 16);
    if (end == hex || byte > 0xFF) return false;
    mac[i] = (uint8_t)byte;
  }
  return true;
}

static void applyConfigKey(const String& key, const String& val) {
  if      (key == "ledEnabled")    ledEnabled    = (val == "true" || val == "1");
  else if (key == "ledBrightness") ledBrightness = (uint8_t)constrain(val.toInt(), 0, 255);
  else if (key == "ledType")       ledType       = (val == "rgb4pin") ? LED_RGB4PIN : LED_WS2812;
  else if (key == "ledBoot")       parseLedEvent(val, evBoot);
  else if (key == "ledGPSAcquire") parseLedEvent(val, evGPSAcquire);
  else if (key == "ledGPSFound")   parseLedEvent(val, evGPSFound);
  else if (key == "ledGPSFix")     parseLedEvent(val, evGPSFix);
  else if (key == "ledScanCycle")  parseLedEvent(val, evScanCycle);
  else if (key == "ledConnecting") parseLedEvent(val, evConnecting);
  else if (key == "ledSyncOK")     parseLedEvent(val, evSyncOK);
  else if (key == "ledSyncFail")   parseLedEvent(val, evSyncFail);
  else if (key == "ledTooBig")     parseLedEvent(val, evTooBig);
  else if (key == "ledLowHeap")    parseLedEvent(val, evLowHeap);
  else if (key == "ledDronePulse") parseLedEvent(val, evDronePulse);
  else if (key == "ledHeartbeat")  parseLedEvent(val, evHeartbeat);
  else if (key == "ledPin")        ledPin        = val.toInt();
  else if (key == "ledPinG")       ledPinG       = val.toInt();
  else if (key == "ledPinB")       ledPinB       = val.toInt();
  else if (key == "gpsBaud")       gpsBaud       = val.toInt();
  else if (key == "gpsRxPin")      gpsRxPin      = val.toInt();
  else if (key == "gpsTxPin")      gpsTxPin      = val.toInt();
  else if (key == "nestSsid")      val.toCharArray(nestSsid, sizeof(nestSsid));
  else if (key == "nestPsk")       val.toCharArray(nestPsk,  sizeof(nestPsk));
  else if (key == "nestIp")        val.toCharArray(nestIp, sizeof(nestIp));
  else if (key == "uploadToken")   val.toCharArray(uploadToken, sizeof(uploadToken));
  else if (key == "nestMac")       parseNestMac(val, nestMac);
  else if (key == "syncEvery")           syncEvery           = max(1, (int)val.toInt());
  else if (key == "heartbeatIntervalMs") heartbeatIntervalMs = max(1000, (int)val.toInt());
  else if (key == "wifiChanMs")          wifiChanMs          = constrain(val.toInt(), 20, 500);
  else if (key == "bleScanMs")           bleScanMs           = constrain(val.toInt(), 500, 10000);
  else if (key == "cycleDelayMs")        cycleDelayMs        = max(0, (int)val.toInt());
  else if (key == "maxLogBytes")      maxLogBytes      = constrain(val.toInt(), 1024, 65536);
  else if (key == "lowHeapThreshold") lowHeapThreshold = max(8192, (int)val.toInt());
  else if (key == "verboseSerial")    verboseSerial    = (val == "true" || val == "1");
}

bool setWorkerConfigKey(const String& key, const String& val) {
  applyConfigKey(key, val);
  return saveNvsConfigKey(key.c_str(), val);
}

void loadNvsConfig() {
  Preferences prefs;
  if (!prefs.begin("wasp-worker", true)) {
    Serial.println("[CFG] NVS unavailable — using compiled defaults");
    return;
  }

  int loaded = 0;
  if (prefs.isKey("nestMac")) {
    String mac = prefs.getString("nestMac");
    if (parseNestMac(mac, nestMac)) loaded++;
  }
  if (prefs.isKey("nestSsid")) { prefs.getString("nestSsid", nestSsid, sizeof(nestSsid)); loaded++; }
  if (prefs.isKey("nestPsk"))  { prefs.getString("nestPsk",  nestPsk,  sizeof(nestPsk));  loaded++; }
  if (prefs.isKey("nestIp"))   { prefs.getString("nestIp",   nestIp,   sizeof(nestIp));   loaded++; }
  if (prefs.isKey("uploadToken")) { prefs.getString("uploadToken", uploadToken, sizeof(uploadToken)); loaded++; }
  if (prefs.isKey("syncEvery"))           { syncEvery           = max(1, (int)prefs.getInt("syncEvery", syncEvery)); loaded++; }
  if (prefs.isKey("heartbeatIntervalMs")) { heartbeatIntervalMs = max(1000, (int)prefs.getInt("heartbeatIntervalMs", heartbeatIntervalMs)); loaded++; }
  if (prefs.isKey("verboseSerial"))       { verboseSerial = prefs.getBool("verboseSerial", verboseSerial); loaded++; }

  prefs.end();
  if (loaded > 0) {
    Serial.printf("[CFG] Loaded %d key(s) from NVS (drone / no SD)\n", loaded);
  } else {
    Serial.println("[CFG] NVS empty — using compiled defaults");
  }
}

bool saveNvsConfigKey(const char* key, const String& val) {
  Preferences prefs;
  if (!prefs.begin("wasp-worker", false)) return false;
  bool ok = false;
  if (strcmp(key, "nestMac") == 0 || strcmp(key, "nestSsid") == 0 ||
      strcmp(key, "nestPsk") == 0 || strcmp(key, "nestIp") == 0 ||
      strcmp(key, "uploadToken") == 0)
    ok = prefs.putString(key, val) > 0;
  else if (strcmp(key, "syncEvery") == 0 || strcmp(key, "heartbeatIntervalMs") == 0)
    ok = prefs.putInt(key, val.toInt()) >= 0;
  else if (strcmp(key, "verboseSerial") == 0)
    ok = prefs.putBool(key, val == "true" || val == "1") >= 0;
  prefs.end();
  return ok;
}

void loadWorkerConfig() {
  memcpy(nestMac, NEST_MAC_DEFAULT, 6);
  if (SD.exists("/reset.cfg")) {
    Serial.println("[CFG] /reset.cfg found — using compiled defaults");
    return;
  }
  File cfg = SD.open("/worker.cfg");
  if (!cfg) {
    loadNvsConfig();
    return;
  }
  while (cfg.available()) {
    String line = cfg.readStringUntil('\n');
    line.trim();
    if (line.startsWith("#") || line.isEmpty()) continue;
    int eq = line.indexOf('=');
    if (eq < 0) continue;
    String key = line.substring(0, eq); key.trim();
    String val = line.substring(eq + 1);
    int comment = val.indexOf('#');
    if (comment >= 0) val = val.substring(0, comment);
    val.trim();
    applyConfigKey(key, val);
  }
  cfg.close();
  Serial.println("[CFG] Loaded from /worker.cfg");
}
