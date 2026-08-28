# Orange Pi CM5 Router Display — MCU Features & Screens

## Project overview

ESP32-2432S022C smart display firmware (`esp32-2432S022C-router`) paired with ImmortalWrt on **Orange Pi CM5 Base**. The CM5 runs `mcudd` (from [luci-app-mcu-display](https://github.com/T-REX-XP/openwrt-packages)); the panel shows live router metrics over **RDCP v1** UART (`/dev/ttyS2` @ 115200).

```text
CM5 mcudd  ←UART→  ESP32 router_app + LVGL
LuCI / BOOT / MaskROM / USERKEY
```

Keep in sync: `src/router/router_pages.c`, `/etc/mcud/pages.json`, `mcud-version.json` (host + firmware).

---

## Screens (page order)

| # | Screen ID | Scope | Status |
|---|-----------|-------|--------|
| — | `router_boot` | — | Done — splash + boot progress |
| 1 | `router_system` | `system` | **Done** — CPU% (/proc/stat), RAM, load, sparkline, link LED; temp when thermal present |
| 2 | `router_network` | `network` | **Done** — WAN IP, RX/TX, ping, eth0/1/2 link badges |
| 3 | `router_clients` | `clients` | **Done** — Wi-Fi per band, DHCP summary/pool |
| 4 | `router_storage` | `storage` | **Done** — root, eMMC/overlay/extroot, swap |
| 5 | `router_wifi` | `wifi` | **Done** — SSID, encryption, AP state, WPA QR |
| 6 | `router_security` | `security` | Stub — firewall, blocky/banIP, VPN |

Navigation: **BOOT** tap (GPIO0), touch swipe, CM5 **MaskROM/USERKEY**, LuCI **Services → MCU Display**, or host FIFO / raw RDCP `cmd`.

---

## 1. System screen (`router_system`)

Host payload (`mcudd` scope `system`):

```json
{"hostname":"…","uptime_short":"…","cpu":"12","cpu_temp":"--","ram_pct":40,"ram_used":"512M","load_short":"0.42"}
```

### Done
- [x] LVGL layout: CPU arc, RAM bar, hostname, uptime
- [x] Parse metrics in `router_data.c`
- [x] Host collects hostname, load→CPU%, RAM, uptime via `mcudd_metrics_system`
- [x] Page polls every 1.5 s when host-linked
- [x] Show RAM used text (`512M`) alongside percent
- [x] Show load average (`load_short`)
- [x] Show CPU temp row (hwmon / thermal_zone when present)
- [x] CPU arc color by load (green / amber / red)
- [x] Real CPU utilization from `/proc/stat` deltas
- [x] CM5 thermal zone / hwmon for `cpu_temp` (falls back to `--` if absent)
- [x] 1-minute CPU+RAM sparkline (40-sample ring buffer)
- [x] Link indicator (`LINK OK` / `LINK LOST` after 5 s UART silence)

### Backlog
- [ ] Enable RK3588 thermal driver in CM5 kernel if zones missing
- [ ] Real CPU sparkline legend / dual-axis polish

---

## 2. Network screen (`router_network`)

- [x] Live RX/TX rates from `/proc/net/dev` (WAN device, typically eth0)
- [x] WAN ping to default gateway, else 1.1.1.1 (background, cached)
- [x] Dual 2.5 GbE port labels (eth0 WAN, eth1+eth2 LAN)
- [x] Link up/down badges per interface + negotiated speed

---

## 3. Clients screen (`router_clients`)

### Done
- [x] Wi-Fi station counts per band (`iwinfo` AP assoclist → `wifi_24` / `wifi_5`)
- [x] DHCP lease summary from `/tmp/dhcp.leases` (`dhcp_summary` hostnames)
- [x] DHCP pool usage bar (`dhcp.lan.limit` → `dhcp_pct`)

### Payload
```json
{"wifi_24":"0","wifi_5":"2","lan_clients":"3","clients_total":"5 clients","dhcp_leases":"5","dhcp_pool":150,"dhcp_pct":3,"dhcp_summary":"phone, laptop, +3"}
```

---

## 4. Storage screen (`router_storage`)

### Done
- [x] Rootfs usage from `statvfs("/")` (`used/total` + pct)
- [x] eMMC / overlay / extroot detection via `/proc/mounts` (`data_kind`, `overlay_dev`)
- [x] Swap usage from `/proc/meminfo` (`off` when absent)

### Payload
```json
{"root_usage":"118M/496M","root_pct":24,"root_dev":"root","data_usage":"25.4G/28.5G","data_pct":93,"data_kind":"emmc","overlay_dev":"mmcblk0p1","swap_usage":"off","swap_pct":0}
```

---

## 5. Wi-Fi AP screen (`router_wifi`)

### Done
- [x] SSID + encryption mode from UCI (`wifi_enc`: open / WPA / WPA2 / WPA3 / WPA2/3)
- [x] AP enabled/disabled/down (`IFF_UP`; carrier is 0 with no clients)
- [x] WPA QR (`wifi_qr`) — `WIFI:T:WPA` or `T:nopass`; SAE still uses `T:WPA` (QR spec)

### Payload
```json
{"wifi_ssid":"ImmortalCM5","wifi_enc":"WPA2","wifi_ap_state":"up","wifi_qr":"WIFI:T:WPA;S:ImmortalCM5;P:secret;;"}
```

---

## 6. Security screen (`router_security`)

- [ ] Firewall4 zone summary
- [ ] Blocky / banIP blocked count (24 h)
- [ ] Tailscale / WireGuard / AmneziaWG tunnel count

---

## Protocol & diagnostics

| Feature | Status |
|---------|--------|
| RDCP v1 JSON lines | Done |
| `push` hello / boot / config | Done |
| `req` metrics per scope | Done |
| `cmd` screen / nav | Done |
| `req` / `res` ping/pong | Done (release ≥ 32) |
| `cmd` / `evt` echo | Done (release ≥ 32) |
| MessagePack wire format | Not planned on MCU |
| Gesture → host nav | Done (swipe + BOOT) |
| Long-press BOOT → CM5 poweroff | Done |

### UART link test (CM5)

```sh
/usr/lib/mcud/mcud-link-test.sh hello
cat /tmp/mcud_link_test.json
```

Raw ping:

```json
{"v":1,"t":"req","id":1,"op":"ping"}
```

---

## Host (`mcudd`) backlog

- [x] Live network throughput in `mcudd_metrics_network`
- [x] Wi-Fi AP SSID/encryption/QR in `mcudd_metrics_wifi`
- [ ] Security metrics from blocky/banIP counters
- [ ] Rate-limit FIFO nav when ESP32 RX saturated
- [ ] Do not update `/tmp/mcud_active_screen` until screen evt ack

---

## Firmware backlog

- [ ] Rename `ROUTER_BTN_SW1_GPIO` → `ROUTER_BTN_BOOT_GPIO` in docs/flags
- [ ] Scope-aware `router_ui_refresh` (update only visible page widgets)
- [ ] Dark theme variant (Bootstrap parity with LuCI)
- [ ] Demo mode on MCU when host offline (show last metrics grayed)

---

## Implementation priority

### Phase 1 — System tab polish (current)
- System screen shows all host `system` scope fields
- Docs aligned with router (this file)

### Phase 2 — Network + clients
- Real throughput and DHCP data on display

### Phase 3 — Wi-Fi + security
- QR join, firewall/blocky/VPN summary

### Phase 4 — Hardening
- UART recovery, version sync CI, e2e link test in CI

---

## Success criteria

- [ ] All six router pages show real CM5 data (not placeholders)
- [ ] LuCI, buttons, and swipe stay in sync with physical page
- [ ] Ping/echo/link test pass over JST UART after boot
- [x] `mcud-version.json` release matched host ↔ firmware
- [x] README + ROUTER_TODO reflect CM5 router (not NAS)
