#!/bin/bash
# Creates pre-release audit GitHub issues. Idempotent: skips if title already exists.
set -euo pipefail
REPO="stokemctoke/ESP32-C5_CYD_WASP_Wardriving"

create_issue() {
  local title="$1"
  local labels="$2"
  local body="$3"
  if gh issue list --repo "$REPO" --search "in:title \"$title\"" --json title --jq '.[].title' 2>/dev/null | grep -qxF "$title"; then
    echo "SKIP (exists): $title"
    return 0
  fi
  gh issue create --repo "$REPO" --title "$title" --label "$labels" --body "$body"
  echo "CREATED: $title"
}

create_issue \
  "[Critical] SD card accessed concurrently without mutex" \
  "bug" \
  "## Problem
SD operations run from \`uploadTask\` (FreeRTOS core 0) and from \`loop()\` (home merge/upload, file browser, delete) with no mutex.

## Files
- \`stage15/nest/nest.ino\`
- \`stage15/nest/nest_upload.cpp\`
- \`stage15/nest/nest_home.cpp\`
- \`stage15/nest/nest_display.cpp\`

## Risk
Concurrent FAT access can corrupt files, produce partial CSV merges, or crash the SD layer.

## Fix
Add a single \`SemaphoreHandle_t sdMutex\` (or marshal all SD I/O through one task/queue).

## Audit
Pre-release audit #1 on branch \`cursor/pre-release-audit-f920\`."

create_issue \
  "[Critical] Upload endpoints lack authentication" \
  "bug" \
  "## Problem
Port 8080 raw TCP and port 80 \`POST /upload\` accept writes from any client on the Nest AP. Default AP credentials are weak (\`WASP-Nest\` / \`waspswarm\`).

## Files
- \`stage15/nest/nest_upload.cpp\`
- \`stage15/worker/worker_sync.cpp\`
- \`stage15/nest/nest_config.cpp\`
- \`stage15/worker/worker_config.cpp\`

## Fix
Shared secret in upload header, MAC allowlist, or per-device provisioning. Document rotating default AP password before field use.

## Audit
Pre-release audit #2."

create_issue \
  "[Critical] HTTP /upload loads entire body into RAM" \
  "bug" \
  "## Problem
\`handleUpload()\` uses \`String body = server.arg(\"plain\")\`, duplicating the full POST body on heap. Drone CSV uploads can be ~10–15 KB; risks heap exhaustion on ESP32.

## Files
- \`stage15/nest/nest_upload.cpp:160-194\`
- \`stage15/worker/worker_sync.cpp\` (drone HTTP POST)

## Fix
Stream from \`WiFiClient\` with max-size check, or enforce hard byte limit; consider consolidating on port 8080 chunked protocol.

## Audit
Pre-release audit #3."

create_issue \
  "[High] Drone mode sends nodeType=0 in heartbeats" \
  "bug" \
  "## Problem
\`sendHeartbeat()\` always sets \`pkt.nodeType = 0\` even when \`droneMode == true\`. Nest UI shows W vs D based on \`nodeType\`; drones always appear as workers.

## Files
- \`stage15/worker/worker_espnow.cpp:29\`
- \`stage15/nest/nest_display.cpp\`

## Fix
\`pkt.nodeType = droneMode ? 1 : 0;\`

## Audit
Pre-release audit #4."

create_issue \
  "[High] GPS UART not serviced during WiFi scan" \
  "bug" \
  "## Problem
\`WiFi.scanNetworks()\` blocks for extended periods. Unlike BLE scan loop, no \`gpsSerial\` draining during WiFi scan. NMEA data can be lost from 512-byte RX buffer.

## Files
- \`stage15/worker/worker_scan.cpp\`

## Fix
Use async WiFi scan with polling loop that feeds GPS and calls \`maybeHeartbeat()\`, or interleave \`feedGPS()\` during scan.

## Audit
Pre-release audit #5."

create_issue \
  "[High] Unbounded BLE result accumulation" \
  "bug" \
  "## Problem
\`setMaxResults(0)\` disables NimBLE cap. \`onDiscovered()\` pushes into five \`std::vector\`s with no upper bound. Dense RF environments can exhaust RAM.

## Files
- \`stage15/worker/worker.ino\`
- \`stage15/worker/worker_scan.cpp\`

## Fix
Cap vectors, set sane \`setMaxResults()\`, add mutex for callback vs main thread access.

## Audit
Pre-release audit #6."

create_issue \
  "[High] ESP-NOW init failure non-fatal on worker" \
  "bug" \
  "## Problem
If \`esp_now_init()\` fails, setup continues. Later \`connectToNest()\` calls \`esp_now_deinit()\` unconditionally.

## Files
- \`stage15/worker/worker_espnow.cpp\`
- \`stage15/worker/worker_sync.cpp\`

## Fix
Track \`espNowOk\` flag; only deinit if init succeeded; surface failure via LED/serial.

## Audit
Pre-release audit #7."

create_issue \
  "[High] Hardcoded default credentials and NEST_MAC" \
  "bug" \
  "## Problem
- Default Nest PSK \`waspswarm\` compiled into worker firmware
- \`NEST_MAC\` is compile-time fixed in \`worker_types.h\` — not configurable via \`worker.cfg\`
- README does not document MAC provisioning step

## Files
- \`stage15/worker/worker_types.h\`
- \`stage15/worker/worker_config.cpp\`
- \`README.md\`

## Fix
Add \`nestMac=\` to \`worker.cfg\`; remove default PSK; document first-time setup.

## Audit
Pre-release audit #8."

create_issue \
  "[Medium] GPS UTC applied via mktime (local time)" \
  "bug" \
  "## Problem
GPS NMEA time is UTC but \`mktime(&t)\` interprets \`struct tm\` as local time. Logged timestamps can disagree with system clock.

## Files
- \`stage15/worker/worker_gps.cpp:76-88\`

## Fix
Use \`timegm()\` or manual UTC epoch math before \`settimeofday()\`.

## Audit
Pre-release audit #9."

create_issue \
  "[Medium] SD failure handling ignored after sync" \
  "bug" \
  "## Problem
\`openLogFile()\` return value ignored at boot. Post-sync SD re-init does not update \`sdOk\`. Persistent SD failure causes silent logging loss.

## Files
- \`stage15/worker/worker_storage.cpp\`
- \`stage15/worker/worker_sync.cpp\`

## Fix
Check return values; refresh \`sdOk\` after re-init; LED/serial error on failure.

## Audit
Pre-release audit #10."

create_issue \
  "[Medium] Sync queue limits and .defer retry storms" \
  "bug" \
  "## Problem
- Only first 100 \`.csv\` files queued per sync pass
- Failed uploads renamed to \`.defer\` then immediately restored to \`.csv\` at end of same sync
- Drone \`buildCSV()\` + \`http.POST()\` double-allocates large strings

## Files
- \`stage15/worker/worker_sync.cpp\`
- \`stage15/worker/worker_drone.cpp\`

## Fix
Paginate queue with warning; exponential backoff for \`.defer\`; stream POST or reuse TCP chunked protocol.

## Audit
Pre-release audit #11."

create_issue \
  "[Medium] Silent data loss when GPS fix absent" \
  "bug" \
  "## Problem
WiFi/BLE rows only written when \`gps.location.isValid()\`. Indoor/cold-start produces zero logged data. Drone buffer silently drops overflow entries.

## Files
- \`stage15/worker/worker_scan.cpp\`
- \`stage15/worker/worker_drone.cpp\`

## Fix
Log with fallback timestamp/coords; warn on buffer overflow and unuploaded slot overwrite.

## Audit
Pre-release audit #12."

create_issue \
  "[Medium] LED config parse lacks bounds validation" \
  "bug" \
  "## Problem
\`parseLedEvent()\` / \`parseNestLedEvent()\` accept malformed values. Negative \`onMs\`/\`offMs\` become huge unsigned \`delay()\` values.

## Files
- \`stage15/worker/worker_config.cpp\`
- \`stage15/nest/nest_config.cpp\`

## Fix
Validate ranges; clamp flashes and timing to sane maxima.

## Audit
Pre-release audit #13."

create_issue \
  "[Medium] WiGLE upload rejects HTTP 201" \
  "bug" \
  "## Problem
\`streamMultipartPost()\` accepts 200/201 but \`uploadFileToWigle()\` only returns true for \`code == 200\`.

## Files
- \`stage15/nest/nest_home.cpp:163,191-199\`

## Fix
\`return (code == 200 || code == 201);\`

## Audit
Pre-release audit #14."

create_issue \
  "[Medium] homeStatus=2 shown as Connecting instead of failure" \
  "bug" \
  "## Problem
\`homeStatus\` set to 2 on WiFi/upload failure but Settings draws \"Connecting...\" for status 2.

## Files
- \`stage15/nest/nest_display.cpp:471-474\`
- \`stage15/nest/nest_home.cpp\`

## Fix
Map status 2 to \"Failed\" or \"Last upload failed\".

## Audit
Pre-release audit #15."

create_issue \
  "[Medium] Worker registry silently drops when full" \
  "bug" \
  "## Problem
\`findOrAddWorker()\` returns -1 when all 8 slots occupied. ESP-NOW packets dropped with no UI/serial warning.

## Files
- \`stage15/nest/nest_registry.cpp\`

## Fix
Evict oldest stale entry on overflow; log/UI alert when registry full.

## Audit
Pre-release audit #16."

create_issue \
  "[Medium] Touch/hardware documentation contradictions" \
  "documentation" \
  "## Problem
- \`nest.ino\` header documents SPI resistive touch; code uses CST820 I2C
- \`HARDWARE_JC2432W328C.md\` IRQ pin differs from README
- README LED flash tables don't match compiled defaults

## Files
- \`stage15/nest/nest.ino\`
- \`HARDWARE_JC2432W328C.md\`
- \`README.md\`

## Fix
Align all docs with \`nest_touch.cpp\`, \`nest_types.h\`, and \`nest_led.cpp\`/\`worker_led.cpp\` defaults.

## Audit
Pre-release audit #17."

create_issue \
  "[Medium] ESP-NOW swarm/firmware filter missing" \
  "bug" \
  "## Problem
Packet structs include \`swarmId\` and \`firmwareVer\` but nest accepts all packets with no validation. Foreign swarms pollute registry.

## Files
- \`stage15/nest/nest_espnow.cpp\`
- \`stage15/nest/nest_types.h\`

## Fix
Filter on configured \`swarmId\`; warn on firmware mismatch.

## Audit
Pre-release audit #18."

create_issue \
  "[Medium] Chunked upload lacks sequence validation" \
  "bug" \
  "## Problem
No validation that chunks arrive in order or that \`0 <= chunkIndex < totalChunks\`.

## Files
- \`stage15/nest/nest_upload.cpp\`

## Fix
Track per-file upload state; reject out-of-order chunks.

## Audit
Pre-release audit #19."

create_issue \
  "[Medium] File delete path lacks filename validation" \
  "bug" \
  "## Problem
File browser delete uses SD filenames without \`isValidFilename()\` check.

## Files
- \`stage15/nest/nest_display.cpp:368-381,587-594\`

## Fix
Reuse \`isValidFilename()\` before delete; reject \`..\` and \`/\`.

## Audit
Pre-release audit #20."

create_issue \
  "[Medium] CSV merge assumes two header lines" \
  "bug" \
  "## Problem
\`buildMergedCsv()\` skips two header lines per subsequent file. Breaks on non-standard CSV preamble.

## Files
- \`stage15/nest/nest_home.cpp:210-256\`

## Fix
Detect Wigle header line explicitly.

## Audit
Pre-release audit #21."

create_issue \
  "[Medium] Heavy Serial logging in ESP-NOW callback" \
  "bug" \
  "## Problem
\`onDataRecv\` runs in WiFi stack context, holds \`gLock\`, prints multi-line logs. Can worsen ESP-NOW timing.

## Files
- \`stage15/nest/nest_espnow.cpp\`

## Fix
Defer logging to loop task; minimal work in callback.

## Audit
Pre-release audit #22."

create_issue \
  "[Medium] Home upload blocks main loop" \
  "bug" \
  "## Problem
\`runHomeUploads()\` runs synchronously in \`loop()\` with 15s WiFi timeout, TLS upload, LED delays. Touch/display freeze during upload.

## Files
- \`stage15/nest/nest_home.cpp\`
- \`stage15/nest/nest.ino\`

## Fix
Run home uploads in dedicated FreeRTOS task with busy flag.

## Audit
Pre-release audit #23."

create_issue \
  "[Medium] Port 80 HTTP starved during 8080 transfers" \
  "bug" \
  "## Problem
\`uploadTask\` blocks on full 8080 chunk before returning to \`server.handleClient()\`. Drone HTTP uploads on port 80 starve.

## Files
- \`stage15/nest/nest.ino\`
- \`stage15/nest/nest_upload.cpp\`

## Fix
Non-blocking 8080 state machine or call \`server.handleClient()\` inside read loop.

## Audit
Pre-release audit #24."

create_issue \
  "[Medium] Status strings read outside critical section" \
  "bug" \
  "## Problem
\`lastSyncStr\`, \`lastWigleStr\`, \`lastWdgStr\` written under \`gLock\` but read without lock in \`drawSettings()\`.

## Files
- \`stage15/nest/nest_display.cpp\`

## Fix
Snapshot under \`gLock\` in all readers.

## Audit
Pre-release audit #25."

create_issue \
  "[Medium] TLS certificate validation disabled" \
  "bug" \
  "## Problem
\`WiFiClientSecure::setInsecure()\` disables server auth for WiGLE/WDGWars uploads.

## Files
- \`stage15/nest/nest_home.cpp:97-98\`

## Fix
Use \`setCACert()\` with embedded CA bundle for known hosts.

## Audit
Pre-release audit #26."

create_issue \
  "[Medium] UI navigation stack silent overflow" \
  "bug" \
  "## Problem
\`uiPush()\` drops pushes when \`uiStackDepth >= UI_STACK_MAX\` with no warning.

## Files
- \`stage15/nest/nest_ui.cpp\`

## Fix
Replace top on lateral navigation or log on overflow.

## Audit
Pre-release audit #27."

create_issue \
  "[Medium] restoreNestAP fragile ESP-NOW re-init" \
  "bug" \
  "## Problem
After home upload, \`esp_now_deinit()\` + re-init with no retry on failure. Workers lose link until reboot.

## Files
- \`stage15/nest/nest_home.cpp:20-28\`

## Fix
Retry init with backoff; surface failure on TFT.

## Audit
Pre-release audit #28."

create_issue \
  "[High] No CI/CD compile verification" \
  "enhancement" \
  "## Problem
No \`.github/workflows/\` for Arduino compile verification. No automated signal that firmware still builds.

## Fix
Add \`ci.yml\` using \`arduino-cli\` to compile nest (ESP32 Dev Module) and worker (XIAO_ESP32C5) with documented libraries.

## Audit
Pre-release audit #29."

create_issue \
  "[High] README LED/config defaults mismatch" \
  "documentation" \
  "## Problem
README config tables and LED flash codes differ from compiled defaults in \`nest_led.cpp\` / \`worker_led.cpp\` and example configs.

## Fix
Regenerate README tables from code and \`wasp.cfg.example\` / \`worker.cfg.example\`.

## Audit
Pre-release audit #30."

create_issue \
  "[Medium] Firmware version mismatch nest vs worker" \
  "bug" \
  "## Problem
Worker \`WASP_FIRMWARE_VER\` is 15; nest defines 10. Heartbeats carry mismatched versions.

## Files
- \`stage15/worker/worker_types.h\`
- \`stage15/nest/nest_types.h\`

## Fix
Unify version constants in shared header.

## Audit
Pre-release audit #31."

create_issue \
  "[Medium] Stage 15/16 naming confusion" \
  "documentation" \
  "## Problem
README marks Stage 16 complete but flash target is \`stage15/\`. Touch UI lives inside stage15 tree.

## Fix
Relabel README build table or rename folder to \`stage16/\`.

## Audit
Pre-release audit #32."

create_issue \
  "[Medium] Missing Nest touch UI user guide" \
  "documentation" \
  "## Problem
Full touch UI (F/S buttons, worker detail, file browser, settings) exists but README has no user guide.

## Fix
Add Nest UI section with screen map and gestures.

## Audit
Pre-release audit #33."

create_issue \
  "[Medium] .gitignore incomplete" \
  "enhancement" \
  "## Problem
Missing \`worker.cfg\`, build artifacts, OS cruft.

## Fix
Extend \`.gitignore\` with \`worker.cfg\`, \`**/wasp.cfg\`, Arduino build outputs.

## Audit
Pre-release audit #34."

create_issue \
  "[Medium] Missing LICENSE and security policy" \
  "documentation" \
  "## Problem
No LICENSE, CONTRIBUTING.md, or SECURITY.md found.

## Fix
Add LICENSE and SECURITY.md with disclosure process and default-PSK warning.

## Audit
Pre-release audit #35."

create_issue \
  "[Low] Non-blocking LED flash uses blocking delay" \
  "enhancement" \
  "## Problem
\`nestLedFlash()\` uses \`delay()\` in main loop, stalling touch during flashes.

## Files
- \`stage15/nest/nest_led.cpp\`

## Fix
Non-blocking LED state machine.

## Audit
Pre-release audit #36."

create_issue \
  "[Low] Worker registry timeout UX mismatch" \
  "enhancement" \
  "## Problem
Status bar counts workers within 30s but list shows up to 90s.

## Files
- \`stage15/nest/nest_types.h\`
- \`stage15/nest/nest_display.cpp\`

## Fix
Align or document timeouts.

## Audit
Pre-release audit #37."

create_issue \
  "[Low] stage14_last_monolith undocumented archive" \
  "documentation" \
  "## Problem
Archive folder not in README layout; headers say Stage 13; lacks touch UI.

## Fix
Add ARCHIVE.md or README subsection.

## Audit
Pre-release audit #38."

create_issue \
  "[Low] Miscellaneous code hygiene items" \
  "enhancement" \
  "## Problem
- BLEScanCallbacks \`new\` never freed
- \`cycleCount\` off-by-one in ESP-NOW summary
- \`maxLogBytes\` dual role (rotation + chunk size)
- Boot log 9/9 then 10/10 inconsistency
- \`sendSummary()\` return values ignored
- Drone CSV uses sync-time not \`capturedMs\`

## Fix
Address individually or batch in maintenance PR.

## Audit
Pre-release audit #39."

echo "Done."
