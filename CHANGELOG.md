# ESP32 Smart Display Demo — Changelog

## 2026-07-06

### Added

- `docs/rdcp-v1.md` — protocol summary and link to OpenWrt system design.
- `tests/test_simulator_protocol.py` — unit tests for JSON/MessagePack serial encoding.
- `run_tests.py` — host test runner.

### Fixed

- `esp32_simulator.py` — implement missing `send_msgpack()` (was called by `send_data()` when `--format msgpack`).
