#!/usr/bin/env python3
"""Push fake RDCP system-tab telemetry to the ESP32 panel.

Sends ``push hello`` so the MCU links, answers ``req metrics`` (scope system)
with a fake ``res``, and also pushes unsolicited system ``res`` so the glass
updates even if a request is missed.

  .venv/bin/python scripts/rdcp-fake-system.py
  .venv/bin/python scripts/rdcp-fake-system.py /dev/cu.usbserial-2140 --seconds 30
  .venv/bin/python scripts/rdcp-fake-system.py --cpu 88 --hostname lab-cm5

Stop the web simulator and router mcudd first.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path

import serial
from serial.tools import list_ports

CH340_VID = 0x1A86
CH340_PID = 0x7523
REPO = Path(__file__).resolve().parents[1]
VERSION_JSON = REPO / "mcud-version.json"


def ts() -> str:
    now = time.time()
    local = time.strftime("%H:%M:%S", time.localtime(now))
    return f"{local}.{int((now % 1) * 1000):03d}"


def load_hello() -> dict:
    data = {"stack": "1.0.0", "release": 47, "component": "mcudd", "rdcp": 1}
    try:
        j = json.loads(VERSION_JSON.read_text(encoding="utf-8"))
        data = {
            "stack": j.get("stack", "1.0.0"),
            "release": int(j.get("release", 0)),
            "component": j.get("components", {}).get("host", "mcudd"),
            "rdcp": int(j.get("rdcp", 1)),
        }
    except (OSError, ValueError, TypeError):
        pass
    return {"v": 1, "t": "push", "op": "hello", "data": data}


def prefer_port(explicit: str | None) -> str:
    if explicit:
        return explicit
    found = list(list_ports.comports())
    for p in found:
        if p.vid == CH340_VID and p.pid == CH340_PID:
            return p.device
    usb = [p for p in found if "usbserial" in (p.device or "")]
    if usb:
        return usb[0].device
    raise SystemExit("No serial port. Plug USB-C CH340 or pass a device path.")


def open_port(device: str, baud: int) -> serial.Serial:
    ser = serial.Serial()
    ser.port = device
    ser.baudrate = baud
    ser.bytesize = serial.EIGHTBITS
    ser.parity = serial.PARITY_NONE
    ser.stopbits = serial.STOPBITS_ONE
    ser.timeout = 0.05
    ser.write_timeout = 1
    ser.dsrdtr = False
    ser.rtscts = False
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def send_json(ser: serial.Serial, obj: dict) -> None:
    raw = json.dumps(obj, separators=(",", ":"))
    ser.write((raw + "\n").encode("utf-8"))
    ser.flush()
    print(f"{ts()}  TX     {raw}", flush=True)


def fake_system(args, elapsed: float) -> dict:
    wave = 0.5 + 0.5 * math.sin(elapsed / 4.0)
    cpu = args.cpu if args.cpu is not None else int(18 + 55 * wave)
    ram = args.ram if args.ram is not None else int(40 + 25 * wave)
    temp = args.temp if args.temp is not None else int(42 + 8 * wave)
    load = args.load if args.load is not None else round(0.15 + 1.2 * wave, 2)
    up_s = int(args.uptime + elapsed)
    days, rem = divmod(up_s, 86400)
    hours, rem = divmod(rem, 3600)
    mins = rem // 60
    if days:
        uptime = f"{days}d {hours}h"
    elif hours:
        uptime = f"{hours}h {mins}m"
    else:
        uptime = f"{mins}m"
    return {
        "hostname": args.hostname,
        "uptime_short": uptime,
        "cpu": str(max(0, min(100, cpu))),
        "cpu_temp": str(temp),
        "ram_pct": max(0, min(100, ram)),
        "ram_used": args.ram_used,
        "load_short": f"{load:.2f}",
    }


def parse_req(line: str):
    if not line.startswith("{"):
        return None
    try:
        obj = json.loads(line)
    except json.JSONDecodeError:
        return None
    if not isinstance(obj, dict):
        return None
    return obj


def tag_rx(line: str) -> str:
    if line.startswith("#rx"):
        return "RX#rx"
    if line.startswith("{"):
        return "RX"
    return "RX.raw"


def main() -> int:
    parser = argparse.ArgumentParser(description="Fake RDCP system-tab telemetry.")
    parser.add_argument("port", nargs="?", help="Serial device (default: CH340)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=30.0, help="0 = until Ctrl-C")
    parser.add_argument("--interval", type=float, default=1.5, help="Unsolicited res period")
    parser.add_argument("--hostname", default="cm5-fake")
    parser.add_argument("--cpu", type=int, default=None, help="Fixed CPU %% (default: sine)")
    parser.add_argument("--ram", type=int, default=None, help="Fixed RAM %%")
    parser.add_argument("--ram-used", default="3.1G")
    parser.add_argument("--temp", type=int, default=None)
    parser.add_argument("--load", type=float, default=None)
    parser.add_argument("--uptime", type=int, default=90000, help="Base uptime seconds")
    parser.add_argument("--no-hello", action="store_true")
    parser.add_argument("--no-push", action="store_true", help="Only answer MCU req metrics")
    args = parser.parse_args()

    device = prefer_port(args.port)
    try:
        ser = open_port(device, args.baud)
    except serial.SerialException as exc:
        print(f"open failed: {device}: {exc}", file=sys.stderr)
        return 1

    print(f"{ts()}  ----   open {device} {args.baud} 8N1  fake system tab", flush=True)
    time.sleep(0.3)

    t0 = time.time()
    deadline = None if args.seconds <= 0 else t0 + args.seconds
    last_push = 0.0
    buf = bytearray()
    next_id = 100
    answered = 0

    try:
        if not args.no_hello:
            send_json(ser, load_hello())
        while True:
            chunk = ser.read(4096)
            if chunk:
                buf.extend(chunk)
                while True:
                    nl = buf.find(b"\n")
                    if nl < 0:
                        break
                    line = bytes(buf[:nl]).decode("utf-8", errors="replace").rstrip("\r")
                    del buf[: nl + 1]
                    if not line:
                        continue
                    print(f"{ts()}  {tag_rx(line):<6} {line}", flush=True)
                    if line.startswith("#"):
                        continue
                    obj = parse_req(line)
                    if not obj:
                        continue
                    if obj.get("t") == "req" and obj.get("op") == "metrics":
                        scope = obj.get("scope") or "system"
                        if scope != "system":
                            continue
                        req_id = obj.get("id", next_id)
                        send_json(ser, {
                            "v": 1,
                            "t": "res",
                            "id": req_id,
                            "data": fake_system(args, time.time() - t0),
                        })
                        answered += 1
            now = time.time()
            if not args.no_push and now - last_push >= args.interval:
                send_json(ser, {
                    "v": 1,
                    "t": "res",
                    "id": next_id,
                    "data": fake_system(args, now - t0),
                })
                next_id += 1
                last_push = now
            if deadline is not None and now >= deadline:
                break
            time.sleep(0.02)
    except KeyboardInterrupt:
        print(f"\n{ts()}  ----   interrupt", flush=True)
    finally:
        ser.close()
        print(f"{ts()}  ----   close {device}  answered_req={answered}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
