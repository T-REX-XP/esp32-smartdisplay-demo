# ESP32 Smart Display Demo — Changelog

## 2026-07-06 (router UI)

### Added

- **Router LVGL screens** — six pages matching `pages.json`: system, network, clients, storage, WiFi (QR), security.
- **`src/router/`** — `router_ui.c`, `router_data.c`, `router_app.cpp`, RDCP scope requests + screen events.
- **PlatformIO env** — `esp32-2432S022C-router` (`ROUTER_UI=1`, demo EEZ screens excluded).
- **Host test** — `tests/test_router_data.c` JSON/RDCP payload parser.

### Changed

- Swipe left/right navigates router pages; each page polls its scope from `mcudd`.

## 2026-07-06

### Added

- `docs/rdcp-v1.md` — protocol summary and link to OpenWrt system design.
- `tests/test_simulator_protocol.py` — unit tests for JSON/MessagePack serial encoding.
- `run_tests.py` — host test runner.

### Fixed

- `esp32_simulator.py` — implement missing `send_msgpack()` (was called by `send_data()` when `--format msgpack`).
- Remove accidental `__pycache__` from version control; ignore `__pycache__/` and `*.pyc`.
