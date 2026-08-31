# USB-C sniff of mcudd payloads (Mac)

The Sunton **ESP32-2432S022C** USB-C CH340 is wired to the same pins as the P1 JST RDCP link:

```text
Mac USB-C CH340 RX  ←  GPIO1  ←  MCU TX  (RDCP out, and `#rx` sniff)
Mac USB-C CH340 TX  →  GPIO3  →  MCU RX  (RDCP in from CM5 ttyS2)
CM5 ttyS2 TX        →  GPIO3
CM5 ttyS2 RX        ←  GPIO1
```

Firmware `RDCP_USB_MIRROR_RX=1` (`esp32-2432S022C-router`) copies every complete **host → MCU** line onto GPIO1 as:

```text
#rx {"v":1,"t":"push","op":"hello",…}
```

Unprefixed JSON on the same port is **MCU → host** (evt / req metrics). `mcudd` ignores `#` lines so the echo does not loop.

## Mac monitor

Do **not** type into the port (CH340 TX fights CM5 TX on GPIO3). `monitor_dtr=0` / `monitor_rts=0` are already set.

```sh
# mcudd may stay running; JST stays connected
pio device list    # /dev/cu.usbserial-*  VID:PID 1A86:7523
pio device monitor -e esp32-2432S022C-router --port /dev/cu.usbserial-XXXX
```

Or: `screen /dev/cu.usbserial-XXXX 115200` (quit: Ctrl-A then k).

| Line | Meaning |
|------|---------|
| `{"v":1,"t":"evt",…}` / `{"v":1,"t":"req",…}` | MCU → CM5 (and USB) |
| `#rx {"v":1,"t":"push",…}` | copy of what mcudd sent (hello, config, metrics `res`, ping) |

## If `#rx` never appears

CH340 TX is still driving GPIO3 (idle high) against CM5 UART TX. The MCU never parses hello, never mirrors. Then:

1. Unplug USB-C, keep JST, sniff on the **router** instead:

   ```sh
   ssh -i ~/.ssh/id_ed25519_openwrt_mcp root@192.168.8.1 \
     "uci set mcud.main.debug_serial=1; uci commit mcud; /etc/init.d/mcudd restart"
   ssh -i ~/.ssh/id_ed25519_openwrt_mcp root@192.168.8.1 \
     "logread -f -e mcudd" | grep 'uart tx:'
   ```

2. Or lift / disconnect the CH340 **TX** pad so USB-C is RX-only (flash with TX connected, sniff with TX lifted).

Disable the mirror (production, no USB sniff): remove `-D RDCP_USB_MIRROR_RX=1` or pass `-D RDCP_USB_MIRROR_RX=0` in `platformio.ini`.
