#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp32_smartdisplay.h>
#include <vector>

#ifdef ROUTER_UI
#include "router/router_app.h"
#else
#include <ui/ui.h>

// Alarm structure
struct Alarm {
    String time;
    String label;
    bool enabled;
};

// Global alarms list
std::vector<Alarm> alarms;
#endif

#ifndef ROUTER_UI

// Define button pins for navigation (using available GPIOs)
// Note: GPIO34 and GPIO35 are input-only and don't have pull-ups
// Using only BOOT button for now to avoid false triggers
#define BTN_UP 0      // GPIO0 (BOOT button)
#define BTN_DOWN 0    // GPIO0 (same as UP for now)
#define BTN_SELECT 0  // GPIO0 (same as UP for now)

// Display dimensions
#define SCREEN_WIDTH DISPLAY_WIDTH
#define SCREEN_HEIGHT DISPLAY_HEIGHT

// Menu state
uint8_t menuIndex = 0; // 0: Overview, 1: Temperature, 2: Storage, 3: Power
uint8_t detailPage = 0; // Sub-page within each menu
unsigned long lastMetricsMs = 0;
JsonDocument metrics;

// Page counts for each menu
const uint8_t MENU_PAGE_COUNTS[] = {1, 2, 3, 1}; // Overview:1, Temp:2, Storage:3, Power:1
const char* MENU_PAGE_NAMES[][4] = {
    {"Overview"},
    {"Temperature", "Internal Temp"},
    {"Storage", "Memory", "Flash Info"},
    {"Power"}
};

// Touch/swipe handling
static lv_coord_t touch_start_x = 0;
static lv_coord_t touch_start_y = 0;
static bool touch_active = false;
static unsigned long touch_start_time = 0;

// LVGL objects for menu display (legacy - not used in new UI)
static lv_obj_t *menu_screen = nullptr;
static lv_obj_t *menu_title = nullptr;
static lv_obj_t *menu_content = nullptr;
static lv_obj_t *menu_status = nullptr;
static lv_obj_t *page_indicator = nullptr;

// Gauge and meter objects (legacy - not used in new UI)
// static lv_obj_t *temp_gauge = nullptr;
// static lv_obj_t *cpu_meter = nullptr;
// static lv_obj_t *memory_chart = nullptr;
// static lv_obj_t *storage_arc = nullptr;
// static lv_obj_t *network_bar = nullptr;

// Function declarations
void drawHeader(const char* title);
void updatePageIndicator();
void drawOverview();
void drawTemp();
void drawStorage();
void drawPower();
void render();
void createMenuScreen();
void touch_event_cb(lv_event_t *e);
void navigateNextPage();
void navigatePrevPage();

// Simple button handler
bool readBtn(int pin) { 
    if (pin == 0) {
        // BOOT button is active low
        return digitalRead(pin) == LOW; 
    }
    return digitalRead(pin) == LOW; 
}

// Touch event handler for swipe support
void touch_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_get_act(), &point);
        touch_start_x = point.x;
        touch_start_y = point.y;
        touch_active = true;
        touch_start_time = millis();
    } 
    else if (code == LV_EVENT_RELEASED) {
        if (touch_active) {
            lv_point_t point;
            lv_indev_get_point(lv_indev_get_act(), &point);
            
            lv_coord_t dx = point.x - touch_start_x;
            lv_coord_t dy = point.y - touch_start_y;
            unsigned long touch_duration = millis() - touch_start_time;
            
            // Check if it was a quick horizontal swipe (min 50px, max 500ms)
            if (abs(dx) > 50 && touch_duration < 500 && abs(dy) < 100) {
                if (dx > 0) {
                    // Swipe right - previous page
                    navigatePrevPage();
                } else {
                    // Swipe left - next page
                    navigateNextPage();
                }
            }
            
            touch_active = false;
        }
    }
}

// Navigation functions
void navigateNextPage() {
    uint8_t maxPages = MENU_PAGE_COUNTS[menuIndex];
    if (detailPage < maxPages - 1) {
        detailPage++;
    } else {
        // Move to next menu if at last page
        if (menuIndex < 3) {
            menuIndex++;
            detailPage = 0;
        }
    }
    render();
}

void navigatePrevPage() {
    if (detailPage > 0) {
        detailPage--;
    } else {
        // Move to previous menu if at first page
        if (menuIndex > 0) {
            menuIndex--;
            detailPage = MENU_PAGE_COUNTS[menuIndex] - 1;
        }
    }
    render();
}

void drawHeader(const char* title) {
    if (menu_title) {
        lv_label_set_text(menu_title, title);
    }
}

void drawOverview() {
    drawHeader("ESP32 Status");
    if (menu_content) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), 
            "Temp: %s C\nCPU: %s%%\nFree: %s MB\nUptime: %lu ms\n\nSwipe left/right\nto navigate",
            metrics["temp_c"].as<const char*>() ?: "--",
            metrics["cpu"].as<const char*>() ?: "--", 
            metrics["fs_free"].as<const char*>() ?: "--",
            millis());
        lv_label_set_text(menu_content, buffer);
    }
}

void drawTemp() {
    if (detailPage == 0) {
        drawHeader("Temperature");
        if (menu_content) {
            char buffer[128];
            snprintf(buffer, sizeof(buffer), 
                "CPU: %s C\n\nSwipe left for\nInternal Temp",
                metrics["temp_c"].as<const char*>() ?: "--");
            lv_label_set_text(menu_content, buffer);
        }
    } else {
        drawHeader("Internal Temp");
        if (menu_content) {
            char buffer[128];
            snprintf(buffer, sizeof(buffer), 
                "Internal Sensor:\n%.1f°C\n\nSwipe right for\nExternal Temp",
                temperatureRead());
            lv_label_set_text(menu_content, buffer);
        }
    }
}

void drawStorage() {
    drawHeader("Storage");
    if (menu_content) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), 
            "Flash Size: %d MB\nFree Flash: %d KB\nFree Heap: %d KB\nFree PSRAM: %d KB",
            ESP.getFlashChipSize() / (1024 * 1024),
            ESP.getFreeSketchSpace() / 1024,
            ESP.getFreeHeap() / 1024,
            ESP.getPsramSize() / 1024);
        lv_label_set_text(menu_content, buffer);
    }
}

void drawPower() {
    drawHeader("Power");
    if (menu_content) {
        lv_label_set_text(menu_content, "Hold BOOT button\nfor 2 seconds\nto Power OFF\n\nSwipe or press\nto navigate");
    }
}

void render() {
    switch(menuIndex) {
        case 0: drawOverview(); break;
        case 1: drawTemp(); break;
        case 2: drawStorage(); break;
        case 3: drawPower(); break;
    }
    
    // Update status bar with page info
    if (menu_status) {
        char status[64];
        uint8_t totalPages = MENU_PAGE_COUNTS[menuIndex];
        if (totalPages > 1) {
            snprintf(status, sizeof(status), "Menu: %d/4 | Page: %d/%d | WiFi: %s", 
                     menuIndex + 1, detailPage + 1, totalPages,
                     WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
        } else {
            snprintf(status, sizeof(status), "Menu: %d/4 | WiFi: %s", 
                     menuIndex + 1, WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
        }
        lv_label_set_text(menu_status, status);
    }
    
    // Update page indicator
    updatePageIndicator();
}

void updatePageIndicator() {
    // Update page indicator with current page info
    if (page_indicator) {
        uint8_t totalPages = MENU_PAGE_COUNTS[menuIndex];
        char page_text[32];
        if (totalPages > 1) {
            snprintf(page_text, sizeof(page_text), "Page %d/%d", detailPage + 1, totalPages);
        } else {
            snprintf(page_text, sizeof(page_text), "Page 1/1");
        }
        lv_label_set_text(page_indicator, page_text);
    }
}

void createMenuScreen() {
    // Create a new screen for the menu
    menu_screen = lv_obj_create(nullptr);
    
    // Add touch event callback to the screen
    lv_obj_add_event_cb(menu_screen, touch_event_cb, LV_EVENT_ALL, nullptr);
    
    // Create title label at top
    menu_title = lv_label_create(menu_screen);
    lv_obj_set_style_text_font(menu_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(menu_title, lv_color_hex(0x00ff00), 0);
    lv_obj_align(menu_title, LV_ALIGN_TOP_MID, 0, 10);
    lv_label_set_text(menu_title, "ESP32 Status");
    
    // Create page indicator below title
    page_indicator = lv_label_create(menu_screen);
    lv_obj_set_style_text_font(page_indicator, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(page_indicator, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(page_indicator, LV_ALIGN_TOP_MID, 0, 35);
    lv_label_set_text(page_indicator, "Page 1/1");
    
    // Create content area
    menu_content = lv_label_create(menu_screen);
    lv_obj_set_style_text_font(menu_content, &lv_font_montserrat_14, 0);
    lv_obj_set_width(menu_content, SCREEN_WIDTH - 20);
    lv_obj_align(menu_content, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(menu_content, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(menu_content, LV_TEXT_ALIGN_CENTER, 0);
    
    // Create status bar at bottom
    menu_status = lv_label_create(menu_screen);
    lv_obj_set_style_text_font(menu_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(menu_status, lv_color_hex(0x888888), 0);
    lv_obj_align(menu_status, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(menu_status, "Menu: 1/4 | WiFi: Disconnected");
    
    // Load the menu screen
    lv_scr_load(menu_screen);
}

void requestMetrics() {
    // Send requests to simulator for real metrics data
    static unsigned long lastCpuRequest = 0;
    static unsigned long lastStorageRequest = 0;
    unsigned long now = millis();

    // Request CPU data every 1.5 seconds ONLY when Call screen (CPU load screen) is active
    if (ui_Call && lv_scr_act() == ui_Call && now - lastCpuRequest > 1500) {
        Serial.println("{\"request\": \"cpu\"}");
        lastCpuRequest = now;
    }

    // Request storage data every 5 seconds
    if (now - lastStorageRequest > 5000) {
        Serial.println("{\"request\": \"storage\"}");
        lastStorageRequest = now;
    }

    // Note: Temperature is read locally from ESP32 sensor
    // CPU and storage data come from simulator
}

void handleSerial() {
    // Handle incoming serial data for metrics and alarms
    while(Serial.available()) {
        String line = Serial.readStringUntil('\n');
        if(line.length() == 0) return;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, line);
        if(!err) {
            if (!doc["alarms"].isNull()) {
                alarms.clear();
                JsonArray alarmsArray = doc["alarms"];
                for (JsonObject alarmObj : alarmsArray) {
                    Alarm a;
                    a.time = alarmObj["time"].as<String>();
                    a.label = alarmObj["label"].as<String>();
                    a.enabled = alarmObj["enabled"];
                    alarms.push_back(a);
                }
                // Refresh Alarm screen if active
                if (lv_scr_act() == ui_Alarm) {
                    // Clear existing alarm components
                    lv_obj_clean(ui_Alarm_container);
                    // Recreate ui_Set_alarm
                    ui_Set_alarm = ui_Small_Label_create(ui_Alarm_container);
                    lv_obj_set_x(ui_Set_alarm, 0);
                    lv_obj_set_y(ui_Set_alarm, 17);
                    lv_obj_set_align(ui_Set_alarm, LV_ALIGN_TOP_MID);
                    lv_label_set_text(ui_Set_alarm, "Set alarm");
                    lv_obj_set_style_text_color(ui_Set_alarm, lv_color_hex(0x000746), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_opa(ui_Set_alarm, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    // Recreate dynamic alarms
                    int y_pos = 43;
                    for (size_t i = 0; i < alarms.size(); ++i) {
                        lv_obj_t *alarm_comp = ui_Alarm_Comp_create(ui_Alarm_container);
                        lv_obj_set_x(alarm_comp, 0);
                        lv_obj_set_y(alarm_comp, y_pos);
                        lv_label_set_text(ui_comp_get_child(alarm_comp, UI_COMP_ALARM_COMP_ALARM_NUM2), alarms[i].time.c_str());
                        lv_label_set_text(ui_comp_get_child(alarm_comp, UI_COMP_ALARM_COMP_PERIOD), alarms[i].label.c_str());
                        if (alarms[i].enabled) {
                            lv_obj_add_state(ui_comp_get_child(alarm_comp, UI_COMP_ALARM_COMP_SWITCH1), LV_STATE_CHECKED);
                        } else {
                            lv_obj_clear_state(ui_comp_get_child(alarm_comp, UI_COMP_ALARM_COMP_SWITCH1), LV_STATE_CHECKED);
                        }
                        y_pos += 85;
                    }
                    if (alarms.empty()) {
                        ui_No_alarm = ui_Small_Label_create(ui_Alarm_container);
                        lv_obj_set_x(ui_No_alarm, 0);
                        lv_obj_set_y(ui_No_alarm, 43);
                        lv_obj_set_align(ui_No_alarm, LV_ALIGN_TOP_MID);
                        lv_label_set_text(ui_No_alarm, "No alarm");
                        lv_obj_set_style_text_color(ui_No_alarm, lv_color_hex(0x000746), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_text_opa(ui_No_alarm, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                }
            } else if (!doc["screen"].isNull()) {
                String screen = doc["screen"];
                if (screen == "Clock") {
                    _ui_screen_change(&ui_Clock, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Clock_screen_init);
                } else if (screen == "Weather") {
                    _ui_screen_change(&ui_Weather, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Weather_screen_init);
                } else if (screen == "Alarm") {
                    _ui_screen_change(&ui_Alarm, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Alarm_screen_init);
                } else if (screen == "Chat") {
                    _ui_screen_change(&ui_Chat, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Chat_screen_init);
                } else if (screen == "Music") {
                    _ui_screen_change(&ui_Music_Player, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Music_Player_screen_init);
                } else if (screen == "Splash") {
                    _ui_screen_change(&ui_Splash, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Splash_screen_init);
                }
            } else {
                // Assume it's metrics
                metrics = doc;
                lastMetricsMs = millis();
                
                // Update CPU gauge if Call screen is active and CPU data is available
                if (ui_Call && lv_scr_act() == ui_Call && !doc["cpu"].isNull()) {
                    ui_update_cpu_gauge();
                }
            }
        }
    }
}

void sendPoweroff() {
    JsonDocument cmd;
    cmd["cmd"] = "POWEROFF";
    String out;
    serializeJson(cmd, out);
    Serial.println(out);
    
    // Actually power down (deep sleep)
    ESP.deepSleep(0);
}

#endif /* !ROUTER_UI */

extern "C" void action_on_rotate(lv_event_t *e)
{
    auto disp = lv_disp_get_default();
    auto rotation = (lv_display_rotation_t)((lv_disp_get_rotation(disp) + 1) % (LV_DISPLAY_ROTATION_270 + 1));
    lv_display_set_rotation(disp, rotation);
}

void setup()
{
#ifdef ARDUINO_USB_CDC_ON_BOOT
    delay(5000);
#endif

    // IMMEDIATELY disable auto-reset before any serial operations
    pinMode(0, OUTPUT);
    digitalWrite(0, HIGH);
    
    // Additional reset prevention - set multiple GPIOs if needed
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);  // TX pin low initially    // Minimal delay for reset prevention
    delay(100);

    Serial.begin(115200);
    Serial.setTimeout(1000);
    Serial.setDebugOutput(false);

    // Keep GPIO0 HIGH throughout initialization
    digitalWrite(0, HIGH);

    // Quick stabilization - much shorter delay
    delay(500);

    // Clear any pending serial data quickly (non-blocking approach)
    unsigned long clearStart = millis();
    while(Serial.available() && (millis() - clearStart) < 200) {  // Max 200ms for clearing
        Serial.read();
        delay(1);
    }

    // Re-enable debug output after stabilization
    Serial.setDebugOutput(true);
    log_i("Board: %s", BOARD_NAME);
    log_i("CPU: %s rev%d, CPU Freq: %d Mhz, %d core(s)", ESP.getChipModel(), ESP.getChipRevision(), getCpuFrequencyMhz(), ESP.getChipCores());
    log_i("Free heap: %d bytes", ESP.getFreeHeap());
    log_i("Free PSRAM: %d bytes", ESP.getPsramSize());
    log_i("SDK version: %s", ESP.getSdkVersion());

#ifndef ROUTER_UI
    pinMode(BTN_UP, INPUT_PULLUP);
    delay(100);
#endif

    smartdisplay_init();

    __attribute__((unused)) auto disp = lv_disp_get_default();

#ifdef ROUTER_UI
    log_i("Router UI mode");
    router_app_init();
#else
    ui_init();
#endif
}

ulong next_millis;
auto lv_last_tick = millis();

void loop()
{
    auto const now = millis();

#ifdef ROUTER_UI
    while (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        if (line.length() > 0)
            router_app_on_serial_line(line.c_str());
    }
    router_app_loop();
#else
    // Handle serial communication
    handleSerial();
    
    // Request metrics periodically
    static unsigned long lastReq = 0; 
    if(millis() - lastReq > 1000) {
        lastReq = millis();
        requestMetrics();
    }
    
    // Handle button input with debouncing
    static unsigned long lastButtonCheck = 0;
    static bool lastBtnState = false;
    static unsigned long btnPressStart = 0;
    
    if(now - lastButtonCheck > 150) { // 150ms debounce
        lastButtonCheck = now;
        bool btnPressed = readBtn(BTN_UP);
        
        // Detect button press (transition from not pressed to pressed)
        if(btnPressed && !lastBtnState) {
            btnPressStart = now;
            if(menuIndex == 3) {
                // On power menu, require long press
            } else {
                // navigateNextPage();
            }
        }
        else if(btnPressed && lastBtnState && (now - btnPressStart > 2000)) {
            if(menuIndex == 3) {
                // sendPoweroff();
            }
        }
        
        lastBtnState = btnPressed;
    }
    
    static unsigned long lastRender = 0; 
    if(now - lastRender > 250) { 
        lastRender = now; 
    }
#endif

#ifdef BOARD_HAS_RGB_LED
    if (now > next_millis) {
        next_millis = now + 500;
        auto const rgb = (now / 2000) % 8;
        smartdisplay_led_set_rgb(rgb & 0x01, rgb & 0x02, rgb & 0x04);
    }
#endif

    // Update the ticker
    lv_tick_inc(now - lv_last_tick);
    lv_last_tick = now;
    
    // Update the UI
    lv_timer_handler();
}
