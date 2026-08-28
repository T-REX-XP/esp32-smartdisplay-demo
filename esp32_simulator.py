#!/usr/bin/env python3
"""
ESP32 MCU display — host-side UART simulator (mcudd stand-in)
=============================================================

Talks RDCP v1 JSON to ``esp32-2432S022C-router`` firmware over serial, the same
way ``mcudd`` does on the CM5. Also still answers the upstream demo firmware's
legacy ``{"request":"cpu"}`` / ``storage`` / ``alarms`` lines.

Usage:
    python3 esp32_simulator.py /dev/cu.usbserial-2140 115200 --format json
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import socket
import time
from pathlib import Path

import msgpack
import psutil
import serial

ROUTER_PAGES = [
    "router_system",
    "router_network",
    "router_clients",
    "router_storage",
    "router_wifi",
    "router_security",
]

SCOPE_FOR_SCREEN = {
    "router_system": "system",
    "router_network": "network",
    "router_clients": "clients",
    "router_storage": "storage",
    "router_wifi": "wifi",
    "router_security": "security",
}

WIFI_DEMO_SSID = "ImmortalCM5"
WIFI_DEMO_PSK = "demo-pass"


def load_mcud_version(path: Path | None = None) -> dict:
    """Read stack/release from mcud-version.json (host component)."""
    if path is None:
        path = Path(__file__).resolve().parent / "mcud-version.json"
    fallback = {
        "rdcp": 1,
        "stack": "1.0.0",
        "release": 0,
        "component": "mcudd",
    }
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        return {
            "rdcp": int(data.get("rdcp", 1)),
            "stack": str(data.get("stack", "1.0.0")),
            "release": int(data.get("release", 0)),
            "component": "mcudd",
        }
    except (OSError, ValueError, TypeError):
        return fallback


def human_bytes(n: int) -> str:
    n = max(0, int(n))
    if n >= 100 * (1 << 30):
        return f"{n >> 30}G"
    if n >= 1 << 30:
        return f"{n / (1024.0 ** 3):.1f}G"
    if n >= 1 << 20:
        return f"{(n + (1 << 19)) >> 20}M"
    return f"{(n + 512) >> 10}K"


def format_uptime(seconds: float) -> str:
    up = int(seconds)
    if up >= 86400:
        return f"{up // 86400}d {(up % 86400) // 3600:02d}h"
    if up >= 3600:
        return f"{up // 3600}h {(up % 3600) // 60:02d}m"
    return f"{up // 60}m"


def classify_data_mount(device: str, mountpoint: str, fstype: str) -> str | None:
    """Best-effort data_kind for a non-root mount (host overlay/extroot analog)."""
    if mountpoint in ("/", "/boot", "/boot/efi", "/System/Volumes/Data"):
        return None
    if fstype in ("tmpfs", "devfs", "proc", "sysfs", "devpts", "overlay"):
        return None
    dev = (device or "").lower()
    mp = (mountpoint or "").lower()
    if "mmcblk" in dev or "mmc" in dev:
        return "emmc"
    if any(x in dev for x in ("nvme", "sd", "disk", "vd")) or mp.startswith("/mnt"):
        return "extroot"
    if mp in ("/overlay", "/data"):
        return "overlay"
    return None


class ESP32Simulator:
    def __init__(self, port="/dev/ttyUSB0", baudrate=115200, format="json"):
        self.port = port
        self.baudrate = baudrate
        self.format = format  # legacy demo payloads only; RDCP is always JSON
        self.serial_conn = None
        self.running = False
        self.version = load_mcud_version()
        self.active_screen = "router_boot"
        self.last_metrics = None
        self.last_metrics_scope = None
        self.fw_version = None
        self._net_prev = None
        self._net_prev_t = 0.0
        self._rx_rate = "--"
        self._tx_rate = "--"

        self.alarms = [
            {"time": "08:00", "label": "Morning Coffee", "enabled": True},
            {"time": "12:30", "label": "Lunch Break", "enabled": True},
            {"time": "18:00", "label": "Dinner Time", "enabled": False},
            {"time": "22:00", "label": "Bedtime", "enabled": True},
        ]

    def log(self, message):
        print(message)

    def connect(self):
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1,
                write_timeout=1,
            )
            self.serial_conn.dtr = False
            self.serial_conn.rts = False
            time.sleep(3)
            self.log(f"✅ Connected to {self.port} at {self.baudrate} baud (DTR/RTS disabled)")
            self.send_hello()
            self.send_boot_push("boot", "Host simulator connected…", 20)
            self.send_req_version()
            return True
        except serial.SerialException as e:
            self.log(f"❌ Failed to connect to {self.port}: {e}")
            return False

    def disconnect(self):
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            self.log("🔌 Disconnected from serial port")

    def send_json(self, data):
        try:
            json_str = json.dumps(data, separators=(",", ":"))
            self.serial_conn.write((json_str + "\n").encode("utf-8"))
            self.serial_conn.flush()
            self.log(f"📤 Sent JSON: {json_str}")
        except Exception as e:
            self.log(f"❌ Error sending JSON data: {e}")

    def send_msgpack(self, data):
        try:
            packed = msgpack.packb(data)
            self.serial_conn.write(packed)
            self.serial_conn.write(b"\n")
            self.serial_conn.flush()
            self.log(f"📦 Sent MessagePack: {data}")
        except Exception as e:
            self.log(f"❌ Error sending MessagePack: {e}")

    def send_data(self, data):
        """Legacy demo encoding. RDCP frames must use send_json / send_rdcp_*."""
        if self.format == "msgpack":
            self.send_msgpack(data)
        else:
            self.send_json(data)

    def send_host_frame(self, data):
        """Host → MCU: RDCP is JSON-only on the router firmware."""
        if isinstance(data, dict) and data.get("v") == 1:
            self.send_json(data)
            return
        self.send_data(data)

    def hello_payload(self):
        return {
            "stack": self.version["stack"],
            "release": self.version["release"],
            "component": self.version["component"],
            "rdcp": self.version["rdcp"],
        }

    def send_hello(self):
        self.send_json({"v": 1, "t": "push", "op": "hello", "data": self.hello_payload()})

    def send_boot_push(self, stage, text, pct, screen="router_boot"):
        self.send_json({
            "v": 1,
            "t": "push",
            "op": "boot",
            "data": {"stage": stage, "text": text, "pct": int(pct), "screen": screen},
        })

    def send_alert(self, text, screen="router_network"):
        self.send_json({
            "v": 1,
            "t": "push",
            "op": "alert",
            "data": {"text": text, "screen": screen},
        })

    def send_rdcp_res(self, req_id, data):
        self.last_metrics = data
        self.send_json({"v": 1, "t": "res", "id": int(req_id or 0), "data": data})

    def send_cmd_screen(self, screen_id, direction="left"):
        self.active_screen = screen_id
        self.send_json({
            "v": 1,
            "t": "cmd",
            "op": "screen",
            "data": {"screen": screen_id, "dir": direction},
        })

    def send_cmd_nav(self, direction="next"):
        self.send_json({
            "v": 1,
            "t": "cmd",
            "op": "nav",
            "data": {"dir": direction},
        })

    def send_req_version(self, req_id=1):
        self.send_json({"v": 1, "t": "req", "id": int(req_id), "op": "version"})

    def send_req_ping(self, req_id=1):
        self.send_json({"v": 1, "t": "req", "id": int(req_id), "op": "ping"})

    def send_cmd_echo(self, text="ping"):
        self.send_json({
            "v": 1,
            "t": "cmd",
            "op": "echo",
            "data": {"text": text},
        })

    def get_real_cpu_usage(self):
        try:
            return int(psutil.cpu_percent(interval=None))
        except Exception as e:
            self.log(f"Error getting CPU usage: {e}")
            return 0

    def _cpu_temp(self):
        try:
            sensors = psutil.sensors_temperatures() or {}
            for key in ("coretemp", "cpu_thermal", "k10temp", "apple-thermal"):
                entries = sensors.get(key) or []
                if entries:
                    cur = getattr(entries[0], "current", None)
                    if cur is not None:
                        return f"{float(cur):.0f}"
            for entries in sensors.values():
                if entries:
                    cur = getattr(entries[0], "current", None)
                    if cur is not None:
                        return f"{float(cur):.0f}"
        except Exception:
            pass
        return "--"

    def _wan_ip(self):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
                s.connect(("1.1.1.1", 80))
                ip = s.getsockname()[0]
                if ip and not ip.startswith("127."):
                    return ip
        except OSError:
            pass
        return "192.168.8.1"

    def _update_net_rates(self):
        now = time.monotonic()
        try:
            io = psutil.net_io_counters()
        except Exception:
            return
        if self._net_prev is None:
            self._net_prev = io
            self._net_prev_t = now
            self._rx_rate = "--"
            self._tx_rate = "--"
            return
        dt = now - self._net_prev_t
        if dt < 0.2:
            return
        rx = (io.bytes_recv - self._net_prev.bytes_recv) / dt
        tx = (io.bytes_sent - self._net_prev.bytes_sent) / dt
        self._net_prev = io
        self._net_prev_t = now
        self._rx_rate = f"{human_bytes(int(rx))}/s"
        self._tx_rate = f"{human_bytes(int(tx))}/s"

    def _swap_fields(self):
        try:
            sw = psutil.swap_memory()
        except Exception:
            return "off", 0
        if not sw.total:
            return "off", 0
        used, total = sw.used, sw.total
        pct = int((used * 100) / total) if total else 0
        return f"{human_bytes(used)}/{human_bytes(total)}", min(pct, 100)

    def _storage_data_mount(self):
        """Pick a data-like mount for overlay/extroot display; else none."""
        try:
            parts = psutil.disk_partitions(all=False)
        except Exception:
            return None
        for part in parts:
            kind = classify_data_mount(part.device, part.mountpoint, part.fstype)
            if not kind:
                continue
            try:
                usage = psutil.disk_usage(part.mountpoint)
            except (OSError, PermissionError):
                continue
            base = part.device.rsplit("/", 1)[-1]
            return {
                "kind": kind,
                "dev": base[:23],
                "usage": f"{human_bytes(usage.used)}/{human_bytes(usage.total)}",
                "pct": int(usage.percent),
            }
        return None

    def get_storage_info(self):
        storage_info = []
        try:
            for partition in psutil.disk_partitions(all=False):
                try:
                    usage = psutil.disk_usage(partition.mountpoint)
                    storage_info.append({
                        "device": partition.device,
                        "mountpoint": partition.mountpoint,
                        "fstype": partition.fstype,
                        "opts": partition.opts,
                        "total_gb": f"{usage.total / (1024 ** 3):.2f}",
                        "used_gb": f"{usage.used / (1024 ** 3):.2f}",
                        "free_gb": f"{usage.free / (1024 ** 3):.2f}",
                        "used_percent": f"{usage.percent:.1f}",
                    })
                except (OSError, PermissionError):
                    continue
        except Exception as e:
            self.log(f"Error getting storage info: {e}")
        return storage_info

    def wifi_qr(self, ssid=WIFI_DEMO_SSID, psk=WIFI_DEMO_PSK, enc="psk2"):
        def esc(s):
            out = []
            for ch in s:
                if ch in "\\;,:":
                    out.append("\\" + ch)
                elif ch != '"':
                    out.append(ch)
            return "".join(out)

        if enc in ("none", "open"):
            return f"WIFI:T:nopass;S:{esc(ssid)};;"
        return f"WIFI:T:WPA;S:{esc(ssid)};P:{esc(psk)};;"

    def build_scope_metrics(self, scope):
        """mcudd-compatible scope payload (router firmware)."""
        cpu_usage = self.get_real_cpu_usage()
        mem = psutil.virtual_memory()
        disk = psutil.disk_usage("/")
        hostname = platform.node().split(".")[0][:47] or "dev-host"
        try:
            load1 = os.getloadavg()[0]
        except OSError:
            load1 = cpu_usage / 100.0
        uptime = time.time() - psutil.boot_time()

        if scope == "system":
            payload = {
                "hostname": hostname,
                "uptime_short": format_uptime(uptime),
                "cpu": str(cpu_usage),
                "cpu_temp": self._cpu_temp(),
                "ram_pct": int(mem.percent),
                "ram_used": f"{mem.used // (1024 * 1024)}M",
                "load_short": f"{load1:.2f}",
            }
        elif scope == "network":
            self._update_net_rates()
            payload = {
                "wan_ip": self._wan_ip(),
                "wan_dev": "eth0",
                "rx_rate": self._rx_rate,
                "tx_rate": self._tx_rate,
                "ping_ms": 12,
                "ping_ok": True,
                "eth0_role": "WAN",
                "eth0_up": True,
                "eth0_speed": "2.5G",
                "eth1_role": "LAN",
                "eth1_up": True,
                "eth1_speed": "2.5G",
                "eth2_role": "LAN",
                "eth2_up": False,
                "eth2_speed": "--",
                "link_ok": True,
            }
        elif scope == "clients":
            payload = {
                "wifi_24": "1",
                "wifi_5": "2",
                "lan_clients": "3",
                "clients_total": "6 clients",
                "dhcp_leases": "6",
                "dhcp_pool": 150,
                "dhcp_pct": 4,
                "dhcp_summary": "phone, laptop, +4",
            }
        elif scope == "storage":
            root_used, root_total = disk.used, disk.total
            root_pct = int(disk.percent) if disk.total else 0
            extra = self._storage_data_mount()
            swap_usage, swap_pct = self._swap_fields()
            if extra:
                data_usage, data_pct, data_kind, overlay_dev = (
                    extra["usage"], extra["pct"], extra["kind"], extra["dev"],
                )
            else:
                data_usage, data_pct, data_kind, overlay_dev = "--", 0, "none", ""
            storage = self.get_storage_info()
            payload = {
                "root_usage": f"{human_bytes(root_used)}/{human_bytes(root_total)}",
                "root_pct": root_pct,
                "root_dev": "root",
                "data_usage": data_usage,
                "data_pct": data_pct,
                "data_kind": data_kind,
                "overlay_dev": overlay_dev,
                "swap_usage": swap_usage,
                "swap_pct": swap_pct,
                "storage": storage or [{
                    "mountpoint": "/",
                    "used_percent": str(root_pct),
                    "free_gb": f"{disk.free / (1024 ** 3):.2f}",
                }],
            }
        elif scope == "wifi":
            payload = {
                "wifi_ssid": WIFI_DEMO_SSID,
                "wifi_enc": "WPA2",
                "wifi_ap_state": "up",
                "wifi_qr": self.wifi_qr(),
            }
        elif scope == "security":
            payload = {
                "firewall_state": "on",
                "blocked_24h": "12",
                "vpn_tunnels": "0",
                "blocky_blocked": 12,
                "banip_blocked": 0,
            }
        elif scope == "alarms":
            payload = {"alarms": self.alarms}
        else:
            payload = {"error": "unknown_scope"}

        self.last_metrics = payload
        self.last_metrics_scope = scope
        return payload

    def handle_alarm_request(self):
        self.log("📋 Sending alarm list...")
        self.send_data({"alarms": self.alarms})

    def handle_cpu_request(self):
        """Legacy demo CPU request — also a flat system object the router accepts."""
        self.log("🔄 CPU request received")
        data = self.build_scope_metrics("system")
        data["temp_c"] = data.get("cpu_temp", "--")
        self.send_data(data)

    def handle_storage_request(self):
        self.log("💾 Storage request received")
        self.send_data(self.build_scope_metrics("storage"))

    def scope_for_screen(self, screen_id):
        return SCOPE_FOR_SCREEN.get(screen_id, "system")

    def page_neighbor(self, screen_id, direction):
        if screen_id == "router_boot":
            return ROUTER_PAGES[0]
        try:
            idx = ROUTER_PAGES.index(screen_id)
        except ValueError:
            return ROUTER_PAGES[0]
        if direction in ("left", "next"):
            return ROUTER_PAGES[(idx + 1) % len(ROUTER_PAGES)]
        return ROUTER_PAGES[(idx - 1) % len(ROUTER_PAGES)]

    def handle_gesture(self, direction):
        nxt = self.page_neighbor(self.active_screen, direction)
        self.log(f"👆 Gesture {direction}: {self.active_screen} -> {nxt}")
        self.send_cmd_screen(nxt, direction)

    def handle_rdcp_request(self, command):
        req_id = command.get("id", 0)
        op = command.get("op")
        if op == "metrics":
            scope = command.get("scope", "system")
            self.log(f"📊 RDCP metrics request scope={scope} id={req_id}")
            data = self.build_scope_metrics(scope)
            if req_id:
                self.send_rdcp_res(req_id, data)
            else:
                self.send_json(data)
            return
        if op == "version":
            self.log(f"ℹ️ MCU asked for version (unusual); host hello already sent id={req_id}")
            return
        if op == "ping":
            # MCU is the pong side; ignore stray MCU pings.
            self.log(f"ℹ️ Unexpected MCU ping id={req_id}")
            return
        self.log(f"⚠️ Unknown RDCP op: {op}")

    def handle_rdcp_event(self, command):
        op = command.get("op")
        data = command.get("data") or {}
        if op == "screen":
            screen = data.get("screen", "?")
            self.active_screen = screen
            self.log(f"📱 MCU screen event: {screen}")
            if screen == "router_boot":
                self.send_boot_push("boot", "Host connected — booting…", 35)
            return
        if op == "input" and data.get("type") == "gesture":
            self.handle_gesture(data.get("dir", "left"))
            return
        if op == "version":
            self.fw_version = data
            self.log(
                f"🏷️ Firmware version stack={data.get('stack')} "
                f"release={data.get('release')} rdcp={data.get('rdcp')}"
            )
            return
        if op == "echo":
            self.log(f"🔁 Echo evt: {data.get('text', '')}")
            return
        self.log(f"⚠️ Unknown RDCP evt: {op}")

    def handle_rdcp_res(self, command):
        data = command.get("data") or {}
        if data.get("pong"):
            self.log(
                f"🏓 Pong id={command.get('id')} uptime_ms={data.get('uptime_ms')}"
            )
            return
        self.log(f"ℹ️ RDCP res from MCU: {command}")

    def handle_legacy_request(self, request_type):
        scope_map = {
            "cpu": "system",
            "system": "system",
            "storage": "storage",
            "alarms": "alarms",
            "network": "network",
            "clients": "clients",
            "wifi": "wifi",
            "security": "security",
        }
        scope = scope_map.get(request_type)
        if not scope:
            self.log(f"⚠️ Unknown request: {request_type}")
            return
        if scope == "alarms":
            self.handle_alarm_request()
            return
        if request_type == "cpu":
            self.handle_cpu_request()
            return
        if request_type == "storage":
            self.handle_storage_request()
            return
        self.send_data(self.build_scope_metrics(scope))

    def handle_screen_change(self, screen_name):
        self.log(f"📱 Screen changed to: {screen_name}")
        if screen_name == "Alarm":
            time.sleep(0.1)
            self.handle_alarm_request()

    def process_command(self, command_str):
        try:
            command = json.loads(command_str.strip())
            self.log(f"📥 Received: {command}")

            if command.get("v") == 1:
                t = command.get("t")
                if t == "req":
                    self.handle_rdcp_request(command)
                    return
                if t == "evt":
                    self.handle_rdcp_event(command)
                    return
                if t == "res":
                    self.handle_rdcp_res(command)
                    return

            if "screen" in command:
                self.handle_screen_change(command["screen"])
            elif "request" in command:
                self.handle_legacy_request(command["request"])
            elif "alarms" in command:
                self.log("📝 Alarm list updated by ESP32")
            else:
                self.log(f"⚠️  Unknown command: {command}")

        except json.JSONDecodeError as e:
            self.log(f"❌ Invalid JSON received: {command_str.strip()} - {e}")
        except Exception as e:
            self.log(f"❌ Error processing command: {e}")

    def listen_loop(self):
        self.log("👂 Listening for ESP32 RDCP (JSON lines)…")
        self.log(f"   Host version {self.version['stack']}+{self.version['release']} rdcp={self.version['rdcp']}")
        self.log("   MCU req metrics / evt screen|input|version — host replies with res/cmd/push")
        buffer = ""
        while self.running:
            try:
                if self.serial_conn and self.serial_conn.is_open:
                    if self.serial_conn.in_waiting > 0:
                        data = self.serial_conn.read(
                            self.serial_conn.in_waiting
                        ).decode("utf-8", errors="ignore")
                        buffer += data
                        while "\n" in buffer:
                            line_end = buffer.find("\n")
                            line = buffer[:line_end].strip()
                            buffer = buffer[line_end + 1 :]
                            if line:
                                self.process_command(line)
                time.sleep(0.01)
            except serial.SerialException as e:
                self.log(f"❌ Serial error: {e}")
                break
            except KeyboardInterrupt:
                self.log("\n🛑 Interrupted by user")
                break
            except Exception as e:
                self.log(f"❌ Unexpected error: {e}")
                break

    def run(self):
        self.log("🚀 ESP32 MCU display host simulator (RDCP v1)")
        self.log("=" * 50)
        if not self.connect():
            return
        self.running = True
        try:
            self.listen_loop()
        except KeyboardInterrupt:
            self.log("\n🛑 Shutting down...")
        finally:
            self.running = False
            self.disconnect()


def main():
    parser = argparse.ArgumentParser(description="ESP32 MCU display host simulator (RDCP v1)")
    parser.add_argument("port", nargs="?", default="COM3", help="Serial port (default: COM3)")
    parser.add_argument(
        "baudrate", nargs="?", type=int, default=115200, help="Baud rate (default: 115200)"
    )
    parser.add_argument(
        "--format",
        choices=["json", "msgpack"],
        default="json",
        help="Legacy demo payload encoding (RDCP is always JSON; default: json)",
    )
    args = parser.parse_args()
    ESP32Simulator(args.port, args.baudrate, args.format).run()


if __name__ == "__main__":
    main()
