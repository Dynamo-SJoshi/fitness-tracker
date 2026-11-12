/*
 ===============================================================================
  Project: Off-Grid Biometric Ankle Tracker (Split-Node Wearable)
  Target:  ESP32 DevKit V1
  Sensors: MPU6050 (6-axis IMU), Y1708 (PPG Optical Pulse),
           BMP280 (Altimeter), DHT11/DHT22 (Microclimate Perspiration)
  
  Architecture:
  - Pod A (Main Processing Pod): Shoelaces / Lower Calf
  - Pod B (Biometric Sensor Node): Sock Elastic Cuff
  - 5-Wire Strain-Relieved Harness: VCC, GND, SDA, SCL, Signal
  
  Connectivity:
  - Autonomous Wi-Fi SoftAP ("ESP32_BioTracker", IP: 192.168.4.1)
  - Embedded Zero-Dependency Dark-Mode Mobile Web Dashboard
  - Real-Time 1 Hz JSON AJAX Streaming & Bidirectional Profile Sync
 ===============================================================================
*/

#include <Arduino.h>
#include "config.h"
#include "profile_storage.h"
#include "sensors_handler.h"
#include "web_server_handler.h"

void printBanner() {
    Serial.println(F("\n========================================================"));
    Serial.println(F("    OFF-GRID BIOMETRIC ANKLE TRACKER (SPLIT-NODE)       "));
    Serial.println(F("  Lower-Limb Pedometer, PPG, Altimeter & Hydration Hub  "));
    Serial.println(F("========================================================"));
}

void setup() {
    // 1. Initialize High-Speed Serial Monitor
    Serial.begin(115200);
    delay(500);
    printBanner();

    // 2. Initialize NVS Flash Profile Storage (Preferences.h)
    Serial.println(F("[Boot] Initializing Non-Volatile Flash Storage..."));
    if (!storage.begin()) {
        Serial.println(F("[Boot] WARNING: NVS storage initialization failed, using RAM fallback."));
    }

    // 3. Initialize Sensor Bus and Hardware Peripherals
    Serial.println(F("[Boot] Initializing Sensor Bus & Hardware..."));
    if (!sensors.begin()) {
        Serial.println(F("[Boot] WARNING: Some sensors could not be verified on I2C/ADC bus."));
    }

    // 4. Start Local Wi-Fi SoftAP and Embedded Web Server
    Serial.println(F("[Boot] Initializing Local Wi-Fi SoftAP & Web Engine..."));
    webServer.begin();

    Serial.println(F("========================================================"));
    Serial.println(F(" SYSTEM READY: Connect smartphone to Wi-Fi:"));
    Serial.printf( " SSID:     %s\n", WIFI_AP_SSID);
    Serial.printf( " Password: %s\n", WIFI_AP_PASSWORD);
    Serial.println(F(" URL:      http://192.168.4.1"));
    Serial.println(F("========================================================\n"));
}

void loop() {
    uint32_t currentMillis = millis();

    // 1. Process Bare-Metal Sensor Polling & Biometric Algorithms
    sensors.update(currentMillis);

    // 2. Process Web Server & AJAX Requests
    webServer.handleClient();

    // 3. Yield to FreeRTOS background tasks (Wi-Fi stack, watchdog)
    yield();
}
