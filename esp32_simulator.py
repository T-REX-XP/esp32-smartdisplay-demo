#!/usr/bin/env python3
"""
ESP32 Smart Display Simulator Service
====================================

This script runs as a Linux service that communicates with the ESP32 smart display
via UART serial connection. It provides:

1. Alarm list management - responds to alarm requests
2. Real CPU metrics - provides actual system CPU usage data
3. Storage drive information - provides drive capacity and free space data
4. Screen navigation handling

Protocol:
- MCU sends JSON commands via serial
- Server responds with MessagePack (default) or JSON data for optimal embedded performance

Usage:
    python esp32_simulator.py [COM_PORT] [BAUD_RATE] [--format json|msgpack]

Example:
    python esp32_simulator.py /dev/ttyUSB0 115200 --format msgpack
"""

import serial
import json
import time
import sys
import psutil
import argparse
import random
import platform
import msgpack

class ESP32Simulator:
    def __init__(self, port='/dev/ttyUSB0', baudrate=115200, format='msgpack'):
        self.port = port
        self.baudrate = baudrate
        self.format = format  # 'json' or 'msgpack'
        self.serial_conn = None
        self.running = False

        # Sample alarm data
        self.alarms = [
            {"time": "08:00", "label": "Morning Coffee", "enabled": True},
            {"time": "12:30", "label": "Lunch Break", "enabled": True},
            {"time": "18:00", "label": "Dinner Time", "enabled": False},
            {"time": "22:00", "label": "Bedtime", "enabled": True}
        ]

        # CPU monitoring parameters
        self.cpu_update_interval = 1.5  # Update every 1.5 seconds

    def connect(self):
        """Establish serial connection to ESP32"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1,
                write_timeout=1
            )
            # Disable DTR and RTS to prevent ESP32 reset on connection
            self.serial_conn.dtr = False
            self.serial_conn.rts = False
            
            # Wait for connection to stabilize
            time.sleep(3)
            
            print(f"✅ Connected to {self.port} at {self.baudrate} baud (DTR/RTS disabled)")
            return True
        except serial.SerialException as e:
            print(f"❌ Failed to connect to {self.port}: {e}")
            return False

    def disconnect(self):
        """Close serial connection"""
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            print("🔌 Disconnected from serial port")

    def send_json(self, data):
        """Send JSON data to ESP32"""
        try:
            json_str = json.dumps(data, separators=(',', ':'))
            self.serial_conn.write((json_str + '\n').encode('utf-8'))
            self.serial_conn.flush()
            print(f"📤 Sent JSON: {json_str}")
        except Exception as e:
            print(f"❌ Error sending JSON data: {e}")

    def send_data(self, data):
        """Send data using configured format (JSON or MessagePack)"""
        if self.format == 'msgpack':
            self.send_msgpack(data)
        else:
            self.send_json(data)

    def handle_alarm_request(self):
        """Send current alarm list to ESP32"""
        print("📋 Sending alarm list...")
        self.send_data({"alarms": self.alarms})

    def get_real_cpu_usage(self):
        """Get real CPU usage percentage from system"""
        try:
            # Get CPU usage over 1 second interval
            cpu_percent = psutil.cpu_percent(interval=1)
            return int(cpu_percent)
        except Exception as e:
            print(f"Error getting CPU usage: {e}")
            return 0

    def handle_cpu_request(self):
        """Send current CPU metrics to ESP32"""
        cpu_usage = self.get_real_cpu_usage()

        # Get additional system metrics
        try:
            memory = psutil.virtual_memory()
            disk = psutil.disk_usage('/')

            metrics = {
                "cpu": str(cpu_usage),
                "temp_c": f"{psutil.sensors_temperatures().get('coretemp', [{}])[0].get('current', 45.0):.1f}",
                "fs_free": f"{disk.free / (1024*1024):.1f}",  # MB
                "fs_used": f"{disk.used / (1024*1024):.1f}"   # MB
            }
        except Exception as e:
            print(f"Error getting system metrics: {e}")
            # Fallback to basic metrics
            metrics = {
                "cpu": str(cpu_usage),
                "temp_c": "45.0",
                "fs_free": "1024.0",
                "fs_used": "2048.0"
            }

        self.send_data(metrics)

    def get_storage_info(self):
        """Get storage drive information and capacity/free space"""
        try:
            storage_info = []
            
            # Get all disk partitions
            partitions = psutil.disk_partitions(all=False)
            
            for partition in partitions:
                try:
                    # Get usage statistics for each partition
                    usage = psutil.disk_usage(partition.mountpoint)
                    
                    # Convert bytes to GB for readability
                    total_gb = usage.total / (1024**3)
                    used_gb = usage.used / (1024**3)
                    free_gb = usage.free / (1024**3)
                    
                    # Get drive info
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
                    # Skip partitions we can't access
                    print(f"Warning: Could not access {partition.mountpoint}: {e}")
                    continue
            
            return storage_info
            
        except Exception as e:
            print(f"Error getting storage info: {e}")
            return []

    def handle_storage_request(self):
        """Send storage drive information to ESP32"""
        print("💾 Sending storage information...")
        storage_info = self.get_storage_info()
        self.send_data({"storage": storage_info})

    def generate_cpu_usage(self):
        """Generate realistic CPU usage with some variation"""
        variation = random.uniform(-self.cpu_variation, self.cpu_variation)
        cpu = self.cpu_base + variation
        # Keep within reasonable bounds
        return max(5, min(95, int(cpu)))

    def handle_screen_change(self, screen_name):
        """Handle screen change notifications from ESP32"""
        print(f"📱 Screen changed to: {screen_name}")

        if screen_name == "Alarm":
            # Send alarm list when Alarm screen opens
            time.sleep(0.1)  # Small delay to ensure screen is ready
            self.handle_alarm_request()

        # Note: CPU monitoring is now handled by MCU sending periodic requests
        # No continuous monitoring needed from server side

    def process_command(self, command_str):
        """Process incoming JSON command from ESP32"""
        try:
            command = json.loads(command_str.strip())
            print(f"📥 Received: {command}")

            # Handle different command types
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
                # ESP32 is sending alarm updates (not requesting)
                print("📝 Alarm list updated by ESP32")

            else:
                print(f"⚠️  Unknown command: {command}")

        except json.JSONDecodeError as e:
            print(f"❌ Invalid JSON received: {command_str.strip()} - {e}")
        except Exception as e:
            print(f"❌ Error processing command: {e}")

    def listen_loop(self):
        """Main listening loop for incoming serial data"""
        print("👂 Listening for ESP32 commands...")
        print(f"📊 Using {self.format.upper()} format for responses")
        print("Commands expected:")
        print("  - Screen changes: {'screen': 'Alarm'} or {'screen': 'Call'}")
        print("  - Direct requests: {'request': 'alarms'}, {'request': 'cpu'}, {'request': 'storage'}")
        print("")

        buffer = ""
        while self.running:
            try:
                if self.serial_conn and self.serial_conn.is_open:
                    # Read available data
                    if self.serial_conn.in_waiting > 0:
                        data = self.serial_conn.read(self.serial_conn.in_waiting).decode('utf-8', errors='ignore')
                        buffer += data

                        # Process complete lines (commands end with newline)
                        while '\n' in buffer:
                            line_end = buffer.find('\n')
                            line = buffer[:line_end].strip()
                            buffer = buffer[line_end + 1:]

                            if line:  # Skip empty lines
                                self.process_command(line)

                time.sleep(0.01)  # Small delay to prevent busy waiting

            except serial.SerialException as e:
                print(f"❌ Serial error: {e}")
                break
            except KeyboardInterrupt:
                print("\n🛑 Interrupted by user")
                break
            except Exception as e:
                print(f"❌ Unexpected error: {e}")
                break

    def run(self):
        """Main run method"""
        print("🚀 ESP32 Smart Display Simulator Server")
        print("=" * 50)

        if not self.connect():
            return

        self.running = True

        try:
            self.listen_loop()
        except KeyboardInterrupt:
            print("\n🛑 Shutting down...")
        finally:
            self.running = False
            self.disconnect()

def main():
    parser = argparse.ArgumentParser(description="ESP32 Smart Display Simulator Server")
    parser.add_argument('port', nargs='?', default='COM3', help='Serial port (default: COM3)')
    parser.add_argument('baudrate', nargs='?', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--format', choices=['json', 'msgpack'], default='msgpack', 
                       help='Data format (default: msgpack)')

    args = parser.parse_args()

    simulator = ESP32Simulator(args.port, args.baudrate, args.format)
    simulator.run()

if __name__ == "__main__":
    main()