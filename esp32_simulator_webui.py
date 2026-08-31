#!/usr/bin/env python3
"""
ESP32 Simulator Web UI
======================

Browser stand-in for ``mcudd`` (RDCP v1) against ``esp32-2432S022C-router``
over USB-C CH340. Same payloads as the CM5 daemon. MCU owns pages; this host
answers ``req metrics``, hello/ping/echo, and shows a TX / RX / ``#rx`` wire log.

Usage:
    python3 esp32_simulator_webui.py /dev/cu.usbserial-2140 115200
    python3 esp32_simulator_webui.py --no-connect --web-port 5000
"""

from __future__ import annotations

import argparse
import json
import queue
import sys
import threading
import time

from flask import Flask, Response, jsonify, render_template, request
from serial.tools import list_ports

from esp32_simulator import ESP32Simulator, ROUTER_PAGES, SCOPE_FOR_SCREEN

CH340_VID = 0x1A86
CH340_PID = 0x7523
WIRE_QUEUE_MAX = 500


def list_serial_ports():
    ports = []
    for p in list_ports.comports():
        vid = int(p.vid) if p.vid is not None else None
        pid = int(p.pid) if p.pid is not None else None
        ports.append({
            "device": p.device,
            "description": p.description or "",
            "hwid": p.hwid or "",
            "vid": vid,
            "pid": pid,
            "ch340": vid == CH340_VID and pid == CH340_PID,
        })
    return ports


def prefer_ch340_port(fallback: str) -> str:
    found = list_serial_ports()
    for p in found:
        if p["ch340"]:
            return p["device"]
    if sys.platform == "darwin":
        for p in found:
            if "usbserial" in p["device"] or "wchusbserial" in p["device"]:
                return p["device"]
    return fallback


def _wire_event(direction: str, line: str | None = None, payload=None) -> dict:
    return {
        "dir": direction,
        "ts": time.strftime("%H:%M:%S"),
        "line": line if line is not None else (
            json.dumps(payload, separators=(",", ":")) if payload is not None else ""
        ),
        "payload": payload,
    }


class ESP32WebSimulator(ESP32Simulator):
    def __init__(self, port="COM7", baudrate=115200, format="json"):
        super().__init__(port=port, baudrate=baudrate, format=format)
        self.log_queue = queue.Queue()
        self.response_queue = queue.Queue()
        self.wire_queue = queue.Queue(maxsize=WIRE_QUEUE_MAX)
        self.listen_thread = None

    def log(self, message):
        timestamp = time.strftime("%H:%M:%S")
        self.log_queue.put(f"[{timestamp}] {message}")
        print(message)

    def _emit_wire(self, direction: str, line: str | None = None, payload=None):
        evt = _wire_event(direction, line=line, payload=payload)
        try:
            self.wire_queue.put_nowait(evt)
        except queue.Full:
            try:
                self.wire_queue.get_nowait()
            except queue.Empty:
                pass
            try:
                self.wire_queue.put_nowait(evt)
            except queue.Full:
                pass

    def send_json(self, data):
        super().send_json(data)
        self._emit_wire("tx", payload=data)

    def process_command(self, command_str):
        line = command_str.strip()
        if line.startswith("#"):
            self._emit_wire("sniff", line=line)
        else:
            try:
                obj = json.loads(line)
                self.response_queue.put(obj)
                self._emit_wire("rx", payload=obj)
            except json.JSONDecodeError:
                self._emit_wire("rx", line=line)
        super().process_command(command_str)

    def start_listening(self):
        self.running = True
        self.listen_thread = threading.Thread(target=self.listen_loop, daemon=True)
        self.listen_thread.start()

    def stop_listening(self):
        self.running = False
        if self.listen_thread:
            self.listen_thread.join(timeout=1)


app = Flask(__name__)
simulator = None
cli_defaults = {
    "port": "COM3",
    "baudrate": 115200,
    "format": "json",
}


def _connected() -> bool:
    return bool(
        simulator
        and simulator.serial_conn
        and simulator.serial_conn.is_open
    )


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/defaults")
def defaults():
    return jsonify({
        "port": cli_defaults["port"],
        "baudrate": cli_defaults["baudrate"],
        "format": cli_defaults["format"],
        "ports": list_serial_ports(),
    })


@app.route("/api/ports")
def ports():
    return jsonify({"ports": list_serial_ports()})


@app.route("/api/connect", methods=["POST"])
def connect():
    global simulator
    data = request.get_json() or {}
    port = data.get("port") or cli_defaults["port"]
    baudrate = int(data.get("baudrate", cli_defaults["baudrate"]))
    format_type = data.get("format", cli_defaults["format"])

    if simulator:
        simulator.stop_listening()
        simulator.disconnect()

    simulator = ESP32WebSimulator(port, baudrate, format_type)
    if simulator.connect():
        simulator.start_listening()
        return jsonify({
            "status": "connected",
            "port": port,
            "baudrate": baudrate,
            "format": format_type,
        })
    return jsonify({"status": "error", "message": f"Failed to connect to {port}"})


@app.route("/api/disconnect", methods=["POST"])
def disconnect():
    global simulator
    if simulator:
        simulator.stop_listening()
        simulator.disconnect()
        simulator = None
    return jsonify({"status": "disconnected"})


@app.route("/api/send_command", methods=["POST"])
def send_command():
    if not _connected():
        return jsonify({"status": "error", "message": "Not connected"})
    data = request.get_json() or {}
    command = data.get("command", {})
    try:
        simulator.send_host_frame(command)
        return jsonify({"status": "sent", "command": command})
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)})


def _sse_queue(get_item, idle_comment="keepalive"):
    def generate():
        while True:
            sim = simulator
            if not sim:
                yield f"data: {json.dumps({'idle': True, 'message': 'No simulator connected'})}\n\n"
                time.sleep(1)
                continue
            try:
                item = get_item(sim)
                yield f"data: {item}\n\n"
            except queue.Empty:
                yield f": {idle_comment}\n\n"

    return Response(
        generate(),
        mimetype="text/event-stream",
        headers={"Cache-Control": "no-cache", "X-Accel-Buffering": "no"},
    )


@app.route("/api/logs")
def logs():
    def get_item(sim):
        return json.dumps({"text": sim.log_queue.get(timeout=1)})

    return _sse_queue(get_item)


@app.route("/api/responses")
def responses():
    def get_item(sim):
        return json.dumps(sim.response_queue.get(timeout=1))

    return _sse_queue(get_item)


@app.route("/api/wire")
def wire():
    def get_item(sim):
        return json.dumps(sim.wire_queue.get(timeout=1))

    return _sse_queue(get_item, idle_comment="wire")


@app.route("/api/pages")
def pages():
    screens = []
    titles = {
        "router_system": ("SYSTEM", "microchip"),
        "router_network": ("NETWORK", "network-wired"),
        "router_clients": ("CLIENTS", "users"),
        "router_storage": ("STORAGE", "hdd"),
        "router_wifi": ("WIFI AP", "wifi"),
        "router_security": ("SECURITY", "shield-halved"),
    }
    for sid in ROUTER_PAGES:
        title, icon = titles.get(sid, (sid, "circle"))
        screens.append({
            "id": sid,
            "title": title,
            "scope": SCOPE_FOR_SCREEN[sid],
            "icon": icon,
        })
    return jsonify({"version": 1, "screens": screens})


@app.route("/api/send_rdcp_res", methods=["POST"])
def send_rdcp_res():
    if not _connected():
        return jsonify({"status": "error", "message": "Not connected"})
    data = request.get_json() or {}
    scope = data.get("scope", "system")
    req_id = int(data.get("id", 1))
    payload = simulator.build_scope_metrics(scope)
    simulator.send_rdcp_res(req_id, payload)
    return jsonify({"status": "sent", "scope": scope, "id": req_id, "data": payload})


@app.route("/api/hello", methods=["POST"])
def hello():
    if not _connected():
        return jsonify({"status": "error", "message": "Not connected"})
    simulator.send_hello()
    return jsonify({"status": "sent", "data": simulator.hello_payload()})


@app.route("/api/ping", methods=["POST"])
def ping():
    if not _connected():
        return jsonify({"status": "error", "message": "Not connected"})
    req_id = int((request.get_json() or {}).get("id", 1))
    simulator.send_req_ping(req_id)
    return jsonify({"status": "sent", "id": req_id})


@app.route("/api/echo", methods=["POST"])
def echo():
    if not _connected():
        return jsonify({"status": "error", "message": "Not connected"})
    text = (request.get_json() or {}).get("text", "ping")
    simulator.send_cmd_echo(text)
    return jsonify({"status": "sent", "text": text})


@app.route("/api/nav", methods=["POST"])
def nav():
    """MCU ignores cmd nav/screen; kept for Debug experiments only."""
    if not _connected():
        return jsonify({"status": "error", "message": "Not connected"})
    direction = (request.get_json() or {}).get("dir", "next")
    simulator.send_cmd_nav(direction)
    return jsonify({
        "status": "sent",
        "dir": direction,
        "note": "MCU owns pages; cmd nav is ignored by firmware",
    })


@app.route("/api/alert", methods=["POST"])
def alert():
    if not _connected():
        return jsonify({"status": "error", "message": "Not connected"})
    body = request.get_json() or {}
    text = body.get("text", "WAN down")
    screen = body.get("screen", "router_network")
    simulator.send_alert(text, screen)
    return jsonify({"status": "sent", "text": text, "screen": screen})


@app.route("/api/boot", methods=["POST"])
def boot():
    if not _connected():
        return jsonify({"status": "error", "message": "Not connected"})
    body = request.get_json() or {}
    text = body.get("text", "Network…")
    pct = int(body.get("pct", 60))
    simulator.send_boot_push("boot", text, pct)
    return jsonify({"status": "sent", "text": text, "pct": pct})


@app.route("/api/status")
def status():
    if _connected():
        return jsonify({
            "connected": True,
            "port": simulator.port,
            "baudrate": simulator.baudrate,
            "format": simulator.format,
            "active_screen": simulator.active_screen,
            "host_version": simulator.version,
            "fw_version": simulator.fw_version,
            "linked": bool(simulator.linked),
            "sniff_count": simulator.sniff_count,
            "last_metrics_scope": simulator.last_metrics_scope,
            "last_metrics": simulator.last_metrics,
        })
    return jsonify({"connected": False, "linked": False, "sniff_count": 0})


def main():
    global simulator

    parser = argparse.ArgumentParser(description="ESP32 Simulator Web UI (mcudd stand-in)")
    parser.add_argument("port", nargs="?", default=None, help="Serial port (default: CH340 if present)")
    parser.add_argument(
        "baudrate", nargs="?", type=int, default=115200, help="Baud rate (default: 115200)"
    )
    parser.add_argument(
        "--format",
        choices=["json", "msgpack"],
        default="json",
        help="Legacy demo encoding (RDCP is always JSON; default: json)",
    )
    parser.add_argument("--web-port", type=int, default=5000, help="Web server port (default: 5000)")
    parser.add_argument("--web-host", default="127.0.0.1", help="Bind address (default: 127.0.0.1)")
    parser.add_argument(
        "--no-connect",
        action="store_true",
        help="Start the UI without opening serial (Connect in the browser)",
    )
    args = parser.parse_args()

    port = args.port or prefer_ch340_port("/dev/cu.usbserial-2140" if sys.platform == "darwin" else "COM3")
    cli_defaults["port"] = port
    cli_defaults["baudrate"] = args.baudrate
    cli_defaults["format"] = args.format

    if not args.no_connect:
        simulator = ESP32WebSimulator(port, args.baudrate, args.format)
        if simulator.connect():
            simulator.start_listening()
        else:
            print(f"Could not connect to {port}. Use the web UI Connect button to retry.")
            simulator = None
    else:
        print("Serial left closed (--no-connect). Use the web UI Connect button.")

    print("ESP32 Simulator Web UI  (Mac = mcudd stand-in)")
    print("=" * 48)
    print(f"Serial Port: {port}")
    print(f"Baud Rate:   {args.baudrate}")
    print(f"Format:      {args.format} (RDCP always JSON)")
    print(f"Web UI:      http://127.0.0.1:{args.web_port}/")
    print("Stop router mcudd first. Close any screen/pio monitor on this port.")
    print("MCU owns pages. Swipe on the panel; host answers req metrics.")
    print("=" * 48)

    try:
        app.run(
            host=args.web_host,
            port=args.web_port,
            debug=False,
            threaded=True,
            use_reloader=False,
        )
    except KeyboardInterrupt:
        print("\nShutting down...")
        if simulator:
            simulator.stop_listening()
            simulator.disconnect()


if __name__ == "__main__":
    main()
