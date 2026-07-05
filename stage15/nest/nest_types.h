#pragma once

#include <stdint.h>
#include <WString.h>

// ── Pins ──────────────────────────────────────────────────────────────────────
#define TFT_BACKLIGHT  21
#define SD_CS           5
#define SD_SCK         18
#define SD_MISO        19
#define SD_MOSI        23

// ── CYD onboard RGB LED (active LOW) ─────────────────────────────────────────
// GPIO 4 shared with TFT_RST — safe to use after tft.init() completes.
#define NEST_LED_R  4
#define NEST_LED_G  16
#define NEST_LED_B  17

// ── Network ───────────────────────────────────────────────────────────────────
#define WASP_DEFAULT_UPLOAD_TOKEN "waspswarm"
#define ESPNOW_CHANNEL          1
#define HOME_UPLOAD_INTERVAL_MS 300000
#define HOME_CONNECT_TIMEOUT_MS  15000

// ── Worker registry ───────────────────────────────────────────────────────────
#define MAX_WORKERS        8
#define WORKER_TIMEOUT_MS  30000   // active / in-range; hide from home list after this
#define WORKER_REMOVE_MS  300000   // free registry slot after 5 min

// ── Display layout (240 × 320 portrait) ──────────────────────────────────────
#define HEADER_H  28
#define STATUS_H  16
#define ROW_H     52
#define MAX_ROWS   5
#define FOOTER_H  16

// ── Colours (RGB565) — Gallus Gadgets brand palette ───────────────────────────
// Backgrounds: #0E0E12 #15151B #1C1C24 #26262F
// Accents:     #E8900A #F5B445 #C45C12 #8B3A0F
// Text/UI:     #F2EDE6 #9E9890 #504A44 #2C2C35
#define CLR_BG        0x0862   // #0E0E12 — main background
#define CLR_HDR_BG    0x18E4   // #1C1C24 — header / panel
#define CLR_HDR_FG    0xF77C   // #F2EDE6 — primary text
#define CLR_LABEL     0x9CB2   // #9E9890 — secondary text
#define CLR_ACTIVE    0xF5C8   // #F5B445 — worker / positive highlight
#define CLR_DRONE     0xC302   // #C45C12 — drone accent
#define CLR_STALE     0xEC81   // #E8900A — stale / warning
#define CLR_OFFLINE   0x5248   // #504A44 — offline / muted
#define CLR_GPS_OK    0xF5C8   // #F5B445 — GPS fix / success
#define CLR_GPS_NO    0x9CB2   // #9E9890 — no fix
#define CLR_DIVIDER   0x2125   // #26262F — row dividers
#define CLR_FTR_BG    0x10A3   // #15151B — footer bar
#define CLR_ERROR     0xF986   // #FF3333 — errors, weak RSSI, delete (not brand — kept red)

// ── Packet types ─────────────────────────────────────────────────────────────
#define WASP_PKT_SUMMARY   0x01
#define WASP_PKT_HEARTBEAT 0x02
#define WASP_FIRMWARE_VER  15

// ── Touch (CST820 capacitive on I²C) ──────────────────────────────────────────
// JC2432W328C uses a CST820 capacitive controller, not the resistive XPT2046.
// INT is GPIO 21, which conflicts with our TFT backlight — we poll instead.
#define TOUCH_SDA   33
#define TOUCH_SCL   32
#define TOUCH_RST   25
#define CST820_ADDR 0x15

// ── UI layout ─────────────────────────────────────────────────────────────────
#define BACK_BTN_W  44          // back button tap area width
#define BACK_BTN_H  HEADER_H    // back button tap area height (matches header)

// ── UI colours ────────────────────────────────────────────────────────────────
#define CLR_BTN_BG    0x18E4   // #1C1C24 — button background
#define CLR_BTN_ACT   0x2125   // #26262F — pressed / border emphasis
#define CLR_HIGHLIGHT 0xEC81   // #E8900A — selected item

// ── Screen IDs ────────────────────────────────────────────────────────────────
enum ScreenId {
  SCR_HOME = 0,
  SCR_WORKER_DETAIL,
  SCR_FILE_BROWSER,
  SCR_FILE_LIST,
  SCR_SETTINGS,
};

// ── LED event descriptor ──────────────────────────────────────────────────────
// colour = 24-bit RGB; CYD LED is binary so only non-zero channels are checked.
// flashes = 0 means solid on (used for upload-in-progress indicator).
struct LedEvent {
  uint32_t colour;
  int      flashes;
  int      onMs;
  int      offMs;
};

// ── Worker registry entry ─────────────────────────────────────────────────────
struct worker_entry_t {
  uint8_t  mac[6];
  uint32_t lastSeenMs;
  uint32_t lastSummaryMs;
  int8_t   rssi;
  uint8_t  gpsFix;
  uint16_t wifiTotal;
  uint8_t  wifi2g, wifi5g;
  uint16_t bleCount;
  uint32_t cycleCount;
  uint8_t  nodeType;   // 0 = worker, 1 = drone
  bool     offlineNotified;  // track if offline transition has been logged
};

// ── Config struct ─────────────────────────────────────────────────────────────
struct wasp_config_t {
  char homeSsid[64];
  char homePsk[64];
  char apSsid[32];
  char apPsk[32];
  char uploadToken[64];
  char wigleBasicToken[128];
  char wdgwarsApiKey[72];
  uint16_t swarmId;        // 0 = accept all ESP-NOW senders
};

// ── ESP-NOW packet structs ────────────────────────────────────────────────────
typedef struct __attribute__((packed)) {
  uint8_t  type;
  uint8_t  workerMac[6];
  uint8_t  nodeType;
  uint16_t swarmId;
  uint8_t  loyaltyLevel;
  uint8_t  gangId;
  uint8_t  firmwareVer;
  uint8_t  battLevel;
  uint16_t playerLevel;
  uint8_t  boostLevel;
  uint8_t  reserved[7];
} heartbeat_t;             // 24 bytes

typedef struct __attribute__((packed)) {
  uint8_t  type;
  uint8_t  workerMac[6];
  uint8_t  gpsFix;
  float    lat, lon, altM;
  uint8_t  sats;
  float    hdop;
  uint16_t wifiTotal;
  uint8_t  wifi2g, wifi5g;
  uint16_t bleCount;
  int8_t   bestRssi;
  uint32_t cycleCount;
  uint16_t swarmId;
  uint8_t  loyaltyLevel;
  uint8_t  gangId;
  uint8_t  firmwareVer;
  uint8_t  battLevel;
  uint16_t playerLevel;
  uint8_t  boostLevel;
  uint8_t  reserved[7];
} scan_summary_t;          // 52 bytes
