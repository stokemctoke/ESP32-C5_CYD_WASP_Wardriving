# Pre-Release Merge Plan

Integration branch: `cursor/pre-release-integration-f920`

## Batch 1 — Repo metadata (no firmware overlap)
| PR | Issue | Files |
|----|-------|-------|
| #44 | Audit report | PRE_RELEASE_AUDIT.md, scripts/ |
| #55 | .gitignore | .gitignore |
| #79 | LICENSE | LICENSE, SECURITY.md |
| #78 | Archive doc | ARCHIVE.md |
| #60 | CI | .github/workflows/ci.yml |

## Batch 2 — Docs-only README (merge together)
| PR | Issue | Note |
|----|-------|------|
| #57 | README LEDs | |
| #74 | Stage naming | |
| #77 | UI guide | |
| #67 | Touch hardware | HARDWARE_JC2432W328C.md, nest.ino header |

## Batch 3 — Nest types/config foundation
| PR | Issue | Files |
|----|-------|-------|
| #53 | FW version | nest_types.h |
| #56 | LED bounds | nest_config.cpp |
| #66 | Swarm filter | nest_config.cpp, nest_types.h, nest_espnow.cpp |

## Batch 4 — Nest upload pipeline (sequential)
| PR | Issue | Depends on |
|----|-------|------------|
| #58 | Upload auth | Batch 3 |
| #49 | HTTP stream | #58 |
| #62 | Chunk seq | #58, #49 |
| #76 | Port 80 | #49, #62 |
| #50 | SD mutex | upload + home + display |

## Batch 5 — Nest home/display
| PR | Issue |
|----|-------|
| #47 | WiGLE 201 |
| #72 | CSV merge |
| #81 | TLS GTS (includes #80) |
| #69 | restoreNestAP |
| #73 | Home upload task |
| #61 | Registry overflow |
| #70 | ESP-NOW log defer |
| #82 | LED non-blocking |
| #46 | homeStatus UI |
| #54 | Delete validate |
| #71 | Status lock |
| #83 | Timeout UX |

## Batch 6 — Worker (independent modules first)
| PR | Issue |
|----|-------|
| #63 | GPS UTC |
| #48 | BLE cap |
| #52 | nodeType |
| #45 | ESP-NOW init |
| #59 | nestMac config |
| #51 | GPS+WiFi scan |
| #64 | SD handling |
| #65 | Sync defer |
| #68 | GPS logging |
| #84 | Hygiene |

## Conflict hotspots (resolved in integration)

| Files | PRs merged | Resolution |
|-------|------------|------------|
| `nest_upload.cpp` | #6+#7+#23+#28+#5 | Auth + streaming + chunk seq + port80 + SD mutex combined |
| `nest_home.cpp` | #5+#18+#25+#30+#32+#27 | SD mutex + TLS GTS + restoreNestAP retry + home task |
| `nest_espnow.cpp` | #22+#26 | Swarm filter + deferred logging |
| `worker_config.*` | #6+#12 | uploadToken + nestMac |
| `nest.ino` | #43+#7+#6 | Boot 10/10 + streaming upload headers |
| `worker_espnow.cpp` | #8+#11+#12+#43 | nodeType + espNowOk + nestMac + send error check |

Integration branch: `cursor/pre-release-integration-f920` — all 39 issues merged.
