# W.A.S.P. Pre-Release Handover

**For:** Next Cursor agent (cloud or local environment)  
**Date:** 2026-07-04  
**Repo:** [stokemctoke/ESP32-C5_CYD_WASP_Wardriving](https://github.com/stokemctoke/ESP32-C5_CYD_WASP_Wardriving)  
**Active firmware:** `stage15/` (Nest = CYD ESP32, Worker = XIAO ESP32-C5)

---

## Start here

Read this file first, then:

1. `PRE_RELEASE_AUDIT.md` — full findings list (39 issues)
2. `MERGE_PLAN.md` — how fixes were batched and conflicts resolved
3. **Integration PR #85** — the single PR to review and merge

**Primary task for next agent:** Review, verify CI, merge **PR #85**, then close superseded draft PRs #45–#84.

---

## What was done

A pre-release audit was run on the entire repo. Results:

| Step | Status | Link |
|------|--------|------|
| Audit report committed | Done | `PRE_RELEASE_AUDIT.md` |
| 39 GitHub issues filed | Done | Issues [#5](https://github.com/stokemctoke/ESP32-C5_CYD_WASP_Wardriving/issues/5)–[#43](https://github.com/stokemctoke/ESP32-C5_CYD_WASP_Wardriving/issues/43) |
| 40 individual fix PRs opened | Done | PRs [#45](https://github.com/stokemctoke/ESP32-C5_CYD_WASP_Wardriving/pull/45)–[#84](https://github.com/stokemctoke/ESP32-C5_CYD_WASP_Wardriving/pull/84) |
| All fixes merged into one branch | Done | `cursor/pre-release-integration-f920` |
| Integration PR opened | **Open** | [**PR #85**](https://github.com/stokemctoke/ESP32-C5_CYD_WASP_Wardriving/pull/85) |
| Individual PRs marked superseded | Done | Banner added to each PR description |

---

## Branches

| Branch | Purpose | Action |
|--------|---------|--------|
| `master` | Production base | Merge target for #85 |
| `cursor/pre-release-audit-f920` | Audit doc + issue script | PR #44 (optional merge) |
| `cursor/pre-release-integration-f920` | **All 39 fixes combined** | PR #85 — **merge this** |
| `cursor/fix-issue-*-f920` | Individual fix branches | Close after #85 lands |

---

## Integration PR #85 — what it contains

All 39 audit fixes in one branch. Highlights:

### Critical
- **SD mutex** (`nest_sd.h`) — serialises SD access across `uploadTask` and `loop()`
- **Upload auth** — shared `uploadToken` in `wasp.cfg` / `worker.cfg`, validated on TCP :8080 and HTTP `/upload`
- **HTTP streaming** — drone POST body streams to SD (no heap `String`), 16 KB cap

### High
- Drone `nodeType` in heartbeats (`worker_espnow.cpp`)
- Async WiFi scan with GPS feed + heartbeat during scan
- BLE result cap (`MAX_BLE_PER_CYCLE = 64`)
- ESP-NOW init/deinit guard (`espNowOk`)
- Configurable `nestMac=` in `worker.cfg`
- CI workflow (`.github/workflows/ci.yml` + `ci/tft_eSPI_User_Setup.h`)
- README LED/config tables aligned with firmware

### Also included
- TLS CA bundle (ISRG Root X1 for WiGLE, GTS Root R4 for WDGWars)
- Home upload background FreeRTOS task
- Swarm ID filter, chunk sequence validation, registry eviction
- LICENSE, SECURITY.md, ARCHIVE.md, Nest UI guide, touch doc fixes
- ~25 additional medium/low fixes — see `PRE_RELEASE_AUDIT.md`

---

## Conflict resolutions (already done on integration branch)

These files had overlapping edits across multiple fix PRs. Conflicts were resolved manually:

| File | Combined features |
|------|-------------------|
| `stage15/nest/nest_upload.cpp` | Auth + streaming + chunk seq + port80 `handleClient()` + SD mutex |
| `stage15/nest/nest_home.cpp` | SD mutex + TLS + CSV merge + restoreNestAP retry + home task |
| `stage15/nest/nest_espnow.cpp` | Swarm filter + deferred callback logging |
| `stage15/worker/worker_config.cpp` | `uploadToken` + `nestMac` parsing |
| `stage15/worker/worker_espnow.cpp` | `espNowOk` + `nodeType` + `nestMac` + send error check |
| `stage15/nest/nest.ino` | Boot 10/10 log + `X-Upload-Token` header collection + streaming upload route |

Details: `MERGE_PLAN.md`

---

## Config changes operators must know

New keys added to config examples:

**`wasp.cfg` (Nest SD):**
```
uploadToken=waspswarm    # must match worker.cfg
#swarmId=0              # 0 = accept all workers
```

**`worker.cfg` (Worker SD):**
```
uploadToken=waspswarm
#nestMac=A4:F0:0F:5D:96:D4   # Nest STA MAC for ESP-NOW
```

Default token matches existing AP password (`waspswarm`) for backward compatibility. **Change before field use** — see `SECURITY.md`.

---

## Next agent tasks (priority order)

### 1. Verify CI on PR #85
```bash
git checkout cursor/pre-release-integration-f920
git pull origin cursor/pre-release-integration-f920
# Watch GitHub Actions on PR #85
```
CI compiles:
- `stage15/nest/nest.ino` → ESP32 Dev Module
- `stage15/worker/worker.ino` → XIAO ESP32-C5 (Huge APP partition)

If CI fails, fix on the integration branch and push.

### 2. Local compile (optional but recommended)
Requires Arduino CLI + ESP32 core + libraries (NimBLE-Arduino, TinyGPSPlus, Adafruit NeoPixel, TFT_eSPI). See `.github/workflows/ci.yml` for exact flags.

### 3. Review hot files
Focus review on:
- `stage15/nest/nest_upload.cpp`
- `stage15/nest/nest_home.cpp`
- `stage15/worker/worker_sync.cpp`
- `stage15/nest/nest.ino`

### 4. Merge PR #85 into `master`
After CI passes and review is satisfactory.

### 5. Close superseded PRs
Close **#45–#84** (individual fix PRs — all superseded by #85).  
**#44** (audit report) can merge separately or with #85.

### 6. Close GitHub issues
Issues #5–#43 should auto-close when #85 merges (if PR body includes `Closes #N`). Verify and close any stragglers.

### 7. Post-merge smoke test (hardware)
- Flash `stage15/nest/nest.ino` to CYD
- Flash `stage15/worker/worker.ino` to XIAO C5
- Set `nestMac` in worker.cfg to Nest STA MAC (printed at boot)
- Verify ESP-NOW heartbeat, file sync (:8080), drone HTTP upload (:80)

---

## Known limitations / not done

| Item | Notes |
|------|-------|
| PR timeline comments | Agent token could not post PR comments (403). Superseded banners were added to PR **descriptions** instead. |
| Hardware testing | No on-device validation in cloud environment |
| Individual PRs | Not intended to be merged — #85 supersedes all |
| `stage14_last_monolith/` | Archive only — do not flash. See `ARCHIVE.md` |
| Default PSK `waspswarm` | Still the compiled default — documented in SECURITY.md but not removed |

---

## Key files in repo

```
HANDOVER.md              ← you are here
PRE_RELEASE_AUDIT.md     ← findings index
MERGE_PLAN.md            ← merge batch order + conflict notes
ARCHIVE.md               ← stage14_last_monolith explanation
SECURITY.md              ← disclosure + default-PSK warning
LICENSE                  ← MIT
scripts/
  create-audit-issues.sh ← idempotent issue creation script
  supersede-pr-bodies.sh ← helper used to mark PRs superseded
.github/workflows/ci.yml ← compile verification
ci/tft_eSPI_User_Setup.h ← TFT_eSPI pins for CI
stage15/nest/            ← active Nest firmware (modular)
stage15/worker/          ← active Worker firmware (modular)
```

---

## Prompt for next agent

Copy-paste this to start a new agent in your Cursor environment:

```
Read HANDOVER.md in the repo root, then continue the pre-release work:

1. Check out branch cursor/pre-release-integration-f920
2. Verify CI passes on PR #85
3. Review the hot files listed in HANDOVER.md (nest_upload.cpp, nest_home.cpp, worker_sync.cpp, nest.ino)
4. If CI fails, fix and push to the integration branch
5. When ready, merge PR #85 into master
6. Close superseded draft PRs #45-#84
7. Confirm GitHub issues #5-#43 are closed

Do not re-open individual fix PRs — everything is in #85.
Reference PRE_RELEASE_AUDIT.md and MERGE_PLAN.md for context.
```

---

## GitHub permissions note

The Cursor GitHub App has broad read/write access (issues, PRs, code, workflows). PR **comment** API returned 403 in cloud agent context; PR **description updates** worked via the PR management tool. If you need agents to comment on PRs, verify Pull requests write scope is accepted on the GitHub App installation.

---

*Generated by pre-release audit agent session, 2026-07-04.*
