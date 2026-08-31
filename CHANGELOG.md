# ESP32 Smart Display Demo — Changelog

## 2026-08-31 (USB-C RDCP sniff)

### Added

- **`RDCP_USB_MIRROR_RX`** on `esp32-2432S022C-router` — copies each host→MCU UART line onto GPIO1 as `#rx {…}` so a Mac USB-C monitor can see mcudd payloads. Receive-only: do not type (CH340 TX shares GPIO3 with JST). See [docs/usb-c-rdcp-sniff.md](docs/usb-c-rdcp-sniff.md).

## 2026-08-28 (firmware UI)

### Added

- **Dark theme** — `ROUTER_UI_DARK=1` on `esp32-2432S022C-router` (LuCI BootstrapDark-style surfaces). Light palette: `-D ROUTER_UI_DARK=0`.
- **Stale metrics** — after UART `LINK LOST`, last values stay on screen at 50% opacity.

### Changed

- `router_ui_refresh` updates only the visible page; switching pages reapplies cached metrics.
- Nav GPIO flag renamed to `ROUTER_BTN_BOOT_GPIO` (`ROUTER_BTN_SW1_GPIO` still accepted).

## 2026-08-28 (Wi-Fi AP screen)

### Added

- Wi-Fi page: SSID, encryption label, AP up/disabled/down, WPA QR (`wifi_qr`). Parser unescapes JSON so SSIDs with `;` / `:` still scan.

## 2026-08-28 (storage screen)

### Added

- Storage page: root usage, overlay/extroot/eMMC label, swap bar.

## 2026-08-28 (network screen)

### Added

- Network page: WAN RX/TX rates, ping, eth0 WAN / eth1+eth2 LAN link badges.

## 2026-08-28 (system screen)

### Added

- **Sparkline** — 40-sample (~1 min) CPU + RAM ring buffer on the system page.
- **LINK** — `LINK OK` while UART frames arrive; `LINK LOST` after 5 s silence; `LINK --` before the first host frame. Host `link_ok` (WAN) is ignored.

## 2026-08-28 (docs)

### Changed

- **README.md** — document production `esp32-2432S022C-router` firmware (RDCP, CM5 UART wiring, pages, version handshake) instead of the old OLED/demo JSON notes.
- **docs/rdcp-v1.md** — match implemented MCU frames (`hello`, `version`, `cmd` nav/screen, host-link behaviour).

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
