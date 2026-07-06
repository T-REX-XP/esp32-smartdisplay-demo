# RDCP v1 — Router Display Communication Protocol

See the full system design in the OpenWrt feed:

`openwrt-packages/docs/luci-app-mcu-display-system-design.md`

## Phase 1 (implemented)

- MCU → host: newline-terminated JSON (`req`, `evt`, legacy `request`)
- Host → MCU: JSON only when `wire_format=json` in `/etc/config/mcud`
- Legacy commands supported by `mcudd`:
  - `{"request":"cpu"}` → system metrics
  - `{"request":"storage"}` → root filesystem stats
  - `{"request":"alarms"}` → empty unless `demo_mode=1` in UCI

## Dev testing

```bash
python esp32_simulator.py /dev/ttyUSB0 115200 --format json
```

Use `--format json` until ESP32 firmware parses CBOR/MessagePack (Phase 2).
