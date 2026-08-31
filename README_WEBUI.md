# ESP32 Simulator Web UI

Browser stand-in for **`mcudd`** talking **RDCP v1** to `esp32-2432S022C-router` over USB-C CH340.

This Mac **is the host**. Stop `/etc/init.d/mcudd` on the router and close `screen` / `pio device monitor` on the same port. MCU owns pages (swipe on the panel). The UI answers `req metrics`, sends hello/ping/echo, and shows a **TX / RX / `#rx`** wire log.

`#rx {…}` lines are firmware USB sniff echoes of host→MCU frames (same GPIO1 as MCU TX). They are not MCU requests.

## Run

```bash
pip install -r requirements.txt

# Stop router daemon first
ssh -i ~/.ssh/id_ed25519_openwrt_mcp root@192.168.8.1 '/etc/init.d/mcudd stop'

python3 esp32_simulator_webui.py /dev/cu.usbserial-2140 115200
```

Open [http://127.0.0.1:5000/](http://127.0.0.1:5000/). `--no-connect` starts the UI without opening serial. `--web-port 8080` if 5000 is busy.

Upstream Clock/Weather demo firmware: use the **Demo gadget** tab (not the CM5 router build).
