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
    def __init__(self, port='COM7', baudrate=115200, format='msgpack'):
        self.port = port
        self.baudrate = baudrate
        self.format = format
        self.serial_conn = None
        self.running = False
        self.log_queue = queue.Queue()
        self.response_queue = queue.Queue()

        # Sample alarm data (same as simulator)
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

    def handle_screen_change(self, screen_name):
        """Handle screen change"""
        self.log(f"📱 Screen changed to: {screen_name}")

        if screen_name == "Alarm":
            time.sleep(0.1)
            self.handle_alarm_request()

    def process_command(self, command_str):
        """Process incoming command"""
        try:
            command = json.loads(command_str.strip())
            self.log(f"📥 Received: {command}")

            # Store response for web UI
            self.response_queue.put(command)

            if "screen" in command:
                self.handle_screen_change(command["screen"])
            elif "request" in command:
                request_type = command["request"]
                if request_type == "alarms":
                    self.handle_alarm_request()
                elif request_type == "cpu":
                    self.handle_cpu_request()
                elif request_type == "storage":
                    self.handle_storage_request()
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
    baudrate = data.get('baudrate', 115200)
    format_type = data.get('format', 'msgpack')

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
        global simulator
        if simulator:
            while True:
                try:
                    log_message = simulator.log_queue.get(timeout=1)
                    yield f"data: {log_message}\n\n"
                except queue.Empty:
                    continue
        else:
            yield "data: No simulator connected\n\n"
            time.sleep(1)

    return Response(generate(), mimetype='text/event-stream')

@app.route('/api/responses')
def responses():
    """Server-sent events for responses"""
    def generate():
        global simulator
        if simulator:
            while True:
                try:
                    response = simulator.response_queue.get(timeout=1)
                    yield f"data: {json.dumps(response)}\n\n"
                except queue.Empty:
                    continue
        else:
            yield "data: No simulator connected\n\n"
            time.sleep(1)

    return Response(generate(), mimetype='text/event-stream')

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
    parser.add_argument('--format', choices=['json', 'msgpack'], default='msgpack',
                       help='Data format (default: msgpack)')
    parser.add_argument('--web-port', type=int, default=5000, help='Web server port (default: 5000)')

    args = parser.parse_args()

    # Initialize simulator
    simulator = ESP32WebSimulator(args.port, args.baudrate, args.format)

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