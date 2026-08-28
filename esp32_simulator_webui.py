#!/usr/bin/env python3
"""
ESP32 Simulator Web UI
======================

Browser sandbox for the CM5 router firmware (RDCP v1) and the upstream demo
gadget. Host-side logic lives in ``esp32_simulator.ESP32Simulator`` (same
payloads as ``mcudd``).

Usage:
    python3 esp32_simulator_webui.py /dev/cu.usbserial-2140 115200 --format json
"""

from __future__ import annotations

import argparse
import json
import queue
import threading
import time

from flask import Flask, Response, jsonify, render_template, request

from esp32_simulator import ESP32Simulator, ROUTER_PAGES, SCOPE_FOR_SCREEN


class ESP32WebSimulator(ESP32Simulator):
    def __init__(self, port="COM7", baudrate=115200, format="json"):
        super().__init__(port=port, baudrate=baudrate, format=format)
        self.log_queue = queue.Queue()
        self.response_queue = queue.Queue()
        self.listen_thread = None

    def log(self, message):
        timestamp = time.strftime("%H:%M:%S")
        self.log_queue.put(f"[{timestamp}] {message}")
        print(message)

    def process_command(self, command_str):
        try:
            command = json.loads(command_str.strip())
            self.response_queue.put(command)
        except json.JSONDecodeError:
            pass
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


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/connect", methods=["POST"])
def connect():
    global simulator
    data = request.get_json() or {}
    port = data.get("port", "COM3")
    baudrate = int(data.get("baudrate", 115200))
    format_type = data.get("format", "json")

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
    if not simulator or not simulator.serial_conn or not simulator.serial_conn.is_open:
        return jsonify({"status": "error", "message": "Not connected"})
    data = request.get_json() or {}
    command = data.get("command", {})
    try:
        simulator.send_host_frame(command)
        return jsonify({"status": "sent", "command": command})
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)})


@app.route("/api/logs")
def logs():
    def generate():
        while True:
            sim = simulator
            if not sim:
                yield "data: No simulator connected\n\n"
                time.sleep(1)
                continue
            try:
                log_message = sim.log_queue.get(timeout=1)
                yield f"data: {log_message}\n\n"
            except queue.Empty:
                continue

    return Response(generate(), mimetype="text/event-stream")


@app.route("/api/responses")
def responses():
    def generate():
        while True:
            sim = simulator
            if not sim:
                yield "data: No simulator connected\n\n"
                time.sleep(1)
                continue
            try:
                response = sim.response_queue.get(timeout=1)
                yield f"data: {json.dumps(response)}\n\n"
            except queue.Empty:
                continue

    return Response(generate(), mimetype="text/event-stream")


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
    if not simulator or not simulator.serial_conn or not simulator.serial_conn.is_open:
        return jsonify({"status": "error", "message": "Not connected"})
    data = request.get_json() or {}
    scope = data.get("scope", "system")
    req_id = int(data.get("id", 1))
    payload = simulator.build_scope_metrics(scope)
    simulator.send_rdcp_res(req_id, payload)
    return jsonify({"status": "sent", "scope": scope, "id": req_id, "data": payload})


@app.route("/api/hello", methods=["POST"])
def hello():
    if not simulator or not simulator.serial_conn or not simulator.serial_conn.is_open:
        return jsonify({"status": "error", "message": "Not connected"})
    simulator.send_hello()
    return jsonify({"status": "sent", "data": simulator.hello_payload()})


@app.route("/api/ping", methods=["POST"])
def ping():
    if not simulator or not simulator.serial_conn or not simulator.serial_conn.is_open:
        return jsonify({"status": "error", "message": "Not connected"})
    req_id = int((request.get_json() or {}).get("id", 1))
    simulator.send_req_ping(req_id)
    return jsonify({"status": "sent", "id": req_id})


@app.route("/api/echo", methods=["POST"])
def echo():
    if not simulator or not simulator.serial_conn or not simulator.serial_conn.is_open:
        return jsonify({"status": "error", "message": "Not connected"})
    text = (request.get_json() or {}).get("text", "ping")
    simulator.send_cmd_echo(text)
    return jsonify({"status": "sent", "text": text})


@app.route("/api/nav", methods=["POST"])
def nav():
    if not simulator or not simulator.serial_conn or not simulator.serial_conn.is_open:
        return jsonify({"status": "error", "message": "Not connected"})
    direction = (request.get_json() or {}).get("dir", "next")
    simulator.send_cmd_nav(direction)
    return jsonify({"status": "sent", "dir": direction})


@app.route("/api/alert", methods=["POST"])
def alert():
    if not simulator or not simulator.serial_conn or not simulator.serial_conn.is_open:
        return jsonify({"status": "error", "message": "Not connected"})
    body = request.get_json() or {}
    text = body.get("text", "WAN down")
    screen = body.get("screen", "router_network")
    simulator.send_alert(text, screen)
    return jsonify({"status": "sent", "text": text, "screen": screen})


@app.route("/api/boot", methods=["POST"])
def boot():
    if not simulator or not simulator.serial_conn or not simulator.serial_conn.is_open:
        return jsonify({"status": "error", "message": "Not connected"})
    body = request.get_json() or {}
    text = body.get("text", "Network…")
    pct = int(body.get("pct", 60))
    simulator.send_boot_push("boot", text, pct)
    return jsonify({"status": "sent", "text": text, "pct": pct})


@app.route("/api/status")
def status():
    if simulator and simulator.serial_conn and simulator.serial_conn.is_open:
        return jsonify({
            "connected": True,
            "port": simulator.port,
            "baudrate": simulator.baudrate,
            "format": simulator.format,
            "active_screen": simulator.active_screen,
            "host_version": simulator.version,
            "fw_version": simulator.fw_version,
            "last_metrics_scope": simulator.last_metrics_scope,
            "last_metrics": simulator.last_metrics,
        })
    return jsonify({"connected": False})


def main():
    global simulator

    parser = argparse.ArgumentParser(description="ESP32 Simulator Web UI")
    parser.add_argument("port", nargs="?", default="COM3", help="Serial port (default: COM3)")
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
    args = parser.parse_args()

    simulator = ESP32WebSimulator(args.port, args.baudrate, args.format)
    if simulator.connect():
        simulator.start_listening()
    else:
        print(f"⚠️  Could not connect to {args.port}. Use the web UI Connect button to retry.")

    print("🚀 ESP32 Simulator Web UI")
    print("=" * 40)
    print(f"Serial Port: {args.port}")
    print(f"Baud Rate: {args.baudrate}")
    print(f"Format: {args.format} (RDCP always JSON)")
    print(f"Web UI: http://localhost:{args.web_port}")
    print("=" * 40)

    try:
        app.run(host="0.0.0.0", port=args.web_port, debug=False, threaded=True)
    except KeyboardInterrupt:
        print("\n🛑 Shutting down...")
        if simulator:
            simulator.stop_listening()
            simulator.disconnect()


if __name__ == "__main__":
    main()
