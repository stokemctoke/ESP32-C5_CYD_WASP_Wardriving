# Archive — `stage14_last_monolith/`

This directory preserves the **last monolithic firmware** from before the Stage 15 modular refactor. It is kept in the repository for historical reference and code archaeology only.

## Do not flash

**Flash `stage15/` instead.** See the [Getting Started](README.md#getting-started) section in `README.md`.

| | `stage14_last_monolith/` | `stage15/` (active) |
|---|---|---|
| Purpose | Archived reference | Production firmware |
| Structure | Single `.ino` per role | Split into modules (`.h`/`.cpp`) |
| Nest touch UI | **No** — display-only, no CST820 driver or menu stack | **Yes** — `nest_touch`, `nest_ui`, file browser, settings |
| Flash target | **Do not use** | `stage15/nest/nest.ino`, `stage15/worker/worker.ino` |

## Contents

```
stage14_last_monolith/
├── nest/
│   ├── nest.ino        ← monolithic CYD firmware (Stage 13/14 era)
│   └── nest_types.h
└── worker/
    └── worker.ino      ← monolithic C5 firmware (Stage 13 era)
```

The nest sketch predates the capacitive CST820 touch driver and stack-based UI added in Stage 16 (which ships under `stage15/nest/`). The worker sketch predates the nine-file module split in `stage15/worker/`.

## When to look here

- Comparing behaviour before and after the modular refactor
- Tracing where a feature lived before it was extracted into its own module
- Understanding the monolith that Stage 15 replaced

For all builds, configuration, and flashing instructions, use `stage15/` and the main `README.md`.
