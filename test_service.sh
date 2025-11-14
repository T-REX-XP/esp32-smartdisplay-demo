#!/bin/bash

# Test script for ESP32 simulator service
echo "Testing ESP32 Simulator Service"
echo "================================"

# Check if service is running
if sudo systemctl is-active --quiet esp32-simulator; then
    echo "✅ Service is running"
else
    echo "❌ Service is not running"
    echo "Start with: sudo systemctl start esp32-simulator"
    exit 1
fi

# Check CPU monitoring
echo ""
echo "Testing CPU monitoring..."
echo "Current CPU usage:"
psutil is available, checking CPU usage...
timeout 5 python3 -c "
import psutil
import time
for i in range(3):
    cpu = psutil.cpu_percent(interval=1)
    print(f'CPU Usage: {cpu}%')
"

echo ""
echo "Service logs (last 10 lines):"
sudo journalctl -u esp32-simulator -n 10 --no-pager

echo ""
echo "To monitor continuously: sudo journalctl -u esp32-simulator -f"