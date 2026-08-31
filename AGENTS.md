# Agent guide — esp32-smartdisplay-demo

LVGL firmware for the **Sunton ESP32-2432S022C** used as the Orange Pi **CM5** MCU display. Talks to ImmortalWrt over **RDCP v1** (newline JSON) on UART.

Production env: **`esp32-2432S022C-router`**. Default env `esp32-2432S022C` is the upstream EEZ demo — do not flash that to the CM5 panel.

## Hardware COM

```text
CM5 ttyS2  <── 115200 8N1 ──>  ESP32 UART2 (GPIO3 RX / GPIO1 TX)
mcudd                          router_app + LVGL
```

USB-C CH340 and the P1 JST header share GPIO1/3. Use **either** USB flash **or** JST to CM5, not both TX drivers.

From the **router** (stop `mcudd` first): `picocom -b 115200 /dev/ttyS2` — skill **`cm5-mcu-serial`** in openwrt-packages.

## Build and flash (macOS)

```sh
# Stop mcudd first (it owns ttyS2 / same ESP32 pins)
ssh -i ~/.ssh/id_ed25519_openwrt_mcp root@192.168.8.1 '/etc/init.d/mcudd stop'

pio device list   # expect /dev/cu.usbserial-* (CH340 VID:PID 1A86:7523)
pio run -e esp32-2432S022C-router -t upload --upload-port /dev/cu.usbserial-XXXX
```

This machine: `/Users/t-rex-xp/Library/Python/3.9/bin/pio` (or `python3 -m platformio`).

After upload: **unplug USB-C**, then start `mcudd` on the router. If `ttyS2` RX stays frozen, tap **RST** on the panel with mcudd running.

## Frozen: swipe → LuCI active page

Working path. **Do not add a gesture opcode or echo `cmd screen` on swipe.**

1. Firmware swipe → local `apply_page` + `evt screen` (the id shown)
2. Go `mcudd` writes `/tmp/mcud_active_screen` from **every** known `evt screen`
3. LuCI **Services → MCU Display** polls that sidecar

LuCI prev/next is the same wire: host `cmd screen` → MCU `apply_page` → `evt screen`.

## Do not

- Flash while `mcudd` holds `/dev/ttyS2`
- Leave USB-C plugged after flash
- Edit swipe to wait for host `cmd screen`, or emit `evt input`

## Related

| Repo | Role |
|------|------|
| `openwrt-packages` | Go `mcudd`, `luci-app-mcu-display` |
| `immortalwrt` | CM5 image; `ttyS2` free of runtime console |

## Project skills

| Skill | When to use |
|-------|-------------|
| `esp32-cm5-router-fw` | Build/flash `esp32-2432S022C-router`, USB vs JST |
| `mcu-display-cm5` | Host `mcudd` / LuCI (in `openwrt-packages/.cursor/skills/`) |
| `cm5-mcu-serial` | Router `picocom`/`socat` on `/dev/ttyS2` (in `openwrt-packages`) |
| `openwrt-mcp-ssh` | Live CM5 via MCP / SSH |
