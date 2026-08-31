#!/usr/bin/env python3
"""Raw RDCP UART RX/TX on the Mac CH340 (or any 115200 8N1 port).

Prints every line with a TX/RX tag. Optional handshake (hello + ping + version).
Close the web simulator and stop router mcudd first — one owner on GPIO1/3.

  .venv/bin/python scripts/rdcp-uart.py
  .venv/bin/python scripts/rdcp-uart.py /dev/cu.usbserial-2140 --seconds 20
  .venv/bin/python scripts/rdcp-uart.py --listen --seconds 15
  .venv/bin/python scripts/rdcp-uart.py --interactive
"""

from __future__ import annotations

import argparse
import json
import select
import sys
import time
from pathlib import Path

import serial
from serial.tools import list_ports

CH340_VID = 0x1A86
CH340_PID = 0x7523
REPO = Path(__file__).resolve().parents[1]
VERSION_JSON = REPO / "mcud-version.json"

HELLO = {
    "v": 1,
    "t": "push",
    "op": "hello",
    "data": {"stack": "1.0.0", "release": 47, "component": "mcudd", "rdcp": 1},
}
PING = {"v": 1, "t": "req", "id": 1, "op": "ping"}
VERSION = {"v": 1, "t": "req", "id": 2, "op": "version"}


def load_hello() -> dict:
    payload = dict(HELLO)
    try:
        j = json.loads(VERSION_JSON.read_text(encoding="utf-8"))
        payload["data"] = {
            "stack": j.get("stack", "1.0.0"),
            "release": int(j.get("release", 0)),
            "component": j.get("components", {}).get("host", "mcudd"),
            "rdcp": int(j.get("rdcp", 1)),
        }
    except (OSError, ValueError, TypeError):
        pass
    return payload


def prefer_ch340(fallback: str | None) -> str:
    found = list(list_ports.comports())
    for p in found:
        if p.vid == CH340_VID and p.pid == CH340_PID:
            return p.device
    if sys.platform == "darwin":
        for p in found:
            if "usbserial" in (p.device or "") and "1210" not in p.device:
                return p.device
    if fallback:
        return fallback
    if found:
        return found[0].device
    raise SystemExit("No serial port found. Plug USB-C or pass a device path.")


def ts() -> str:
    now = time.time()
    local = time.strftime("%H:%M:%S", time.localtime(now))
    return f"{local}.{int((now % 1) * 1000):03d}"


def tag_rx(line: str) -> str:
    if line.startswith("#rx"):
        return "RX#rx"
    if line.startswith("{") or line.startswith("["):
        return "RX"
    return "RX.raw"


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


def send_line(ser: serial.Serial, line: str) -> None:
    raw = line.rstrip("\r\n")
    ser.write((raw + "\n").encode("utf-8", errors="replace"))
    ser.flush()
    print(f"{ts()}  TX     {raw}", flush=True)


def drain_rx(ser: serial.Serial, buf: bytearray) -> None:
    chunk = ser.read(4096)
    if not chunk:
        return
    buf.extend(chunk)
    while True:
        nl = buf.find(b"\n")
        if nl < 0:
            if len(buf) > 8192:
                dump = bytes(buf).decode("utf-8", errors="replace")
                print(f"{ts()}  RX.raw {dump!r}", flush=True)
                buf.clear()
            return
        line = bytes(buf[:nl]).decode("utf-8", errors="replace").rstrip("\r")
        del buf[: nl + 1]
        if line:
            print(f"{ts()}  {tag_rx(line):<6} {line}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Raw RDCP UART RX/TX (CH340 / P1).")
    parser.add_argument("port", nargs="?", help="Serial device (default: CH340 1A86:7523)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--listen",
        action="store_true",
        help="Do not send; print RX only",
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        help="Type JSON lines; Enter sends. Ctrl-C to quit.",
    )
    parser.add_argument(
        "--seconds",
        type=float,
        default=None,
        help="Exit after N seconds (default 20; 0 = until Ctrl-C)",
    )
    args = parser.parse_args()
    if args.seconds is None:
        args.seconds = 0.0 if args.interactive else 20.0

    device = args.port or prefer_ch340(None)
    try:
        ser = open_port(device, args.baud)
    except serial.SerialException as exc:
        print(f"open failed: {device}: {exc}", file=sys.stderr)
        print("pio device list / ls /dev/cu.usbserial*", file=sys.stderr)
        return 1
    print(
        f"{ts()}  ----   open {device} {args.baud} 8N1 DTR/RTS=0  "
        f"(listen={args.listen} interactive={args.interactive})",
        flush=True,
    )
    time.sleep(0.3)

    buf = bytearray()
    deadline = None if args.seconds <= 0 else time.time() + args.seconds
    handshake_at = time.time() + 0.5
    sent_hello = args.listen

    try:
        while True:
            drain_rx(ser, buf)
            now = time.time()
            if not sent_hello and now >= handshake_at:
                send_line(ser, json.dumps(load_hello(), separators=(",", ":")))
                send_line(ser, json.dumps(PING, separators=(",", ":")))
                send_line(ser, json.dumps(VERSION, separators=(",", ":")))
                sent_hello = True
            if args.interactive and sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                typed = sys.stdin.readline()
                if typed == "":
                    break
                typed = typed.strip()
                if typed:
                    send_line(ser, typed)
            if deadline is not None and now >= deadline:
                break
            time.sleep(0.02)
    except KeyboardInterrupt:
        print(f"\n{ts()}  ----   interrupt", flush=True)
    finally:
        drain_rx(ser, buf)
        ser.close()
        print(f"{ts()}  ----   close {device}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
