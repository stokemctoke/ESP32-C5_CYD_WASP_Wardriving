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

## Conflict hotspots
- `nest_upload.cpp` — #58, #49, #62, #76, #50
- `nest_display.cpp` — #50, #46, #54, #71, #83, #73
- `nest_home.cpp` — #50, #47, #72, #81, #69, #73
- `nest.ino` — #50, #70, #82, #43, #73, #21
- `worker_sync.cpp` — #58, #49, #45, #64, #65
- `README.md` — #57, #74, #77, #42, #12
