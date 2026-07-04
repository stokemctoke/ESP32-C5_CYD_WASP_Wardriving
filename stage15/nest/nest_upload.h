#pragma once

#include "nest_types.h"
#include <SD.h>        // must precede WebServer.h — SD.h adds "using namespace fs;"
#include <WebServer.h>
#include <WiFiServer.h>

extern WebServer   server;
extern WiFiServer  rawServer;
extern char        lastSyncStr[48];

bool isValidMac(const String& mac);
bool isValidFilename(const String& name);
bool isValidUploadToken(const String& token);
void handleRawUpload();
void handleUploadBody();
void handleUpload();
