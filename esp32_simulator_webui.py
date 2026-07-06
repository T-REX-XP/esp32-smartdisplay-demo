#!/usr/bin/env python3
"""
ESP32 Simulator Web UI
======================

A web interface to test and interact with the ESP32 Smart Display Simulator.
Provides a sandbox environment to send commands and monitor responses.

Features:
- Send screen change commands
- Request alarm lists, CPU metrics, and storage info
- View real-time communication logs
- Test all simulator functionalities

Usage:
    python esp32_simulator_webui.py [COM_PORT] [BAUD_RATE] [--format json|msgpack]
"""

import serial
import json
import time
import sys
import psutil
import argparse
import threading
import queue
from flask import Flask, render_template, request, jsonify, Response
import msgpack

class ESP32WebSimulator:
    def __init__(self, port='COM7', baudrate=115200, format='json'):
        self.port = port
        self.baudrate = baudrate
        self.format = format
        self.serial_conn = None
        self.running = False
        self.log_queue = queue.Queue()
        self.response_queue = queue.Queue()
        self.rdcp_req_id = 0
        self.router_pages = [
            "router_system", "router_network", "router_clients",
            "router_storage", "router_wifi", "router_security",
        ]
        self.active_screen = "router_boot"
        self.alarms = [
            {"time": "08:00", "label": "Morning Coffee", "enabled": True},
            {"time": "12:30", "label": "Lunch Break", "enabled": True},
            {"time": "18:00", "label": "Dinner Time", "enabled": False},
            {"time": "22:00", "label": "Bedtime", "enabled": True}
        ]

    def log(self, message):
        """Add message to log queue"""
        timestamp = time.strftime("%H:%M:%S")
        self.log_queue.put(f"[{timestamp}] {message}")
        print(message)

    def connect(self):
        """Establish serial connection"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1,
                write_timeout=1
            )
            self.serial_conn.dtr = False
            self.serial_conn.rts = False
            time.sleep(3)
            self.log(f"✅ Connected to {self.port} at {self.baudrate} baud")
            self.send_boot_push("boot", "Starting host simulator...", 15)
            return True
        except serial.SerialException as e:
            self.log(f"❌ Failed to connect to {self.port}: {e}")
            return False

    def disconnect(self):
        """Close serial connection"""
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            self.log("🔌 Disconnected from serial port")

    def send_json(self, data):
        """Send JSON data"""
        try:
            json_str = json.dumps(data, separators=(',', ':'))
            self.serial_conn.write((json_str + '\n').encode('utf-8'))
            self.serial_conn.flush()
            self.log(f"📤 Sent JSON: {json_str}")
        except Exception as e:
            self.log(f"❌ Error sending JSON: {e}")

    def send_msgpack(self, data):
        """Send MessagePack data"""
        try:
            packed = msgpack.packb(data)
            self.serial_conn.write(packed)
            self.serial_conn.write(b'\n')
            self.serial_conn.flush()
            self.log(f"📦 Sent MessagePack: {data}")
        except Exception as e:
            self.log(f"❌ Error sending MessagePack: {e}")

    def send_data(self, data):
        """Send data using configured format"""
        if self.format == 'msgpack':
            self.send_msgpack(data)
        else:
            self.send_json(data)

    def handle_alarm_request(self):
        """Send alarm list"""
        self.log("📋 Sending alarm list...")
        self.send_data({"alarms": self.alarms})

    def get_real_cpu_usage(self):
        """Get real CPU usage"""
        try:
            cpu_percent = psutil.cpu_percent(interval=1)
            return int(cpu_percent)
        except Exception as e:
            self.log(f"Error getting CPU usage: {e}")
            return 0

    def handle_cpu_request(self):
        """Send CPU metrics"""
        cpu_usage = self.get_real_cpu_usage()

        try:
            memory = psutil.virtual_memory()
            disk = psutil.disk_usage('/')

            metrics = {
                "cpu": str(cpu_usage),
                "temp_c": f"{psutil.sensors_temperatures().get('coretemp', [{}])[0].get('current', 45.0):.1f}",
                "fs_free": f"{disk.free / (1024*1024):.1f}",
                "fs_used": f"{disk.used / (1024*1024):.1f}"
            }
        except Exception as e:
            metrics = {
                "cpu": str(cpu_usage),
                "temp_c": "45.0",
                "fs_free": "1024.0",
                "fs_used": "2048.0"
            }

        self.send_data(metrics)

    def get_storage_info(self):
        """Get storage information"""
        try:
            storage_info = []
            partitions = psutil.disk_partitions(all=False)

            for partition in partitions:
                try:
                    usage = psutil.disk_usage(partition.mountpoint)
                    total_gb = usage.total / (1024**3)
                    used_gb = usage.used / (1024**3)
                    free_gb = usage.free / (1024**3)

                    drive_info = {
                        "device": partition.device,
                        "mountpoint": partition.mountpoint,
                        "fstype": partition.fstype,
                        "opts": partition.opts,
                        "total_gb": f"{total_gb:.2f}",
                        "used_gb": f"{used_gb:.2f}",
                        "free_gb": f"{free_gb:.2f}",
                        "used_percent": f"{usage.percent:.1f}"
                    }

                    storage_info.append(drive_info)

                except (OSError, PermissionError) as e:
                    continue

            return storage_info

        except Exception as e:
            self.log(f"Error getting storage info: {e}")
            return []

    def handle_storage_request(self):
        """Send storage information"""
        self.log("💾 Sending storage information...")
        storage_info = self.get_storage_info()
        self.send_data({"storage": storage_info})

    def build_scope_metrics(self, scope):
        """Build router scope payload (mcudd-compatible dev data)."""
        cpu_usage = self.get_real_cpu_usage()
        mem = psutil.virtual_memory()
        disk = psutil.disk_usage('/')

        if scope == 'system':
            return {
                "hostname": "dev-host",
                "uptime_short": "1h 00m",
                "cpu": str(cpu_usage),
                "cpu_temp": "42.0",
                "ram_pct": int(mem.percent),
                "ram_used": f"{mem.used // (1024 * 1024)}M",
                "load_short": "0.25",
            }
        if scope == 'network':
            return {
                "wan_ip": "192.168.1.1",
                "rx_rate": "12.4M",
                "tx_rate": "1.2M",
                "ping_ms": 14,
                "link_ok": True,
            }
        if scope == 'clients':
            return {
                "wifi_24": "3",
                "wifi_5": "2",
                "lan_clients": "5",
                "clients_total": "5",
                "dhcp_leases": "5",
                "dhcp_pct": 25,
            }
        if scope == 'storage':
            storage = self.get_storage_info()
            root_pct = int(disk.percent) if disk.total else 0
            return {
                "root_usage": f"{root_pct}%",
                "root_pct": root_pct,
                "data_usage": "--",
                "data_pct": 0,
                "swap_usage": "0",
                "storage": storage or [{
                    "mountpoint": "/",
                    "used_percent": str(root_pct),
                    "free_gb": f"{disk.free / (1024**3):.2f}",
                }],
            }
        if scope == 'wifi':
            return {
                "wifi_ssid": "OpenWrt-Demo",
                "wifi_ap_state": "up",
                "wifi_qr": "WIFI:T:WPA;S:OpenWrt-Demo;P:demo-pass;;",
            }
        if scope == 'security':
            return {
                "firewall_state": "on",
                "blocked_24h": "0",
                "vpn_tunnels": "0",
            }
        if scope == 'alarms':
            return {"alarms": self.alarms}
        return {"error": "unknown_scope"}

    def send_rdcp_res(self, req_id, data):
        """RDCP response (host → MCU), always JSON on the wire."""
        frame = {"v": 1, "t": "res", "id": req_id, "data": data}
        self.send_json(frame)

    def send_boot_push(self, stage, text, pct):
        frame = {
            "v": 1,
            "t": "push",
            "op": "boot",
            "data": {"stage": stage, "text": text, "pct": pct},
        }
        self.send_json(frame)

    def send_cmd_screen(self, screen_id):
        frame = {
            "v": 1,
            "t": "cmd",
            "op": "screen",
            "data": {"screen": screen_id},
        }
        self.active_screen = screen_id
        self.send_json(frame)

    def scope_for_screen(self, screen_id):
        scope_map = {
            "router_system": "system",
            "router_network": "network",
            "router_clients": "clients",
            "router_storage": "storage",
            "router_wifi": "wifi",
            "router_security": "security",
        }
        return scope_map.get(screen_id, "system")

    def page_neighbor(self, screen_id, direction):
        if screen_id == "router_boot":
            return self.router_pages[0]
        try:
            idx = self.router_pages.index(screen_id)
        except ValueError:
            return self.router_pages[0]
        if direction in ("left", "next"):
            return self.router_pages[(idx + 1) % len(self.router_pages)]
        return self.router_pages[(idx - 1) % len(self.router_pages)]

    def handle_gesture(self, direction):
        nxt = self.page_neighbor(self.active_screen, direction)
        self.log(f"👆 Gesture {direction}: {self.active_screen} -> {nxt}")
        self.send_cmd_screen(nxt)

    def handle_rdcp_request(self, command):
        """MCU → host RDCP req (metrics, etc.)."""
        req_id = command.get("id", 0)
        op = command.get("op")

        if op == "metrics":
            scope = command.get("scope", "system")
            self.log(f"📊 RDCP metrics request scope={scope} id={req_id}")
            data = self.build_scope_metrics(scope)
            if req_id:
                self.send_rdcp_res(req_id, data)
            else:
                self.send_data(data)
            return

        self.log(f"⚠️ Unknown RDCP op: {op}")

    def handle_legacy_request(self, request_type):
        """Legacy {\"request\":\"cpu\"} shim."""
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
        data = self.build_scope_metrics(scope)
        if request_type == "cpu":
            self.send_data(data)
        elif request_type == "storage":
            self.send_data(data)
        else:
            self.send_data(data)

    def handle_screen_change(self, screen_name):
        """Handle screen change"""
        self.log(f"📱 Screen changed to: {screen_name}")

        if screen_name == "Alarm":
            time.sleep(0.1)
            self.handle_alarm_request()

    def process_command(self, command_str):
        """Process incoming JSON line from MCU (req, evt, legacy)."""
        try:
            command = json.loads(command_str.strip())
            self.log(f"📥 Received: {command}")
            self.response_queue.put(command)

            if command.get("v") == 1:
                t = command.get("t")
                if t == "req":
                    self.handle_rdcp_request(command)
                    return
                if t == "evt":
                    op = command.get("op")
                    data = command.get("data", {})
                    if op == "screen":
                        screen = data.get("screen", "?")
                        self.active_screen = screen
                        self.log(f"📱 MCU screen event: {screen}")
                        if screen == "router_boot":
                            self.send_boot_push("boot", "Host connected — booting...", 25)
                        return
                    if op == "input" and data.get("type") == "gesture":
                        self.handle_gesture(data.get("dir", "left"))
                        return

            if "screen" in command:
                self.handle_screen_change(command["screen"])
            elif "request" in command:
                self.handle_legacy_request(command["request"])
            elif "alarms" in command:
                self.log("📝 Alarm list updated")

        except json.JSONDecodeError as e:
            self.log(f"❌ Invalid JSON: {command_str.strip()} - {e}")
        except Exception as e:
            self.log(f"❌ Error processing command: {e}")

    def listen_loop(self):
        """Main listening loop"""
        self.log("👂 Listening for commands...")
        buffer = ""

        while self.running:
            try:
                if self.serial_conn and self.serial_conn.is_open:
                    if self.serial_conn.in_waiting > 0:
                        data = self.serial_conn.read(self.serial_conn.in_waiting).decode('utf-8', errors='ignore')
                        buffer += data

                        while '\n' in buffer:
                            line_end = buffer.find('\n')
                            line = buffer[:line_end].strip()
                            buffer = buffer[line_end + 1:]

                            if line:
                                self.process_command(line)

                time.sleep(0.01)

            except serial.SerialException as e:
                self.log(f"❌ Serial error: {e}")
                break
            except Exception as e:
                self.log(f"❌ Unexpected error: {e}")
                break

    def start_listening(self):
        """Start the listening thread"""
        self.running = True
        self.listen_thread = threading.Thread(target=self.listen_loop, daemon=True)
        self.listen_thread.start()

    def stop_listening(self):
        """Stop the listening thread"""
        self.running = False
        if hasattr(self, 'listen_thread'):
            self.listen_thread.join(timeout=1)

# Flask Web Application
app = Flask(__name__)
simulator = None

@app.route('/')
def index():
    """Main web interface"""
    return render_template('index.html')

@app.route('/api/connect', methods=['POST'])
def connect():
    """Connect to serial port"""
    global simulator
    data = request.get_json()

    port = data.get('port', 'COM3')
    baudrate = int(data.get('baudrate', 115200))
    format_type = data.get('format', 'json')

    if simulator:
        simulator.disconnect()

    simulator = ESP32WebSimulator(port, baudrate, format_type)

    if simulator.connect():
        simulator.start_listening()
        return jsonify({'status': 'connected', 'port': port, 'baudrate': baudrate})
    else:
        return jsonify({'status': 'error', 'message': f'Failed to connect to {port}'})

@app.route('/api/disconnect', methods=['POST'])
def disconnect():
    """Disconnect from serial port"""
    global simulator
    if simulator:
        simulator.stop_listening()
        simulator.disconnect()
        simulator = None

    return jsonify({'status': 'disconnected'})

@app.route('/api/send_command', methods=['POST'])
def send_command():
    """Send a command to the simulator"""
    global simulator
    if not simulator or not simulator.serial_conn or not simulator.serial_conn.is_open:
        return jsonify({'status': 'error', 'message': 'Not connected'})

    data = request.get_json()
    command = data.get('command', {})

    try:
        simulator.send_data(command)
        return jsonify({'status': 'sent', 'command': command})
    except Exception as e:
        return jsonify({'status': 'error', 'message': str(e)})

@app.route('/api/logs')
def logs():
    """Server-sent events for logs"""
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

    return Response(generate(), mimetype='text/event-stream')

@app.route('/api/responses')
def responses():
    """Server-sent events for responses"""
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

    return Response(generate(), mimetype='text/event-stream')

@app.route('/api/pages')
def pages():
    """Router screen manifest (matches mcud/pages.json)."""
    return jsonify({
        "version": 1,
        "screens": [
            {"id": "router_system", "title": "SYSTEM", "scope": "system", "icon": "microchip"},
            {"id": "router_network", "title": "NETWORK", "scope": "network", "icon": "network-wired"},
            {"id": "router_clients", "title": "CLIENTS", "scope": "clients", "icon": "users"},
            {"id": "router_storage", "title": "STORAGE", "scope": "storage", "icon": "hdd"},
            {"id": "router_wifi", "title": "WIFI AP", "scope": "wifi", "icon": "wifi"},
            {"id": "router_security", "title": "SECURITY", "scope": "security", "icon": "shield-halved"},
        ],
    })

@app.route('/api/send_rdcp_res', methods=['POST'])
def send_rdcp_res():
    """Push RDCP res frame to MCU (host → device, for UI testing)."""
    global simulator
    if not simulator or not simulator.serial_conn or not simulator.serial_conn.is_open:
        return jsonify({'status': 'error', 'message': 'Not connected'})

    data = request.get_json() or {}
    scope = data.get('scope', 'system')
    req_id = int(data.get('id', 1))
    payload = simulator.build_scope_metrics(scope)
    simulator.send_rdcp_res(req_id, payload)
    return jsonify({'status': 'sent', 'scope': scope, 'id': req_id, 'data': payload})

@app.route('/api/status')
def status():
    """Get current connection status"""
    global simulator
    if simulator and simulator.serial_conn and simulator.serial_conn.is_open:
        return jsonify({
            'connected': True,
            'port': simulator.port,
            'baudrate': simulator.baudrate,
            'format': simulator.format
        })
    else:
        return jsonify({'connected': False})

def main():
    global simulator

    parser = argparse.ArgumentParser(description="ESP32 Simulator Web UI")
    parser.add_argument('port', nargs='?', default='COM3', help='Serial port (default: COM3)')
    parser.add_argument('baudrate', nargs='?', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--format', choices=['json', 'msgpack'], default='json',
                       help='Data format (default: json)')
    parser.add_argument('--web-port', type=int, default=5000, help='Web server port (default: 5000)')

    args = parser.parse_args()

    # Initialize simulator and auto-connect in hardware mode.
    simulator = ESP32WebSimulator(args.port, args.baudrate, args.format)
    if simulator.connect():
        simulator.start_listening()
    else:
        print(f"⚠️  Could not connect to {args.port}. Use the web UI Connect button to retry.")

    print("🚀 ESP32 Simulator Web UI")
    print("=" * 40)
    print(f"Serial Port: {args.port}")
    print(f"Baud Rate: {args.baudrate}")
    print(f"Format: {args.format}")
    print(f"Web UI: http://localhost:{args.web_port}")
    print("=" * 40)

    try:
        app.run(host='0.0.0.0', port=args.web_port, debug=False, threaded=True)
    except KeyboardInterrupt:
        print("\n🛑 Shutting down...")
        if simulator:
            simulator.stop_listening()
            simulator.disconnect()

if __name__ == "__main__":
    main()