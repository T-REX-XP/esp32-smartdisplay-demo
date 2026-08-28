# ESP32 MCU display firmware (CM5 / ImmortalWrt)

LVGL firmware for a **Sunton ESP32-2432S022C** (2.2" 240×320) used as the Orange Pi **CM5** router display. The panel talks to ImmortalWrt over UART using **RDCP v1** (newline-terminated JSON). The host side is `mcudd` in [luci-app-mcu-display](https://github.com/T-REX-XP/openwrt-packages) (**Services → MCU Display**).

This tree is a fork of [rzeldent/esp32-smartdisplay-demo](https://github.com/rzeldent/esp32-smartdisplay-demo). The **production** build is `esp32-2432S022C-router`. The default PlatformIO env (`esp32-2432S022C`) is the upstream EEZ demo (Clock / Weather / Alarm) and is **not** what the CM5 image expects.

> Clone with submodules: `git clone --recurse-submodules`. Required for the [Sunton board definitions](https://github.com/rzeldent/platformio-espressif32-sunton).

## Stack

| Side | Component | Role |
|------|-----------|------|
| ESP32 | this firmware (`ROUTER_UI=1`) | LVGL screens, swipe, RDCP JSON |
| CM5 | `mcudd` | UART bridge, metrics, boot/alerts, FIFO from buttons/LuCI |
| CM5 | `/etc/mcud/pages.json` | Screen IDs and scopes — keep in sync with `src/router/router_pages.c` |
| Version | `mcud-version.json` | Shared with the host package (`stack` / `release` / `rdcp`) |

Current version (from `mcud-version.json`): **stack 1.0.0**, **release 32**, **RDCP 1**.

Backlog: [ROUTER_TODO.md](ROUTER_TODO.md).

```text
CM5 ttyS2  <── 115200 8N1 ──>  ESP32 UART2 (GPIO3 RX / GPIO1 TX)
mcudd                          router_app + LVGL
LuCI / USERKEY / MaskROM
```

## Hardware

**Board:** ESP32-2432S022C (USB-C, ST7789 i80, CST816S touch). GPIO16/17 are the LCD DC/CS lines, so RDCP does **not** use the usual UART2 pins.

**Serial (router env):** UART2 remapped to the P1 JST “Power + Serial” header — **GPIO3 RX**, **GPIO1 TX**, 115200 8N1. Those pins are shared with USB-C serial: use **either** USB-C (bench) **or** the JST wired to the CM5, not both TX drivers at once.

| CM5 J3 debug pin | Signal | ESP32 (2432S022C) |
|------------------|--------|-------------------|
| 1 | GND | GND |
| 2 | RX (GPIO0_B6) | GPIO1 TX |
| 3 | TX (GPIO0_B5) | GPIO3 RX |

3.3 V logic only. Cross TX↔RX. The CM5 bootscript leaves `/dev/ttyS2` free for `mcudd` (no runtime kernel console on that port).

**Buttons (2432S022 — not the same key):**

| Silkscreen | Wired to | Firmware |
|------------|----------|----------|
| **BOOT** | ESP32 **GPIO0** | Short tap → next page; long hold → power-off countdown |
| **SW1** | IP5306 **KEY** (battery/power IC) | **Not readable** by ESP32 — toggles boost/charge LEDs only |
| **RST** | EN reset | Hardware reset only |

Navigation in firmware uses **BOOT (GPIO0)**, not SW1. Use **touch swipe** or wire an external button to a free GPIO (e.g. 35) and set `-D ROUTER_BTN_SW1_GPIO=35` in `platformio.ini`.

Schematics: `boards/assets/schematics/ESP32-2432S022-{MCU,LCM}-V1.0.png`.

## Build and flash

[PlatformIO](https://platformio.org/) (CLI or VS Code / Cursor).

```bash
# Production firmware for CM5
pio run -e esp32-2432S022C-router
pio run -e esp32-2432S022C-router -t upload
pio device monitor -e esp32-2432S022C-router   # USB-C only; disconnect JST to CM5
```

`scripts/gen_mcud_version.py` runs as a pre-script and writes `src/router/mcud_version.h` from `mcud-version.json`. Do not edit the header by hand. Keep the JSON in lockstep with `openwrt-packages/feeds/luci/luci-app-mcu-display/mcud-version.json`.

**Upstream demo UI** (not for the router):

```bash
pio run -e esp32-2432S022C -t upload
```

## Screens

Swipe left/right (or host `cmd` frames) cycles pages. IDs must match `/etc/mcud/pages.json`.

| Screen ID | Scope | Content |
|-----------|-------|---------|
| `router_boot` | — | Splash + boot progress (`push` `op=boot`) |
| `router_system` | `system` | Hostname, CPU %, RAM bar + used, load, temp, uptime |
| `router_network` | `network` | WAN IP, RX/TX, ping |
| `router_clients` | `clients` | Wi-Fi / LAN / DHCP counts |
| `router_storage` | `storage` | Root / data usage |
| `router_wifi` | `wifi` | SSID, AP state, join QR |
| `router_security` | `security` | Firewall, blocked, VPN |

On boot the MCU emits `evt` `screen` (`router_boot`) and `evt` `version`. Until the host sends a frame, swipe navigates locally. After the first valid host line (`hello`, `res`, `cmd`, …) the MCU is **host-linked**: swipe sends `evt` `input` and waits for `mcudd` to reply with `cmd` `screen` / `nav`. Linked pages poll `req` `metrics` every 1.5 s (system) or 2 s (others).

## Protocol (RDCP v1)

Line-delimited JSON (`\n`), max 4096 bytes. Firmware parses **JSON only** (MessagePack / CBOR is not implemented). Set `wire_format=json` in `/etc/config/mcud`.

Details: [docs/rdcp-v1.md](docs/rdcp-v1.md). Host design: `openwrt-packages/docs/luci-app-mcu-display-system-design.md`.

**MCU → host**

```json
{"v":1,"t":"req","id":1,"op":"metrics","scope":"system"}
{"v":1,"t":"evt","op":"screen","data":{"screen":"router_system","action":"loaded"}}
{"v":1,"t":"evt","op":"input","data":{"type":"gesture","dir":"left"}}
{"v":1,"t":"evt","op":"version","data":{"stack":"1.0.0","release":31,"component":"esp32-router","rdcp":1}}
```

**Host → MCU**

```json
{"v":1,"t":"push","op":"hello","data":{"stack":"1.0.0","release":31,"component":"mcudd","rdcp":1}}
{"v":1,"t":"res","id":1,"data":{"hostname":"cm5","cpu":"12","ram_pct":40}}
{"v":1,"t":"cmd","op":"screen","data":{"screen":"router_wifi","dir":"left"}}
{"v":1,"t":"cmd","op":"nav","data":{"dir":"next"}}
{"v":1,"t":"push","op":"boot","data":{"text":"Network…","pct":60,"screen":"router_boot"}}
```

`req` `op=version` from the host is answered with another `evt` `version`. Bare metric objects without `"t"` are still accepted (legacy).

## Layout

```text
src/main.cpp                 # smartdisplay_init; ROUTER_UI vs demo UI
src/router/                  # production UI + RDCP
  router_app.cpp             # frames, swipe, polling
  router_ui.c                # LVGL pages
  router_data.c              # metrics JSON merge
  router_pages.c             # screen IDs / scopes
  rdcp_transport.cpp         # UART2 (GPIO3/1) or USB Serial
  mcud_version.h             # generated
src/ui/                      # EEZ Studio demo (excluded from -router)
mcud-version.json            # stack / release / rdcp
platformio.ini               # env:esp32-2432S022C-router
docs/rdcp-v1.md
```

## Tests and bench

```bash
./run_tests.sh                          # C parser + Python protocol tests
python esp32_simulator.py /dev/ttyUSB0 115200 --format json
```

Use `--format json` until the firmware parses MessagePack. The Flask web UI (`README_WEBUI.md`) targets the **demo** Clock/Weather screens, not the router pages.

On the router, after flash:

```sh
logread -e mcudd | tail
cat /tmp/mcud_firmware_version.json     # filled after version handshake
```

LuCI **Services → MCU Display** should show a matching stack/release. USERKEY = next page, MaskROM = previous (`cm5-button-scripts`).

## Related

| Repo / doc | Role |
|------------|------|
| `openwrt-packages/feeds/luci/luci-app-mcu-display` | `mcudd`, LuCI, `pages.json` |
| `openwrt-packages/docs/mcu-display-migration-backlog.md` | CM5 backlog and UART wiring |
| `openwrt-packages/feeds/packages/cm5-button-scripts` | GPIO → mcudd FIFO |
| [esp32-smartdisplay](https://github.com/rzeldent/esp32-smartdisplay) | Display driver library |
