# W.A.S.P. Pre-Release Audit

**Date:** 2026-07-04  
**Scope:** `stage15/` (active firmware), repo infrastructure, documentation  
**Branch:** `cursor/pre-release-audit-f920`

## Summary

| Severity | Count |
|----------|-------|
| Critical | 3 |
| High | 10 |
| Medium | 18 |
| Low | 8 |
| **Total distinct issues** | **39** |

## Issue Index

Each row maps to a GitHub Issue. Overlapping findings are consolidated into one issue per fix scope.

| # | Title | Severity | Component |
|---|-------|----------|-----------|
| 1 | SD card accessed concurrently without mutex | Critical | nest |
| 2 | Upload endpoints lack authentication | Critical | nest/worker |
| 3 | HTTP /upload loads entire body into RAM | Critical | nest |
| 4 | Drone mode sends nodeType=0 in heartbeats | High | worker |
| 5 | GPS UART not serviced during WiFi scan | High | worker |
| 6 | Unbounded BLE result accumulation | High | worker |
| 7 | ESP-NOW init failure non-fatal on worker | High | worker |
| 8 | Hardcoded default credentials and NEST_MAC | High | worker/nest/docs |
| 9 | GPS UTC applied via mktime (local time) | Medium | worker |
| 10 | SD failure handling ignored after sync | Medium | worker |
| 11 | Sync queue limits and .defer retry storms | Medium | worker |
| 12 | Silent data loss when GPS fix absent | Medium | worker |
| 13 | LED config parse lacks bounds validation | Medium | worker/nest |
| 14 | WiGLE upload rejects HTTP 201 | Medium | nest |
| 15 | homeStatus=2 shown as Connecting | Medium | nest |
| 16 | Worker registry silently drops when full | Medium | nest |
| 17 | Touch/hardware documentation contradictions | Medium | docs |
| 18 | ESP-NOW swarm/firmware filter missing | Medium | nest |
| 19 | Chunked upload lacks sequence validation | Medium | nest |
| 20 | File delete path lacks filename validation | Medium | nest |
| 21 | CSV merge assumes two header lines | Medium | nest |
| 22 | Heavy Serial logging in ESP-NOW callback | Medium | nest |
| 23 | Home upload blocks main loop | Medium | nest |
| 24 | Port 80 HTTP starved during 8080 transfers | Medium | nest |
| 25 | Status strings read outside critical section | Medium | nest |
| 26 | TLS certificate validation disabled | Medium | nest |
| 27 | UI navigation stack silent overflow | Medium | nest |
| 28 | restoreNestAP fragile ESP-NOW re-init | Medium | nest |
| 29 | No CI/CD compile verification | High | infra |
| 30 | README LED/config defaults mismatch | High | docs |
| 31 | Firmware version mismatch nest vs worker | Medium | nest/worker |
| 32 | Stage 15/16 naming confusion | Medium | docs |
| 33 | Missing Nest touch UI user guide | Medium | docs |
| 34 | .gitignore incomplete | Medium | infra |
| 35 | Missing LICENSE and security policy | Medium | infra |
| 36 | Non-blocking LED flash (blocking delay) | Low | nest |
| 37 | Worker registry timeout UX mismatch | Low | nest |
| 38 | stage14_last_monolith undocumented archive | Low | docs |
| 39 | Miscellaneous low-priority code hygiene | Low | worker/nest |

## Recommended Fix Order

1. **Blockers:** #1, #2, #3, #6, #7
2. **Pre-release:** #4, #5, #8, #14, #15, #29, #30
3. **Next sprint:** #9–#13, #16–#28, #31–#35
