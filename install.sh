#!/bin/bash

# ESP32 Smart Display Simulator Installer
# This script installs the ESP32 simulator as a Linux service

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERVICE_NAME="esp32-simulator"
SERVICE_USER="esp32sim"
INSTALL_DIR="/opt/esp32-simulator"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
SERIAL_PORT="/dev/ttyUSB0"  # Default serial port

echo -e "${BLUE}ESP32 Smart Display Simulator Installer${NC}"
echo "========================================"

# Check if running as root
if [[ $EUID -eq 0 ]]; then
   echo -e "${RED}This script should not be run as root. Please run as a regular user with sudo access.${NC}"
   exit 1
fi

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for required tools
echo -e "${YELLOW}Checking system requirements...${NC}"

if ! command -v python3 >/dev/null 2>&1; then
    echo -e "${RED}Python 3 is not installed. Installing...${NC}"
    sudo apt-get update
    sudo apt-get install -y python3 python3-pip
fi

if ! command -v pip3 >/dev/null 2>&1; then
    echo -e "${RED}pip3 is not installed. Installing...${NC}"
    sudo apt-get install -y python3-pip
fi

# Install Python dependencies
echo -e "${YELLOW}Installing Python dependencies...${NC}"
pip3 install --user pyserial psutil

# Create service user
echo -e "${YELLOW}Creating service user...${NC}"
if ! id "$SERVICE_USER" &>/dev/null; then
    sudo useradd --system --shell /bin/false --home-dir "$INSTALL_DIR" --create-home "$SERVICE_USER"
    echo -e "${GREEN}Created service user: $SERVICE_USER${NC}"
else
    echo -e "${BLUE}Service user $SERVICE_USER already exists${NC}"
fi

# Create installation directory
echo -e "${YELLOW}Creating installation directory...${NC}"
sudo mkdir -p "$INSTALL_DIR"
sudo chown "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR"

# Copy files to installation directory
echo -e "${YELLOW}Installing simulator files...${NC}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
sudo cp "$SCRIPT_DIR/esp32_simulator.py" "$INSTALL_DIR/"
sudo cp "$SCRIPT_DIR/esp32_simulator.service.template" "$INSTALL_DIR/"
sudo chown -R "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR"

# Configure serial port
echo -e "${YELLOW}Configuring serial port...${NC}"
read -p "Enter the serial port for ESP32 (default: $SERIAL_PORT): " user_port
if [[ -n "$user_port" ]]; then
    SERIAL_PORT="$user_port"
fi

# Check if serial port exists
if [[ ! -e "$SERIAL_PORT" ]]; then
    echo -e "${YELLOW}Warning: Serial port $SERIAL_PORT does not exist yet.${NC}"
    echo -e "${YELLOW}Make sure to connect your ESP32 device.${NC}"
fi

# Add user to dialout group for serial access
echo -e "${YELLOW}Adding user to dialout group for serial access...${NC}"
sudo usermod -a -G dialout "$SERVICE_USER"
sudo usermod -a -G dialout "$USER"

# Create systemd service file
echo -e "${YELLOW}Creating systemd service...${NC}"
cat > /tmp/esp32-simulator.service << EOF
[Unit]
Description=ESP32 Smart Display Simulator
After=network.target
Wants=network.target

[Service]
Type=simple
User=$SERVICE_USER
Group=$SERVICE_USER
WorkingDirectory=$INSTALL_DIR
ExecStart=/usr/bin/python3 $INSTALL_DIR/esp32_simulator.py $SERIAL_PORT
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
SyslogIdentifier=esp32-simulator

# Security settings
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ReadWritePaths=$INSTALL_DIR
ProtectHome=yes

[Install]
WantedBy=multi-user.target
EOF

sudo mv /tmp/esp32-simulator.service "$SERVICE_FILE"
sudo chmod 644 "$SERVICE_FILE"

# Reload systemd
echo -e "${YELLOW}Reloading systemd...${NC}"
sudo systemctl daemon-reload

# Enable and start service
echo -e "${YELLOW}Enabling and starting service...${NC}"
sudo systemctl enable "$SERVICE_NAME"
sudo systemctl start "$SERVICE_NAME"

# Check service status
echo -e "${YELLOW}Checking service status...${NC}"
sleep 2
if sudo systemctl is-active --quiet "$SERVICE_NAME"; then
    echo -e "${GREEN}✅ Service is running successfully!${NC}"
else
    echo -e "${RED}❌ Service failed to start. Check logs with: sudo journalctl -u $SERVICE_NAME${NC}"
fi

echo ""
echo -e "${GREEN}Installation completed!${NC}"
echo "=============================="
echo -e "Service name: ${BLUE}$SERVICE_NAME${NC}"
echo -e "Installation directory: ${BLUE}$INSTALL_DIR${NC}"
echo -e "Serial port: ${BLUE}$SERIAL_PORT${NC}"
echo ""
echo "Management commands:"
echo -e "  Start service:   ${BLUE}sudo systemctl start $SERVICE_NAME${NC}"
echo -e "  Stop service:    ${BLUE}sudo systemctl stop $SERVICE_NAME${NC}"
echo -e "  Restart service: ${BLUE}sudo systemctl restart $SERVICE_NAME${NC}"
echo -e "  Check status:    ${BLUE}sudo systemctl status $SERVICE_NAME${NC}"
echo -e "  View logs:       ${BLUE}sudo journalctl -u $SERVICE_NAME -f${NC}"
echo ""
echo -e "${YELLOW}Note: You may need to reconnect your ESP32 device if the serial port changes.${NC}"
echo -e "${YELLOW}To change the serial port, edit $SERVICE_FILE and restart the service.${NC}"