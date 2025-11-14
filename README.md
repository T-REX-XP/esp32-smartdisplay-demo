# Bare minimum sketch for the Sunton aka Cheap Yellow Display (CYD) boards. ESP32_1732S019N/C, 2424S012N/C, 2432S024R/C/N, 2432S028R, 2432S032N/R/C, 3248S035R/C, 4827S043R/C, 4848S040C, 8048S050N/C and 8048S070N/C

For [PlatformIO](https://platformio.org/)

[![Platform IO CI](https://github.com/rzeldent/esp32-smartdisplay-demo/actions/workflows/main.yml/badge.svg)](https://github.com/rzeldent/esp32-smartdisplay-demo/actions/workflows/main.yml)

This is a demo application for the [esp32-smartdisplay](https://github.com/rzeldent/esp32-smartdisplay) library that is intended to be used in [PlatformIO](https://platformio.org/).
See [https://github.com/rzeldent/esp32-smartdisplay](https://github.com/rzeldent/esp32-smartdisplay/) for more information about the driver library.

> [!WARNING]
> Do not forget to clone this repository with submodules: `git clone --recurse-submodules`!
> This is required to also have the latest version of the [Sunton boards definitions](https://github.com/rzeldent/platformio-espressif32-sunton).

Example with sound! (if WiFi credentials are provided and speaker attached)

![Example](assets/PXL_20231130_225143662.jpg)

## Version history

- December 2024
  - Use EEZ Studio for designing the GUI [https://www.envox.eu/studio/studio-introduction/](https://www.envox.eu/studio/studio-introduction/)
- August 2024
  - LVGL 9.2
  - New boards
- July 2024
  - LVGL 9.1
  - Use release 2.0.10
- June 2024
  - Update SquareLine project to 1.4.1
- July 2024
  - LVGL 9.1
  - Use release 2.0.10
- June 2024
  - Update SquareLine project to 1.4.1
- March 2024
  - Added rotate button
  - Removed radio (and wifi dependencies)
- December 2023
  - Release 2.0.0
  - Updated demo with sound
- November 2023
  - Use of new library
  - Updated demo application with sound
- March 2023
  - Demo application created
- October 2023
  - Updated UI using [SquareLine Studio](https://squareline.io). This is a graphical UI design tool.

## ESP32 OLED Menu System

This project includes an ESP32 OLED menu system that provides a navigation interface for displaying system metrics and managing device functions.

### Overview

The menu system replaces the default LVGL UI with a custom menu interface that displays:
- **Overview**: System status, temperature, CPU usage, free memory, and uptime
- **Temperature**: External and internal temperature readings
- **Storage**: Flash size, free space, heap, and PSRAM information  
- **Power**: Power off functionality

### Hardware Requirements

#### Buttons
The system uses three GPIO pins for navigation:

```cpp
#define BTN_UP 0      // GPIO0 (BOOT button - built-in)
#define BTN_DOWN 35   // GPIO35 (external button required)
#define BTN_SELECT 34 // GPIO34 (external button required)
```

**Button Configuration:**
- **UP**: Navigate to previous menu item (wraps around)
- **DOWN**: Navigate to next menu item (wraps around)  
- **SELECT**: Execute action (only works in Power menu)

#### Display
- Uses the existing LVGL display (240x320 for ESP32-2432S022C)
- No additional OLED hardware required
- Green title text, white content, gray status bar

### Usage

#### Basic Navigation
1. **Navigate menus**: Use UP/DOWN buttons to cycle through 4 menu screens
2. **View information**: Each screen displays different system metrics
3. **Power off**: Navigate to Power menu and press SELECT to shutdown

#### External Metrics
The menu system can receive metrics via Serial communication:

```json
{"temp_c":"45","cpu":"23","fs_free":"1.5","fs_used":"2.8"}
```

**Supported Metrics:**
- `temp_c`: External temperature in Celsius (string)
- `cpu`: CPU usage percentage (string)
- `fs_free`: Free filesystem space in MB (string)
- `fs_used`: Used filesystem space in MB (string)

## UART Commands

The device supports various commands via UART (115200 baud) for remote control and data updates. All commands are JSON-formatted and must end with a newline.

### Screen Navigation

Switch between UI screens remotely:

```json
{"screen": "ScreenName"}
```

**Supported Screens:**
- `"Clock"`: Main clock display
- `"Weather"`: Weather information
- `"Alarm"`: Alarm management
- `"Chat"`: Chat interface
- `"Music"`: Music player
- `"Call"`: CPU load gauge
- `"Splash"`: Splash screen

**Example:**
```json
{"screen": "Weather"}
```

### CPU Load Gauge

The Call screen now displays a real-time CPU usage gauge that updates automatically when metrics are received:

- Visual analog meter with needle indicator (0-100%)
- Percentage label below the gauge
- Updates in real-time as `cpu` metric values are sent via UART
- Smooth needle animation for better visual feedback

**Example CPU Update:**
```json
{"cpu": "75"}
```

### Alarm Management

Update the alarm list dynamically:

```json
{"alarms": [
  {"time": "08:00", "label": "Breakfast", "enabled": true},
  {"time": "14:30", "label": "Meeting", "enabled": false}
]}
```

**Alarm Properties:**
- `time`: Alarm time in "HH:MM" format (string)
- `label`: Alarm description/label (string)
- `enabled`: Whether the alarm is active (boolean)

**Notes:**
- Sending an empty array `{"alarms": []}` clears all alarms
- The Alarm screen updates automatically if currently active
- Displays "No alarm" when the list is empty

### Metrics Update

Send system metrics for display:

```json
{"temp_c": "42.5", "cpu": "15", "fs_free": "1.2", "fs_used": "2.8"}
```

**Supported Metrics:**
- `temp_c`: External temperature in Celsius (string)
- `cpu`: CPU usage percentage (string) - *Used for CPU gauge on Call screen*
- `fs_free`: Free filesystem space in MB (string)
- `fs_used`: Used filesystem space in MB (string)

### Payload Examples

#### Standard Metrics Update
```json
{"temp_c":"42.5","cpu":"15","fs_free":"1.2","fs_used":"2.8"}
```

#### Partial Metrics (will merge with existing data)
```json
{"temp_c":"38.0"}
```

#### Power Command (sent by device)
```json
{"cmd":"POWEROFF"}
```

### Communication Protocol

#### Receiving Commands
Send JSON objects via Serial (115200 baud) ending with newline:

```bash
# Update metrics
echo '{"temp_c":"45","cpu":"30"}' > /dev/ttyUSB0

# Switch screen
echo '{"screen":"Weather"}' > /dev/ttyUSB0

# Update alarms
echo '{"alarms":[{"time":"08:00","label":"Breakfast","enabled":true}]}' > /dev/ttyUSB0
```

#### Automatic Updates
- The system requests simulated metrics every 1 second
- Real metrics should be sent via Serial when available
- Display updates every 250ms
- Screen navigation and alarm updates are processed immediately

### Integration Examples

#### Python Example
```python
import serial
import json
import time

ser = serial.Serial('/dev/ttyUSB0', 115200)

while True:
    # Send metrics
    metrics = {
        "temp_c": str(45 + time.time() % 10),
        "cpu": str(int(20 + time.time() % 80)),
        "fs_free": "1.5",
        "fs_used": "2.8"
    }
    ser.write((json.dumps(metrics) + "\n").encode())
    
    # Switch to Weather screen
    ser.write(b'{"screen": "Weather"}\n')
    
    # Update alarms
    alarms = {
        "alarms": [
            {"time": "08:00", "label": "Breakfast", "enabled": True},
            {"time": "14:30", "label": "Meeting", "enabled": False}
        ]
    }
    ser.write((json.dumps(alarms) + "\n").encode())
    
    time.sleep(5)
```

#### Arduino Example
```cpp
#include <ArduinoJson.h>

void sendMetrics() {
    JsonDocument doc;
    doc["temp_c"] = String(temperatureSensor.read());
    doc["cpu"] = String(getCpuUsage());
    doc["fs_free"] = String(getFreeSpace());
    
    Serial.println(doc.as<String>());
}

void switchToWeather() {
    JsonDocument doc;
    doc["screen"] = "Weather";
    Serial.println(doc.as<String>());
}

void updateAlarms() {
    JsonDocument doc;
    JsonArray alarms = doc["alarms"].to<JsonArray>();
    
    JsonObject alarm1 = alarms.createNestedObject();
    alarm1["time"] = "08:00";
    alarm1["label"] = "Breakfast";
    alarm1["enabled"] = true;
    
    JsonObject alarm2 = alarms.createNestedObject();
    alarm2["time"] = "14:30";
    alarm2["label"] = "Meeting";
    alarm2["enabled"] = false;
    
    Serial.println(doc.as<String>());
}
```

### Display Layout

```
┌─────────────────────────┐
│    ESP32 Status          │  ← Title (green)
│                         │
│  Temp: 42 C             │
│  CPU: 15%               │  ← Content (white)
│  Free: 1.2 MB           │
│  Uptime: 12345 ms       │
│                         │
│ Menu: 1/4 | WiFi: Disconnected │  ← Status (gray)
└─────────────────────────┘
```

### Configuration

#### Button Pin Changes
Modify the pin definitions in `main.cpp`:

```cpp
#define BTN_UP 0      // Change if needed
#define BTN_DOWN 4    // Change to available pin
#define BTN_SELECT 2  // Change to available pin
```

#### Update Intervals
Adjust timing in the `loop()` function:

```cpp
// Button check debounce (ms)
if(now - lastButtonCheck > 150) {

// Display refresh rate (ms)
if(now - lastRender > 250) {

// Metrics request interval (ms)
if(millis() - lastReq > 1000) {
```

### Troubleshooting

#### Buttons Not Working
1. Check GPIO pin availability for your board
2. Ensure buttons are wired to GND (active low)
3. Verify pin definitions match hardware

#### Display Issues
1. Ensure LVGL fonts are available (uses `lv_font_montserrat_14`)
2. Check display initialization in `smartdisplay_init()`
3. Verify screen dimensions match your board

#### Serial Communication
1. Use 115200 baud rate
2. End JSON messages with newline (`\n`)
3. Validate JSON syntax before sending

### Dependencies

- **ArduinoJson 7.x**: For JSON parsing and serialization
- **ArduinoOTA**: For over-the-air firmware updates (built-in)
- **WiFi.h**: For WiFi connectivity and OTA
- **esp32-smartdisplay**: For display and hardware integration

Add to `platformio.ini`:
```ini
lib_deps =
    https://github.com/rzeldent/esp32-smartdisplay
    bblanchon/ArduinoJson@^7.0.4
```

### Board Compatibility

Tested with:
- ESP32-2432S022C (240x320 display)
- Other ESP32 smartdisplay boards (adjust pin definitions as needed)
