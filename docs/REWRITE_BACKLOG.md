# ESP32 firmware COM rewrite — backlog

Shared IDs with `openwrt-packages/feeds/packages/mcudd/docs/backlog.md`.

**Legend:** `[ ]` todo · `[~]` in progress · `[x]` done

---

## R0 — Contract

| ID | Task | Status |
|----|------|--------|
| R0-1 | Golden RDCP traces | [x] |
| R0-2 | Host backlog tracker | [x] |
| R0-3 | Firmware backlog tracker | [x] |
| R0-4 | Fixture sync script | [x] |

---

## R2 — Firmware

| ID | Task | Status |
|----|------|--------|
| R2-1 | `src/proto` host-testable C parse/build | [x] |
| R2-2 | `src/app` swipe emits `evt screen` only (no `evt input`) | [x] |
| R2-3 | Host gcc tests 100% of proto+app | [x] |
| R2-4 | Rebind LVGL / UART2 / BOOT | [x] |
| R2-5 | Remove txbeacon env | [x] |

---

## R3 — Hardware

| ID | Task | Status |
|----|------|--------|
| R3-1 | Flash `esp32-2432S022C-router` | [x] |
| R3-2 | Deploy aarch64 `mcudd` to CM5 | [x] |
| R3-3 | USB COM: ping + echo + screen + version | [x] |
| R3-4 | Silent RDCP UART (no Arduino logs) | [x] |
| R3-5 | `mcud-link-test.sh` on ttyS2 after USB unplug | [ ] |

---

## Changelog

| Date | Note |
|------|------|
| 2026-08-29 | proto+app rewrite; host tests 100%; UART2/LVGL rebound |
