#include "nest_display.h"
#include "nest_registry.h"
#include "nest_home.h"
#include "nest_config.h"
#include "nest_upload.h"
#include "nest_ui.h"
#include "nest_sd.h"
#include "../common/wasp_mac.h"
#include <Arduino.h>
#include <SD.h>

TFT_eSPI tft = TFT_eSPI();

// ── File browser / list cache ─────────────────────────────────────────────────
#define MAX_BROWSER_WORKERS  8
#define MAX_FILE_ENTRIES    20

struct BrowserEntry { char mac12[13]; int fileCount; long totalBytes; };
static BrowserEntry browserEntries[MAX_BROWSER_WORKERS];
static int          browserEntryCount = 0;
static bool         browserLoaded     = false;

struct FileEntry { char name[48]; long size; };
static FileEntry fileEntries[MAX_FILE_ENTRIES];
static int       fileEntryCount = 0;
static bool      fileListLoaded = false;

// Modal state for file delete confirmation
static bool fileModalShown = false;
static int  fileModalIdx   = -1;

// Worker detail — VIEW FILES button (draw + tap must match)
static int detailViewFilesBtnY = 0;
static const int DETAIL_BTN_W  = 220;
static const int DETAIL_BTN_H  = 30;

// Home incremental redraw cache
#define FOOTER_ROTATE_MS 4000

struct HomeRowCache {
  uint8_t  mac[6];
  int8_t   rssi;
  uint8_t  gpsFix;
  uint16_t wifiTotal;
  uint8_t  wifi2g, wifi5g;
  uint16_t bleCount;
  uint32_t cycleCount;
  uint8_t  nodeType;
  uint32_t lastSeenMs;
  bool     valid;
};

static HomeRowCache homeRowCache[MAX_ROWS];
static bool         homeStaticDrawn = false;
static int          homeLastActive  = -1;
static uint8_t      homeLastStatus  = 255;
static uint32_t     homeFooterNextMs = 0;
static uint8_t      homeFooterSlot   = 0;

void uiInvalidateBrowser()  { browserLoaded  = false; }
void uiInvalidateFileList() { fileListLoaded = false; }

// ── Helpers ───────────────────────────────────────────────────────────────────
static uint16_t rssiColor(int8_t r) {
  if (r > -50) return CLR_ACTIVE;
  if (r > -70) return CLR_STALE;
  return CLR_ERROR;   // weak signal — red warning
}

static void drawBrandMark() {
  tft.setTextFont(1);
  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(CLR_LABEL, CLR_HDR_BG);
  tft.drawString("Gallus Gadgets", 236, HEADER_H - 2);
}

static void drawScreenHeader(const char* title) {
  tft.fillRect(0, 0, 240, HEADER_H, CLR_HDR_BG);
  tft.setTextColor(CLR_HDR_FG, CLR_HDR_BG);
  tft.setTextFont(4);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("<", 6, HEADER_H / 2);
  tft.drawRect(2, 2, BACK_BTN_W, BACK_BTN_H - 4, CLR_HIGHLIGHT);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(title, 148, HEADER_H / 2);
  drawBrandMark();
}

static void drawBtn(int x, int y, int w, int h, const char* label) {
  tft.fillRect(x, y, w, h, CLR_BTN_BG);
  tft.drawRect(x, y, w, h, CLR_BTN_ACT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(CLR_HDR_FG, CLR_BTN_BG);
  tft.drawString(label, x + w / 2, y + h / 2);
}

// ── Boot ──────────────────────────────────────────────────────────────────────
void drawBootMsg(const char* msg) {
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(CLR_LABEL, CLR_BG);
  tft.fillRect(0, HEADER_H + STATUS_H, 240, ROW_H, CLR_BG);
  tft.drawString(msg, 120, HEADER_H + STATUS_H + ROW_H / 2);
}

void drawHeader() {
  tft.fillRect(0, 0, 240, HEADER_H, CLR_HDR_BG);
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(CLR_HDR_FG, CLR_HDR_BG);
  tft.drawString("W.A.S.P.", 6, 4);
  tft.setTextFont(1);
  tft.setTextColor(CLR_LABEL, CLR_HDR_BG);
  tft.drawString("Gallus Gadgets", 6, 18);
}

// ── HOME ─────────────────────────────────────────────────────────────────────
static void drawWorkerRow(int row, const worker_entry_t& w) {
  int y = HEADER_H + STATUS_H + row * ROW_H;
  uint32_t age   = millis() - w.lastSeenMs;
  bool active    = age < WORKER_TIMEOUT_MS;
  bool stale     = age >= WORKER_STALE_MS && active;
  bool isDrone   = (w.nodeType == 1);
  uint16_t base  = isDrone ? CLR_DRONE : CLR_ACTIVE;
  uint16_t nameC = active ? (stale ? CLR_STALE : base) : CLR_OFFLINE;

  tft.fillRect(0, y, 240, ROW_H, CLR_BG);
  tft.drawFastHLine(0, y + ROW_H - 1, 240, CLR_DIVIDER);
  tft.fillCircle(8, y + 14, 5, nameC);

  tft.setTextFont(1);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(nameC, CLR_BG);
  tft.drawString(isDrone ? "D" : "W", 16, y + 10);

  char mac[18];
  formatMacColon(w.mac, mac, sizeof(mac));
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(nameC, CLR_BG);
  tft.drawString(mac, 26, y + 14);

  char rssiStr[10];
  snprintf(rssiStr, sizeof(rssiStr), "%d dBm", w.rssi);
  tft.setTextColor(rssiColor(w.rssi), CLR_BG);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(rssiStr, 234, y + 14);

  tft.setTextDatum(ML_DATUM);
  if (w.lastSummaryMs == 0) {
    tft.setTextColor(CLR_LABEL, CLR_BG);
    tft.drawString("awaiting scan data...", 26, y + 38);
    return;
  }
  if (w.gpsFix) {
    tft.setTextColor(CLR_GPS_OK, CLR_BG);
    tft.drawString("FIX", 26, y + 38);
  } else {
    tft.setTextColor(CLR_GPS_NO, CLR_BG);
    tft.drawString("NO FIX", 26, y + 38);
  }
  char scanLine[40];
  snprintf(scanLine, sizeof(scanLine), "W:%d(%d+%d) B:%d #%u",
           w.wifiTotal, w.wifi2g, w.wifi5g, w.bleCount, (unsigned)w.cycleCount);
  tft.setTextColor(CLR_LABEL, CLR_BG);
  tft.drawString(scanLine, w.gpsFix ? 64 : 94, y + 38);
}

static void drawHomeStatic() {
  if (homeStaticDrawn) return;
  drawHeader();
  tft.fillRect(170, 0, 70, HEADER_H, CLR_HDR_BG);
  drawBtn(174, 4, 30, 20, "F");
  drawBtn(206, 4, 30, 20, "S");
  homeStaticDrawn = true;
}

static void drawHomeStatusBar(int active) {
  tft.fillRect(0, HEADER_H, 240, STATUS_H, CLR_BG);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(CLR_LABEL, CLR_BG);
  char buf[32];
  if (active > MAX_ROWS)
    snprintf(buf, sizeof(buf), "Workers: %d +%d  ch:%d", MAX_ROWS, active - MAX_ROWS, ESPNOW_CHANNEL);
  else
    snprintf(buf, sizeof(buf), "Workers: %d  ch:%d", active, ESPNOW_CHANNEL);
  tft.drawString(buf, 6, HEADER_H + STATUS_H / 2);
  if (cfg.homeSsid[0]) {
    uint16_t hCol = (homeStatus == 1) ? CLR_GPS_OK : (homeStatus == 2) ? CLR_ERROR : CLR_OFFLINE;
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(hCol, CLR_BG);
    tft.drawString("H", 234, HEADER_H + STATUS_H / 2);
  }
}

static void drawHomeFooter(const char* syncSnap, const char* wigleSnap, const char* wdgSnap) {
  int fy = 320 - FOOTER_H;
  tft.fillRect(0, fy, 240, FOOTER_H, CLR_FTR_BG);
  tft.setTextFont(1);
  tft.setTextDatum(ML_DATUM);
  char fbuf[52];
  if (homeFooterSlot == 0) {
    tft.setTextColor(CLR_LABEL, CLR_FTR_BG);
    snprintf(fbuf, sizeof(fbuf), "Sync: %s", syncSnap);
  } else if (homeFooterSlot == 1) {
    tft.setTextColor(CLR_GPS_OK, CLR_FTR_BG);
    snprintf(fbuf, sizeof(fbuf), "WiGLE: %s", wigleSnap);
  } else {
    tft.setTextColor(CLR_STALE, CLR_FTR_BG);
    snprintf(fbuf, sizeof(fbuf), "WDG: %s", wdgSnap);
  }
  tft.drawString(fbuf, 6, fy + FOOTER_H / 2);
}

static bool homeRowChanged(const worker_entry_t& w, const HomeRowCache& c, uint32_t now) {
  if (!c.valid) return true;
  if (memcmp(c.mac, w.mac, 6) != 0) return true;
  if (c.rssi != w.rssi || c.gpsFix != w.gpsFix) return true;
  if (c.wifiTotal != w.wifiTotal || c.wifi2g != w.wifi2g || c.wifi5g != w.wifi5g) return true;
  if (c.bleCount != w.bleCount || c.cycleCount != w.cycleCount) return true;
  if (c.nodeType != w.nodeType) return true;
  bool wasStale = c.lastSeenMs > 0 && (now - c.lastSeenMs) >= WORKER_STALE_MS &&
                  (now - c.lastSeenMs) < WORKER_TIMEOUT_MS;
  bool isStale  = (now - w.lastSeenMs) >= WORKER_STALE_MS &&
                  (now - w.lastSeenMs) < WORKER_TIMEOUT_MS;
  if (wasStale != isStale) return true;
  return false;
}

static void cacheHomeRow(int row, const worker_entry_t& w) {
  HomeRowCache& c = homeRowCache[row];
  memcpy(c.mac, w.mac, 6);
  c.rssi = w.rssi;
  c.gpsFix = w.gpsFix;
  c.wifiTotal = w.wifiTotal;
  c.wifi2g = w.wifi2g;
  c.wifi5g = w.wifi5g;
  c.bleCount = w.bleCount;
  c.cycleCount = w.cycleCount;
  c.nodeType = w.nodeType;
  c.lastSeenMs = w.lastSeenMs;
  c.valid = true;
}

static void clearHomeRow(int row) {
  if (!homeRowCache[row].valid) return;
  int y = HEADER_H + STATUS_H + row * ROW_H;
  tft.fillRect(0, y, 240, ROW_H, CLR_BG);
  homeRowCache[row].valid = false;
}

static void refreshHomeRows(const worker_entry_t snap[MAX_WORKERS], uint32_t now) {
  worker_entry_t visible[MAX_ROWS];
  int visCount = 0;
  for (int i = 0; i < MAX_WORKERS && visCount < MAX_ROWS; i++) {
    if (snap[i].lastSeenMs > 0 && (now - snap[i].lastSeenMs) < WORKER_TIMEOUT_MS)
      visible[visCount++] = snap[i];
  }
  for (int row = 0; row < MAX_ROWS; row++) {
    if (row < visCount) {
      if (homeRowChanged(visible[row], homeRowCache[row], now)) {
        drawWorkerRow(row, visible[row]);
        cacheHomeRow(row, visible[row]);
      }
    } else {
      clearHomeRow(row);
    }
  }
}

static void refreshHome(bool fullPaint) {
  worker_entry_t snap[MAX_WORKERS];
  char syncSnap[sizeof(lastSyncStr)];
  char wigleSnap[sizeof(lastWigleStr)];
  char wdgSnap[sizeof(lastWdgStr)];
  taskENTER_CRITICAL(&gLock);
  memcpy(snap,      workers,      sizeof(snap));
  memcpy(syncSnap,  lastSyncStr,  sizeof(syncSnap));
  memcpy(wigleSnap, lastWigleStr, sizeof(wigleSnap));
  memcpy(wdgSnap,   lastWdgStr,   sizeof(wdgSnap));
  taskEXIT_CRITICAL(&gLock);

  uint32_t now = millis();
  int active = 0;
  for (int i = 0; i < MAX_WORKERS; i++)
    if (snap[i].lastSeenMs > 0 && (now - snap[i].lastSeenMs) < WORKER_TIMEOUT_MS) active++;

  if (fullPaint) {
    homeStaticDrawn = false;
    memset(homeRowCache, 0, sizeof(homeRowCache));
    homeLastActive = -1;
    homeLastStatus = 255;
  }

  drawHomeStatic();
  if (fullPaint || active != homeLastActive || homeStatus != homeLastStatus) {
    drawHomeStatusBar(active);
    homeLastActive = active;
    homeLastStatus = homeStatus;
  }

  refreshHomeRows(snap, now);

  uint32_t t = millis();
  if (fullPaint || t >= homeFooterNextMs) {
    if (t >= homeFooterNextMs) {
      homeFooterSlot = (homeFooterSlot + 1) % 3;
      homeFooterNextMs = t + FOOTER_ROTATE_MS;
    } else if (fullPaint) {
      homeFooterNextMs = t + FOOTER_ROTATE_MS;
    }
    drawHomeFooter(syncSnap, wigleSnap, wdgSnap);
  }
}

void drawHome() {
  refreshHome(true);
}

// ── WORKER DETAIL ─────────────────────────────────────────────────────────────
void drawWorkerDetail() {
  drawScreenHeader("WORKER DETAIL");
  tft.fillRect(0, HEADER_H, 240, 320 - HEADER_H, CLR_BG);

  worker_entry_t snap[MAX_WORKERS];
  taskENTER_CRITICAL(&gLock);
  memcpy(snap, workers, sizeof(snap));
  taskEXIT_CRITICAL(&gLock);

  // Find worker by MAC
  const worker_entry_t* w = nullptr;
  for (int i = 0; i < MAX_WORKERS; i++) {
    char mac[18];
    formatMacColon(snap[i].mac, mac, sizeof(mac));
    if (strcmp(mac, uiDetailMac) == 0) { w = &snap[i]; break; }
  }

  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  int y = HEADER_H + 8;
  const int lh = 18;

  if (!w || w->lastSeenMs == 0) {
    tft.setTextColor(CLR_OFFLINE, CLR_BG);
    tft.drawString("Worker not found", 6, y);
    return;
  }

  uint32_t age = millis() - w->lastSeenMs;
  bool active  = age < WORKER_TIMEOUT_MS;

  tft.setTextColor(active ? CLR_ACTIVE : CLR_OFFLINE, CLR_BG);
  tft.drawString(uiDetailMac, 6, y); y += lh + 2;

  char buf[48];
  tft.setTextColor(CLR_LABEL, CLR_BG);
  snprintf(buf, sizeof(buf), "Type: %s   Status: %s",
           w->nodeType == 1 ? "Drone" : "Worker", active ? "Active" : "Offline");
  tft.drawString(buf, 6, y); y += lh;

  snprintf(buf, sizeof(buf), "Last seen: %lu s ago", age / 1000);
  tft.drawString(buf, 6, y); y += lh;

  tft.setTextColor(rssiColor(w->rssi), CLR_BG);
  snprintf(buf, sizeof(buf), "RSSI: %d dBm", w->rssi);
  tft.drawString(buf, 6, y); y += lh;

  tft.setTextColor(w->gpsFix ? CLR_GPS_OK : CLR_GPS_NO, CLR_BG);
  tft.drawString(w->gpsFix ? "GPS: FIX" : "GPS: NO FIX", 6, y); y += lh;
  if (w->gpsFix) {
    tft.setTextColor(CLR_LABEL, CLR_BG);
    snprintf(buf, sizeof(buf), "  %.5f, %.5f", w->lat, w->lon);
    tft.drawString(buf, 6, y); y += lh;
    snprintf(buf, sizeof(buf), "  alt %.0fm  sats %u  hdop %.1f", w->altM, w->sats, w->hdop);
    tft.drawString(buf, 6, y); y += lh;
  }

  tft.setTextColor(CLR_LABEL, CLR_BG);
  if (w->lastSummaryMs > 0) {
    snprintf(buf, sizeof(buf), "WiFi: %d  (2.4G:%d  5G:%d)", w->wifiTotal, w->wifi2g, w->wifi5g);
    tft.drawString(buf, 6, y); y += lh;
    snprintf(buf, sizeof(buf), "BLE: %d devices", w->bleCount);
    tft.drawString(buf, 6, y); y += lh;
    snprintf(buf, sizeof(buf), "Cycles: %lu", (unsigned long)w->cycleCount);
    tft.drawString(buf, 6, y); y += lh;
  } else {
    tft.setTextColor(CLR_STALE, CLR_BG);
    tft.drawString("No scan data yet", 6, y); y += lh;
  }

  y += 4;
  tft.drawFastHLine(6, y, 228, CLR_DIVIDER); y += 8;

  // Count files for this worker on SD
  int fCount = 0; long fBytes = 0;
  String dir = "/logs/" + String(uiDetailMac12);
  sdTake();
  File d = SD.open(dir.c_str());
  if (d) {
    while (true) {
      File e = d.openNextFile();
      if (!e) break;
      if (!e.isDirectory()) { fCount++; fBytes += e.size(); }
      e.close();
    }
    d.close();
  }
  sdGive();
  tft.setTextColor(CLR_LABEL, CLR_BG);
  snprintf(buf, sizeof(buf), "Files on SD: %d  (%ld KB)", fCount, fBytes / 1024);
  tft.drawString(buf, 6, y); y += lh + 4;

  detailViewFilesBtnY = y;
  drawBtn(10, y, DETAIL_BTN_W, DETAIL_BTN_H, "VIEW FILES");
}

// ── FILE BROWSER ─────────────────────────────────────────────────────────────
static void loadBrowserData() {
  browserEntryCount = 0;
  sdTake();
  File root = SD.open("/logs");
  if (!root) { sdGive(); browserLoaded = true; return; }
  while (browserEntryCount < MAX_BROWSER_WORKERS) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) { entry.close(); continue; }
    String name = String(entry.name());
    entry.close();
    if (name.length() != 12) continue;
    BrowserEntry& be = browserEntries[browserEntryCount++];
    name.toCharArray(be.mac12, sizeof(be.mac12));
    be.fileCount = 0; be.totalBytes = 0;
    String subPath = "/logs/" + name;
    File sub = SD.open(subPath.c_str());
    if (sub) {
      while (true) {
        File f = sub.openNextFile();
        if (!f) break;
        if (!f.isDirectory()) { be.fileCount++; be.totalBytes += f.size(); }
        f.close();
      }
      sub.close();
    }
  }
  root.close();
  sdGive();
  browserLoaded = true;
}

void drawFileBrowser() {
  if (!browserLoaded) loadBrowserData();
  drawScreenHeader("FILES");
  tft.fillRect(0, HEADER_H, 240, 320 - HEADER_H, CLR_BG);

  if (browserEntryCount == 0) {
    tft.setTextFont(2);
    tft.setTextColor(CLR_LABEL, CLR_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No log files on SD", 120, 160);
    return;
  }

  const int rowH = 44;
  for (int i = 0; i < browserEntryCount && i < 6; i++) {
    int y = HEADER_H + i * rowH;
    BrowserEntry& be = browserEntries[i];
    tft.fillRect(0, y, 240, rowH, CLR_BG);
    tft.drawFastHLine(0, y + rowH - 1, 240, CLR_DIVIDER);
    tft.fillCircle(8, y + rowH / 2, 4, CLR_ACTIVE);

    char dispMac[18];
    snprintf(dispMac, sizeof(dispMac), "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
             be.mac12[0],be.mac12[1],be.mac12[2],be.mac12[3],
             be.mac12[4],be.mac12[5],be.mac12[6],be.mac12[7],
             be.mac12[8],be.mac12[9],be.mac12[10],be.mac12[11]);

    tft.setTextFont(2);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(CLR_ACTIVE, CLR_BG);
    tft.drawString(dispMac, 18, y + 12);

    char info[32];
    snprintf(info, sizeof(info), "%d file%s  %ld KB",
             be.fileCount, be.fileCount == 1 ? "" : "s", be.totalBytes / 1024);
    tft.setTextColor(CLR_LABEL, CLR_BG);
    tft.drawString(info, 18, y + 28);
  }
}

// ── FILE LIST ─────────────────────────────────────────────────────────────────
static void loadFileListData() {
  fileEntryCount = 0;
  String path = "/logs/" + String(uiDetailMac12);
  sdTake();
  File dir = SD.open(path.c_str());
  if (!dir) { sdGive(); fileListLoaded = true; return; }
  while (fileEntryCount < MAX_FILE_ENTRIES) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) { entry.close(); continue; }
    String n = String(entry.name());
    FileEntry& fe = fileEntries[fileEntryCount++];
    n.toCharArray(fe.name, sizeof(fe.name));
    fe.size = entry.size();
    entry.close();
  }
  dir.close();
  sdGive();
  fileListLoaded = true;
}

static void drawFileModal() {
  if (!fileModalShown || fileModalIdx < 0 || fileModalIdx >= fileEntryCount) return;
  int mx = 20, my = 110, mw = 200, mh = 80;
  tft.fillRect(mx, my, mw, mh, CLR_BTN_BG);
  tft.drawRect(mx, my, mw, mh, CLR_HDR_FG);
  tft.setTextFont(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(CLR_HDR_FG, CLR_BTN_BG);
  tft.drawString("Delete file?", mx + mw / 2, my + 14);
  char truncName[24];
  strncpy(truncName, fileEntries[fileModalIdx].name, 23);
  truncName[23] = '\0';
  tft.setTextColor(CLR_LABEL, CLR_BTN_BG);
  tft.drawString(truncName, mx + mw / 2, my + 30);
  drawBtn(mx + 6,        my + 46, 86, 26, "YES");
  drawBtn(mx + mw - 92,  my + 46, 86, 26, "NO");
}

void drawFileList() {
  if (!fileListLoaded) loadFileListData();

  char title[20];
  snprintf(title, sizeof(title), "%.6s FILES", uiDetailMac12 + 6);
  drawScreenHeader(title);
  tft.fillRect(0, HEADER_H, 240, 320 - HEADER_H, CLR_BG);

  if (fileEntryCount == 0) {
    tft.setTextFont(2);
    tft.setTextColor(CLR_LABEL, CLR_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No files", 120, 160);
    return;
  }

  const int rowH = 36;
  for (int i = 0; i < fileEntryCount && i < 7; i++) {
    int y = HEADER_H + i * rowH;
    FileEntry& fe = fileEntries[i];
    bool isDone  = strstr(fe.name, ".done")  != nullptr;
    bool isDefer = strstr(fe.name, ".defer") != nullptr;
    uint16_t col = isDone ? CLR_GPS_OK : isDefer ? CLR_STALE : CLR_LABEL;

    tft.fillRect(0, y, 240, rowH, CLR_BG);
    tft.drawFastHLine(0, y + rowH - 1, 240, CLR_DIVIDER);

    char dispName[28];
    strncpy(dispName, fe.name, 27);
    dispName[27] = '\0';

    tft.setTextFont(1);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(col, CLR_BG);
    tft.drawString(dispName, 6, y + 10);

    char szStr[12];
    snprintf(szStr, sizeof(szStr), "%ld B", fe.size);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(CLR_LABEL, CLR_BG);
    tft.drawString(szStr, 210, y + 10);

    // Delete 'x' target
    tft.setTextColor(CLR_ERROR, CLR_BG);
    tft.drawString("x", 234, y + 18);
  }

  if (fileModalShown) drawFileModal();
}

// ── SETTINGS ─────────────────────────────────────────────────────────────────
void drawSettings() {
  char wigleSnap[sizeof(lastWigleStr)];
  char wdgSnap[sizeof(lastWdgStr)];
  taskENTER_CRITICAL(&gLock);
  memcpy(wigleSnap, lastWigleStr, sizeof(wigleSnap));
  memcpy(wdgSnap,   lastWdgStr,   sizeof(wdgSnap));
  taskEXIT_CRITICAL(&gLock);

  drawScreenHeader("SETTINGS");
  tft.fillRect(0, HEADER_H, 240, 320 - HEADER_H, CLR_BG);

  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  int y = HEADER_H + 10;
  const int lh = 20;
  char buf[48];

  tft.setTextColor(CLR_LABEL, CLR_BG);
  tft.drawString("Home WiFi:", 6, y); y += lh;
  tft.setTextColor(cfg.homeSsid[0] ? CLR_ACTIVE : CLR_OFFLINE, CLR_BG);
  tft.drawString(cfg.homeSsid[0] ? cfg.homeSsid : "not configured", 14, y); y += lh;

  uint16_t hCol = (homeStatus == 1) ? CLR_GPS_OK : (homeStatus == 2) ? CLR_ERROR : CLR_OFFLINE;
  const char* hStr = (homeStatus == 1) ? "Connected" : (homeStatus == 2) ? "Failed" : "Offline";
  tft.setTextColor(hCol, CLR_BG);
  tft.drawString(hStr, 14, y); y += lh + 4;

  tft.drawFastHLine(6, y, 228, CLR_DIVIDER); y += 8;

  tft.setTextColor(CLR_LABEL, CLR_BG);
  snprintf(buf, sizeof(buf), "Auto-upload: every %lu min", (unsigned long)(HOME_UPLOAD_INTERVAL_MS / 60000));
  tft.drawString(buf, 6, y); y += lh + 4;

  tft.drawFastHLine(6, y, 228, CLR_DIVIDER); y += 8;

  tft.setTextColor(CLR_GPS_OK, CLR_BG);
  snprintf(buf, sizeof(buf), "WiGLE: %s", wigleSnap);
  tft.drawString(buf, 6, y); y += lh;
  tft.setTextColor(CLR_STALE, CLR_BG);
  snprintf(buf, sizeof(buf), "WDG:   %s", wdgSnap);
  tft.drawString(buf, 6, y); y += lh + 8;

  const int btnY = y;
  if (isHomeUploadRunning()) {
    tft.fillRect(10, btnY, 220, 34, CLR_BUSY);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(CLR_HDR_FG, CLR_BUSY);
    tft.drawString("Uploading...", 120, btnY + 17);
  } else {
    drawBtn(10, btnY, 220, 34, "UPLOAD NOW");
  }
  y += 42;

  tft.setTextFont(1);
  tft.setTextColor(CLR_OFFLINE, CLR_BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("Edit wasp.cfg on SD to change settings", 6, y);
}

// ── DISPATCHER ───────────────────────────────────────────────────────────────
void drawCurrentScreen() {
  switch (uiCurrent()) {
    case SCR_HOME:          drawHome();          break;
    case SCR_WORKER_DETAIL: drawWorkerDetail();  break;
    case SCR_FILE_BROWSER:  drawFileBrowser();   break;
    case SCR_FILE_LIST:     drawFileList();      break;
    case SCR_SETTINGS:      drawSettings();      break;
  }
}

void refreshDisplay() {
  // 1Hz tick: Home auto-redraws for live worker stats. Settings redraws when
  // a manual upload finishes so the button returns to normal.
  static bool prevUploadRunning = false;
  bool uploading = isHomeUploadRunning();

  if (uiCurrent() == SCR_HOME) {
    refreshHome(false);
  } else if (uiCurrent() == SCR_SETTINGS && prevUploadRunning && !uploading) {
    drawSettings();
  }
  prevUploadRunning = uploading;
}

void dispatchTap(int px, int py) {
  switch (uiCurrent()) {
    case SCR_HOME:          handleTapHome(px, py);          break;
    case SCR_WORKER_DETAIL: handleTapWorkerDetail(px, py);  break;
    case SCR_FILE_BROWSER:  handleTapFileBrowser(px, py);   break;
    case SCR_FILE_LIST:     handleTapFileList(px, py);      break;
    case SCR_SETTINGS:      handleTapSettings(px, py);      break;
  }
}

// ── TAP HANDLERS ─────────────────────────────────────────────────────────────
void handleTapHome(int px, int py) {
  if (py < HEADER_H) {
    if (px >= 170 && px < 204) { uiInvalidateBrowser(); uiTransitionTo(SCR_FILE_BROWSER); return; }
    if (px >= 204 && px < 240) { uiTransitionTo(SCR_SETTINGS); return; }
  }
  int contentY = HEADER_H + STATUS_H;
  int footerY  = 320 - FOOTER_H;
  if (py >= contentY && py < footerY) {
    int row = (py - contentY) / ROW_H;
    worker_entry_t snap[MAX_WORKERS];
    taskENTER_CRITICAL(&gLock);
    memcpy(snap, workers, sizeof(snap));
    taskEXIT_CRITICAL(&gLock);
    uint32_t now = millis();
    int visible = 0;
    for (int i = 0; i < MAX_WORKERS; i++) {
      if (snap[i].lastSeenMs > 0 && (now - snap[i].lastSeenMs) < WORKER_TIMEOUT_MS) {
        if (visible == row) {
          formatMacColon(snap[i].mac, uiDetailMac, sizeof(uiDetailMac));
          formatMac12(snap[i].mac, uiDetailMac12, sizeof(uiDetailMac12));
          uiInvalidateFileList();
          uiTransitionTo(SCR_WORKER_DETAIL);
          return;
        }
        visible++;
      }
    }
  }
}

void handleTapWorkerDetail(int px, int py) {
  if (uiBackHit(px, py)) { uiBack(); return; }
  if (px > 10 && px < 10 + DETAIL_BTN_W &&
      py >= detailViewFilesBtnY && py < detailViewFilesBtnY + DETAIL_BTN_H) {
    uiInvalidateFileList();
    uiTransitionTo(SCR_FILE_LIST);
  }
}

void handleTapFileBrowser(int px, int py) {
  if (uiBackHit(px, py)) { uiBack(); return; }
  const int rowH = 44;
  int row = (py - HEADER_H) / rowH;
  if (row >= 0 && row < browserEntryCount) {
    strncpy(uiDetailMac12, browserEntries[row].mac12, sizeof(uiDetailMac12));
    mac12ToColon(uiDetailMac12, uiDetailMac, sizeof(uiDetailMac));
    uiInvalidateFileList();
    uiTransitionTo(SCR_FILE_LIST);
  }
}

void handleTapFileList(int px, int py) {
  if (fileModalShown) {
    int mx = 20, my = 110, mw = 200;
    bool hitYes = (px >= mx + 6       && px <= mx + 92      && py >= my + 46 && py <= my + 72);
    bool hitNo  = (px >= mx + mw - 92  && px <= mx + mw - 6  && py >= my + 46 && py <= my + 72);
    if (hitYes && fileModalIdx >= 0 && fileModalIdx < fileEntryCount) {
      String fileName = String(fileEntries[fileModalIdx].name);
      if (!isValidFilename(fileName)) {
        Serial.printf("[UI] Rejected delete: bad filename '%s'\n", fileName.c_str());
      } else {
        String path = "/logs/" + String(uiDetailMac12) + "/" + fileName;
        sdTake();
        SD.remove(path.c_str());
        sdGive();
        Serial.printf("[UI] Deleted %s\n", path.c_str());
      }
      fileModalShown = false; fileModalIdx = -1;
      uiInvalidateFileList(); uiInvalidateBrowser();
      drawCurrentScreen();
    } else if (hitNo) {
      fileModalShown = false; fileModalIdx = -1;
      drawCurrentScreen();
    }
    return;
  }
  if (uiBackHit(px, py)) { uiBack(); return; }
  const int rowH = 36;
  int row = (py - HEADER_H) / rowH;
  if (row >= 0 && row < fileEntryCount && px > 200) {
    fileModalShown = true;
    fileModalIdx   = row;
    drawFileModal();
  }
}

void handleTapSettings(int px, int py) {
  if (uiBackHit(px, py)) { uiBack(); return; }
  // UPLOAD NOW button — y position in drawSettings is approx HEADER_H + 10 + 6*lh + 2*divider+padding
  // lh=20, so roughly y = 28 + 10 + 120 + 24 = 182, button height 34 → range ~182–216
  // Use generous range to be touch-friendly
  if (px > 10 && px < 230 && py > 170 && py < 230) {
    if (isHomeUploadRunning()) return;
    requestHomeUpload();
    drawSettings();
  }
}
