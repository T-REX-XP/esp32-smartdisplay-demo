// Arduino ESP32 MessagePack Example for NAS Display
// Install via Arduino Library Manager: "ArduinoMsgPack" by Rob Tillaart

#include <Arduino.h>
#include <MsgPack.h>
#include <HardwareSerial.h>

// UART configuration
#define RX_PIN 16
#define TX_PIN 17
#define BAUD_RATE 115200

HardwareSerial SerialPort(2); // Use UART2 for communication

void setup() {
    Serial.begin(115200); // Debug serial
    SerialPort.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);

    Serial.println("ESP32 NAS Display - MessagePack Mode");
    Serial.println("Ready to receive data...");
}

void loop() {
    // Check for incoming MessagePack data
    if (SerialPort.available()) {
        // Read until newline (frame separator)
        String data = SerialPort.readStringUntil('\n');

        if (data.length() > 0) {
            // Parse MessagePack data
            MsgPack::Unpacker unpacker;
            MsgPack::map_t<String, MsgPack::Element> map;

            if (unpacker.feed((uint8_t*)data.c_str(), data.length())) {
                unpacker.deserialize(map);

                // Handle different data types
                if (map.count("cpu")) {
                    String cpu = map["cpu"];
                    Serial.print("CPU: ");
                    Serial.println(cpu);

                    // Update LVGL CPU gauge here
                    // lv_gauge_set_value(cpu_gauge, cpu.toInt());
                }

                if (map.count("storage")) {
                    // Handle storage array
                    Serial.println("Storage data received");
                    // Parse and display storage info
                }
            }
        }
    }

    // Send requests to server
    static unsigned long lastRequest = 0;
    if (millis() - lastRequest > 1500) { // Every 1.5 seconds
        // Send JSON command (commands stay JSON for simplicity)
        SerialPort.println("{\"request\":\"cpu\"}");
        lastRequest = millis();
    }
}