#include "nest_upload.h"
#include "nest_config.h"
#include "nest_registry.h"
#include "nest_led.h"
#include <SD.h>
#include <WiFi.h>
#include <Arduino.h>

WebServer  server(80);
WiFiServer rawServer(8080);
char       lastSyncStr[48] = "none";

extern bool sdOk;

static constexpr int HTTP_UPLOAD_MAX_BODY = 16 * 1024;

static struct {
  File     sdFile;
  size_t   written;
  int      contentLen;
  bool     ok;
  int      httpCode;
  const char* errText;
  String   path;
  String   workerMac;
} gHttpUp;

static struct {
  bool   active;
  String path;
  int    expectedChunk;
  int    totalChunks;
} sChunkSeq = { false, "", 0, 0 };

static void clearChunkSeq() {
  sChunkSeq.active        = false;
  sChunkSeq.path          = "";
  sChunkSeq.expectedChunk = 0;
  sChunkSeq.totalChunks   = 0;
}

static bool resolveUploadMeta(String& workerMac, String& fileName) {
  workerMac = server.header("X-Worker");
  fileName  = server.header("X-File");
  if (!workerMac.isEmpty() && !fileName.isEmpty()) {
    return isValidMac(workerMac) && isValidFilename(fileName);
  }
  if (server.hasArg("worker") && server.hasArg("file")) {
    workerMac = server.arg("worker");
    fileName  = server.arg("file");
    return isValidMac(workerMac) && isValidFilename(fileName);
  }
  return false;
}

bool isValidMac(const String& mac) {
  if (mac.length() != 12) return false;
  for (unsigned int i = 0; i < mac.length(); i++) {
    char c = mac[i];
    if (!((c >= '0' && c <= '9') ||
          (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

bool isValidFilename(const String& name) {
  if (name.isEmpty() || name.length() > 64) return false;
  if (!name.endsWith(".csv")) return false;
  if (name.indexOf("..") >= 0) return false;
  for (unsigned int i = 0; i < name.length(); i++) {
    char c = name[i];
    if (!isAlphaNumeric(c) && c != '_' && c != '-' && c != '.') return false;
  }
  return true;
}

bool isValidUploadToken(const String& token) {
  if (token.isEmpty() || cfg.uploadToken[0] == '\0') return false;
  return token == cfg.uploadToken;
}

void handleRawUpload() {
  WiFiClient client = rawServer.accept();
  if (!client) return;

  client.setTimeout(15000);
  String hdr = client.readStringUntil('\n');
  hdr.trim();

  bool isChunked = hdr.startsWith("UPLOAD_CHUNK ");
  bool isSingle  = !isChunked && hdr.startsWith("UPLOAD ");
  if (!isSingle && !isChunked) {
    client.println("ERR bad header");
    client.stop();
    return;
  }

  String uploadToken, workerMac, fileName;
  int fileSize = 0, chunkIndex = 0, totalChunks = 1;

  if (isSingle) {
    int s1 = hdr.indexOf(' ');
    int s2 = hdr.indexOf(' ', s1 + 1);
    int s3 = hdr.indexOf(' ', s2 + 1);
    int s4 = hdr.indexOf(' ', s3 + 1);
    if (s4 < 0) { client.println("ERR bad header"); client.stop(); return; }
    uploadToken = hdr.substring(s1 + 1, s2);
    workerMac   = hdr.substring(s2 + 1, s3);
    fileName    = hdr.substring(s3 + 1, s4);
    fileSize    = hdr.substring(s4 + 1).toInt();
  } else {
    int s1 = hdr.indexOf(' ');
    int s2 = hdr.indexOf(' ', s1 + 1);
    int s3 = hdr.indexOf(' ', s2 + 1);
    int s4 = hdr.indexOf(' ', s3 + 1);
    int s5 = hdr.indexOf(' ', s4 + 1);
    int s6 = hdr.indexOf(' ', s5 + 1);
    if (s6 < 0) { client.println("ERR bad header"); client.stop(); return; }
    uploadToken = hdr.substring(s1 + 1, s2);
    workerMac   = hdr.substring(s2 + 1, s3);
    fileName    = hdr.substring(s3 + 1, s4);
    chunkIndex  = hdr.substring(s4 + 1, s5).toInt();
    totalChunks = hdr.substring(s5 + 1, s6).toInt();
    fileSize    = hdr.substring(s6 + 1).toInt();
  }

  if (!isValidUploadToken(uploadToken)) {
    Serial.println("[NEST] Rejected upload: bad token");
    client.println("ERR unauthorized");
    client.stop();
    return;
  }

  if (!sdOk || fileSize <= 0) {
    client.println("ERR bad params");
    client.stop();
    return;
  }
  if (!isValidMac(workerMac) || !isValidFilename(fileName)) {
    Serial.printf("[NEST] Rejected upload: mac='%s' file='%s' (validation failed)\n",
                  workerMac.c_str(), fileName.c_str());
    client.println("ERR bad params");
    client.stop();
    return;
  }

  String dir  = "/logs/" + workerMac;
  String path = dir + "/" + fileName;

  if (isChunked) {
    if (totalChunks <= 0 || chunkIndex < 0 || chunkIndex >= totalChunks) {
      Serial.printf("[NEST] Rejected chunk: invalid index %d/%d\n",
                    chunkIndex + 1, totalChunks);
      client.println("ERR bad chunk");
      client.stop();
      return;
    }
    if (!sChunkSeq.active) {
      if (chunkIndex != 0) {
        Serial.printf("[NEST] Rejected chunk %d: no upload in progress\n", chunkIndex);
        client.println("ERR chunk out of order");
        client.stop();
        return;
      }
      sChunkSeq.active        = true;
      sChunkSeq.path          = path;
      sChunkSeq.expectedChunk = 0;
      sChunkSeq.totalChunks   = totalChunks;
    } else if (sChunkSeq.path != path || sChunkSeq.totalChunks != totalChunks ||
               chunkIndex != sChunkSeq.expectedChunk) {
      Serial.printf("[NEST] Rejected chunk %d for %s (expected %d)\n",
                    chunkIndex, fileName.c_str(), sChunkSeq.expectedChunk);
      client.println("ERR chunk out of order");
      client.stop();
      return;
    }
  }

  if (!SD.exists("/logs")) SD.mkdir("/logs");
  if (!SD.exists(dir))     SD.mkdir(dir);

  File f;
  if (chunkIndex == 0) {
    if (SD.exists(path.c_str())) SD.remove(path.c_str());
    f = SD.open(path.c_str(), FILE_WRITE);
  } else {
    f = SD.open(path.c_str(), FILE_APPEND);
    if (!f) f = SD.open(path.c_str(), FILE_WRITE);
  }
  if (!f) {
    if (isChunked) clearChunkSeq();
    client.println("ERR sd open failed");
    client.stop();
    return;
  }

  client.println("READY");

  static uint8_t buf[4096];
  int      remaining = fileSize;
  size_t   written   = 0;
  uint32_t deadline  = millis() + 30000;
  bool     sdFail    = false;
  while (remaining > 0 && client.connected() && millis() < deadline) {
    int n = client.read(buf, min(remaining, (int)sizeof(buf)));
    if (n > 0) {
      size_t wrote = f.write(buf, n);
      written += wrote;
      if ((int)wrote < n) {
        Serial.printf("[NEST] SD write short %u/%d — aborting\n", (unsigned)wrote, n);
        sdFail = true;
        break;
      }
      remaining -= n;
    } else {
      yield();
    }
  }
  f.close();

  if (sdFail) {
    SD.remove(path.c_str());
    if (isChunked) clearChunkSeq();
    client.println("ERR sd write failed");
    client.stop();
    return;
  }

  Serial.printf("[NEST] recv %u/%d B for %s\n", (unsigned)written, fileSize, fileName.c_str());

  if ((int)written < fileSize) {
    if (isChunked) clearChunkSeq();
    client.println("ERR transfer incomplete");
    client.stop();
    SD.remove(path.c_str());
    return;
  }

  if (isChunked) {
    sChunkSeq.expectedChunk++;
    if (chunkIndex == totalChunks - 1) clearChunkSeq();
  }

  client.println("OK");
  client.stop();

  nestLedFlashEvent(evNestChunk);

  Serial.printf("[NEST] Saved %s chunk %d/%d (%u bytes)\n",
                fileName.c_str(), chunkIndex + 1, totalChunks, (unsigned)written);
  if (chunkIndex == totalChunks - 1) {
    taskENTER_CRITICAL(&gLock);
    snprintf(lastSyncStr, sizeof(lastSyncStr), "%s  %dB", workerMac.c_str(), (int)written);
    taskEXIT_CRITICAL(&gLock);
    Serial.printf("[NEST] Complete: %s\n", path.c_str());
  }
}

void handleUploadBody() {
  HTTPRaw& raw = server.raw();

  switch (raw.status) {
  case RAW_START: {
    memset(&gHttpUp, 0, sizeof(gHttpUp));
    gHttpUp.ok         = true;
    gHttpUp.httpCode   = 200;
    gHttpUp.contentLen = server.clientContentLength();
    if (!isValidUploadToken(server.header("X-Upload-Token"))) {
      gHttpUp.ok = false; gHttpUp.httpCode = 401; gHttpUp.errText = "Unauthorized";
      return;
    }
    if (gHttpUp.contentLen <= 0 || gHttpUp.contentLen > HTTP_UPLOAD_MAX_BODY) {
      gHttpUp.ok = false;
      gHttpUp.httpCode = (gHttpUp.contentLen > HTTP_UPLOAD_MAX_BODY) ? 413 : 400;
      gHttpUp.errText  = (gHttpUp.contentLen > HTTP_UPLOAD_MAX_BODY) ? "Too large" : "Bad length";
      return;
    }
    if (!sdOk) {
      gHttpUp.ok = false; gHttpUp.httpCode = 503; gHttpUp.errText = "SD not ready";
      return;
    }
    String workerMac, fileName;
    if (!resolveUploadMeta(workerMac, fileName)) {
      gHttpUp.ok = false; gHttpUp.httpCode = 400; gHttpUp.errText = "Bad params";
      return;
    }
    gHttpUp.workerMac = workerMac;
    String dir = "/logs/" + workerMac;
    gHttpUp.path = dir + "/" + fileName;
    if (!SD.exists("/logs")) SD.mkdir("/logs");
    if (!SD.exists(dir))     SD.mkdir(dir);
    if (SD.exists(gHttpUp.path.c_str())) SD.remove(gHttpUp.path.c_str());
    gHttpUp.sdFile = SD.open(gHttpUp.path.c_str(), FILE_WRITE);
    if (!gHttpUp.sdFile) {
      gHttpUp.ok = false; gHttpUp.httpCode = 500; gHttpUp.errText = "SD open failed";
    }
    break;
  }
  case RAW_WRITE: {
    if (!gHttpUp.ok || !gHttpUp.sdFile) return;
    size_t n = gHttpUp.sdFile.write(raw.buf, raw.currentSize);
    gHttpUp.written += n;
    if (n < raw.currentSize) {
      gHttpUp.ok = false; gHttpUp.httpCode = 500; gHttpUp.errText = "SD write failed";
    }
    break;
  }
  case RAW_END: {
    if (gHttpUp.sdFile) gHttpUp.sdFile.close();
    if (gHttpUp.ok && (int)gHttpUp.written != gHttpUp.contentLen) {
      gHttpUp.ok = false; gHttpUp.httpCode = 500; gHttpUp.errText = "Transfer incomplete";
    }
    if (!gHttpUp.ok && gHttpUp.path.length()) SD.remove(gHttpUp.path.c_str());
    if (gHttpUp.ok) {
      taskENTER_CRITICAL(&gLock);
      snprintf(lastSyncStr, sizeof(lastSyncStr), "%s  %dB",
               gHttpUp.workerMac.c_str(), gHttpUp.contentLen);
      taskEXIT_CRITICAL(&gLock);
      Serial.printf("[NEST] Saved %s (%d bytes)\n", gHttpUp.path.c_str(), gHttpUp.contentLen);
    }
    break;
  }
  case RAW_ABORTED: {
    gHttpUp.ok = false; gHttpUp.httpCode = 500; gHttpUp.errText = "Upload aborted";
    if (gHttpUp.sdFile) gHttpUp.sdFile.close();
    if (gHttpUp.path.length()) SD.remove(gHttpUp.path.c_str());
    break;
  }
  default:
    break;
  }
}

void handleUpload() {
  if (!gHttpUp.ok) {
    server.send(gHttpUp.httpCode, "text/plain", gHttpUp.errText ? gHttpUp.errText : "Upload failed");
    return;
  }
  server.send(200, "text/plain", "OK");
}
