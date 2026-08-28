# RDCP v1 — Router Display Communication Protocol

Line-delimited JSON (`0x0A`) between ESP32 firmware (`esp32-2432S022C-router`) and OpenWrt `mcudd`. Full host design: `openwrt-packages/docs/luci-app-mcu-display-system-design.md`.

**Implemented on the MCU:** JSON only, both directions. MessagePack / CBOR is not parsed. Set `option wire_format 'json'` in `/etc/config/mcud`.

## Frames

Every RDCP frame has `"v":1` and `"t"`. Max line length: 4096 bytes.

| `t` | Direction | Purpose |
|-----|-----------|---------|
| `req` | MCU → host | Pull metrics (`op=metrics`, `scope=…`) |
| `req` | Host → MCU | `op=version` — MCU replies with `evt` `version` |
| `res` | Host → MCU | Metrics payload in `data` (same `id` as `req`) |
| `push` | Host → MCU | Unsolicited: `hello`, `boot`, `alert` |
| `evt` | MCU → host | `screen` loaded, `input` gesture, `version` |
| `cmd` | Host → MCU | `screen` (goto id) or `nav` (`next` / `prev` / `left` / `right`) |

Legacy lines without `"t"` (flat metric objects) are still applied as metrics.

## MCU → host (examples)

```json
{"v":1,"t":"req","id":1,"op":"metrics","scope":"system"}
{"v":1,"t":"evt","op":"screen","data":{"screen":"router_system","action":"loaded"}}
{"v":1,"t":"evt","op":"input","data":{"type":"gesture","dir":"left"}}
{"v":1,"t":"evt","op":"version","data":{"stack":"1.0.0","release":31,"component":"esp32-router","rdcp":1}}
```

Scopes: `system`, `network`, `clients`, `storage`, `wifi`, `security` — same order as `/etc/mcud/pages.json` and `src/router/router_pages.c`.

Screen IDs: `router_boot`, `router_system`, `router_network`, `router_clients`, `router_storage`, `router_wifi`, `router_security`.

## Host → MCU (examples)

```json
{"v":1,"t":"push","op":"hello","data":{"stack":"1.0.0","release":31,"component":"mcudd","rdcp":1}}
{"v":1,"t":"req","id":1,"op":"version"}
{"v":1,"t":"res","id":1,"data":{"hostname":"cm5","cpu":"12","ram_pct":40,"wan_ip":"10.0.0.1"}}
{"v":1,"t":"res","id":2,"data":{"wifi_ssid":"ImmortalCM5","wifi_enc":"WPA2","wifi_ap_state":"up","wifi_qr":"WIFI:T:WPA;S:ImmortalCM5;P:secret;;"}}
{"v":1,"t":"cmd","op":"screen","data":{"screen":"router_wifi","dir":"left"}}
{"v":1,"t":"cmd","op":"nav","data":{"dir":"next"}}
{"v":1,"t":"push","op":"boot","data":{"text":"Network…","pct":60,"screen":"router_boot"}}
{"v":1,"t":"push","op":"alert","data":{"text":"WAN down","screen":"router_network"}}
```

`push` `hello` and `req` `version` both trigger an MCU `evt` `version` and mark the link as live.

## Host-link behaviour (firmware)

1. Boot: show `router_boot`, emit `evt` `screen` + `evt` `version`.
2. **Standalone** (no host frame yet): swipe changes page locally.
3. **Linked** (first valid host JSON): swipe emits `evt` `input` and waits for `cmd`; pages poll `req` `metrics`.

## Transport

| Build | Port | Pins (2432S022C) |
|-------|------|------------------|
| `esp32-2432S022C-router` (`RDCP_TRANSPORT_UART2`) | UART2 @ 115200 8N1 | RX=GPIO3, TX=GPIO1 (P1 JST; GPIO16/17 are LCD) |

CM5: `/dev/ttyS2` on the 3-pin debug header. See the [firmware README](../README.md#hardware).

## Dev testing

```bash
python esp32_simulator.py /dev/ttyUSB0 115200 --format json
./run_tests.sh
```

Use `--format json` until the firmware parses MessagePack.

## Host legacy (mcudd still accepts)

Unmodified demo firmware may send:

- `{"request":"cpu"}` → system metrics
- `{"request":"storage"}` → root filesystem stats
- `{"request":"alarms"}` → empty unless `demo_mode=1`

The router UI does not emit these; it uses `req` `metrics` with a `scope`.
