# ESP32 Simulator Web UI

A comprehensive web interface for testing and interacting with the ESP32 Smart Display Simulator. This tool provides a sandbox environment to test all simulator functionalities through an intuitive web interface.

## Features

### 🔌 Connection Management
- Connect/disconnect from serial ports
- Configure baud rate and data format (JSON/MessagePack)
- Real-time connection status

### 📱 Screen Navigation Testing
- Send screen change commands to simulate ESP32 navigation
- Test all available screens: Clock, Weather, Alarm, Chat, Music Player, Call, Splash

### 📊 Data Request Testing
- Request alarm lists from the simulator
- Get real-time CPU usage and system metrics
- Retrieve storage drive information and capacity

### 💬 Real-time Communication
- Live log display of all serial communications
- Response viewer for incoming data from ESP32
- Server-sent events for instant updates

### 🛠️ Custom Commands
- Send custom JSON commands to the simulator
- Test edge cases and custom functionality
- Command history and validation

## Installation

1. **Install Python dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

2. **Run the web UI:**
   ```bash
   python esp32_simulator_webui.py [COM_PORT] [BAUD_RATE] [--format json|msgpack]
   ```

   **Parameters:**
   - `COM_PORT`: Serial port (default: COM3 on Windows, /dev/ttyUSB0 on Linux)
   - `BAUD_RATE`: Baud rate (default: 115200)
   - `--format`: Data format for responses (default: msgpack)
   - `--web-port`: Web server port (default: 5000)

   **Examples:**
   ```bash
   # Windows default
   python esp32_simulator_webui.py

   # Linux with custom port
   python esp32_simulator_webui.py /dev/ttyUSB0 115200 --web-port 8080

   # Use JSON format instead of MessagePack
   python esp32_simulator_webui.py COM4 115200 --format json
   ```

3. **Open your browser:**
   Navigate to `http://localhost:5000` (or the port you specified)

## Usage

### 1. Connect to Serial Port
- Enter your ESP32's serial port (COM3, /dev/ttyUSB0, etc.)
- Select appropriate baud rate (usually 115200)
- Choose data format (MessagePack is recommended for ESP32)
- Click "Connect"

### 2. Test Screen Navigation
- Use the "Screen Navigation" buttons to simulate screen changes
- Watch the logs to see commands being sent
- Observe responses in the "Responses from ESP32" panel

### 3. Test Data Requests
- Click "Get Alarms" to request the alarm list
- Click "Get CPU Metrics" to see real-time system CPU usage
- Click "Get Storage Info" to view drive information

### 4. Send Custom Commands
- Enter any valid JSON command in the custom command field
- Examples:
  ```json
  {"screen": "Clock"}
  {"request": "cpu"}
  {"request": "alarms"}
  ```

### 5. Monitor Communication
- All sent commands appear in the "Communication Logs"
- All received responses appear in the "Responses from ESP32" panel
- Use "Clear Logs" and "Clear Responses" to clean up the displays

## Simulator Features Tested

### Alarm Management
- Request current alarm list
- View alarm times, labels, and enabled status
- Test alarm screen navigation

### System Monitoring
- Real CPU usage percentage
- System temperature (if available)
- Memory usage statistics
- Storage drive information

### Screen Navigation
- Clock screen with animations
- Weather information display
- Alarm management interface
- Chat functionality
- Music player controls
- Call interface
- Splash screen

## Architecture

The web UI consists of:

1. **Flask Backend** (`esp32_simulator_webui.py`):
   - Serial communication with ESP32 simulator
   - REST API endpoints for web interface
   - Server-sent events for real-time updates
   - Command processing and response handling

2. **Web Frontend** (`templates/index.html`):
   - Modern responsive Bootstrap interface
   - Real-time log and response displays
   - Command buttons and custom input
   - Connection management controls

3. **Data Flow**:
   ```
   Web UI → Flask API → Serial Port → ESP32 Simulator
   ESP32 Simulator → Serial Port → Flask API → Web UI (SSE)
   ```

## Troubleshooting

### Connection Issues
- **Port not found**: Check device manager (Windows) or `ls /dev/tty*` (Linux) for correct port
- **Permission denied**: On Linux, you may need to add user to dialout group: `sudo usermod -a -G dialout $USER`
- **Port busy**: Close other serial terminal applications

### No Responses
- Ensure ESP32 simulator is running and connected to the same serial port
- Check baud rate matches between web UI and simulator
- Verify data format (JSON vs MessagePack) consistency

### Web Interface Issues
- Clear browser cache if interface doesn't load properly
- Check that port 5000 (or specified port) is not in use
- Ensure all Python dependencies are installed

## Development

The web UI is built with:
- **Flask**: Lightweight Python web framework
- **Bootstrap 5**: Responsive CSS framework
- **Server-Sent Events**: Real-time communication
- **Font Awesome**: Icons and visual elements

## License

This project is part of the ESP32 Smart Display Demo and follows the same licensing terms.