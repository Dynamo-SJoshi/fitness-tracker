#pragma once

#include <Arduino.h>

// =============================================================================
// OFF-GRID BIOMETRIC ANKLE TRACKER - SYSTEM CONFIGURATION
// =============================================================================

// --- Wi-Fi SoftAP Configuration ---
#define WIFI_AP_SSID            "ESP32_BioTracker"
#define WIFI_AP_PASSWORD        "biotracker123"      // Minimum 8 chars (or set empty for open)
#define WIFI_AP_CHANNEL         1
#define WIFI_AP_MAX_CONN        4

// --- Pin Allocations (ESP32 DevKit V1) ---
// I2C Shared Bus (MPU6050 & BMP280)
#define PIN_I2C_SDA             21
#define PIN_I2C_SCL             22

// Biometric PPG Analog In (ADC1 safe for concurrent Wi-Fi operations)
#define PIN_PPG_ANALOG          32

// Perspiration Microclimate Sensor (DHT11 or DHT22)
#define PIN_DHT_DATA            4
#define DHT_TYPE                DHT22               // Set to DHT11 or DHT22

// Onboard Status LED
#define PIN_STATUS_LED          2

// --- I2C Device Addresses ---
#define I2C_ADDR_MPU6050        0x68
#define I2C_ADDR_BMP280_ALT     0x76                // Many modules use 0x76, default Adafruit is 0x77
#define I2C_ADDR_BMP280_DEF     0x77

// --- Pedometer Algorithm Parameters (MPU6050) ---
// Human gait frequency is predominantly < 5 Hz (cadence ~ 1.5 - 2.5 steps/sec)
#define PEDO_SAMPLE_RATE_HZ         50                  // 50 Hz IMU polling (20ms interval)
#define PEDO_GRAVITY_EARTH          9.80665f            // 1g baseline in m/s^2
#define PEDO_PEAK_THRESHOLD         2.50f               // Net acceleration threshold (m/s^2) above 1g baseline
#define PEDO_HYSTERESIS_DEBOUNCE_MS 350                 // Strict 350ms lockout window to prevent false double-counts

// --- PPG / Heart Rate Parameters (Y1708) ---
#define SIMULATE_PPG_HEART_RATE     true                // Set true for physiological ankle demo simulation
#define PPG_RESTING_BASE_BPM        74.0f               // Baseline resting heart rate (BPM)
#define PPG_HOLD_AFTER_STOP_SEC     8                   // Plateau hold time after stopping (seconds)
#define PPG_RECOVERY_DECAY_RATE     0.85f               // Cardiovascular recovery rate back to baseline (BPM/sec)

// Optical Skin Proximity (Plan A Contact Gate)
#define PPG_SKIN_ADC_MIN            500                 // Min reflected ADC voltage to confirm tissue contact
#define PPG_SKIN_ADC_MAX            3800                // Max direct ambient saturation ceiling
#define PPG_CONTACT_DEBOUNCE_MS     500                 // Sustained skin contact required to engage (ms)

#define PPG_SAMPLE_RATE_HZ      100                 // 100 Hz PPG ADC sampling (10ms interval)
#define PPG_BUFFER_SIZE         10
#define PPG_MIN_BPM             45                  // Physiological human limits
#define PPG_MAX_BPM             215
#define PPG_ALPHA_DC_FILTER     0.95f               // High-pass dynamic baseline follower
#define PPG_PEAK_COOLDOWN_MS    280                 // Minimum interval between beats (~214 BPM max)

// --- Altimeter Parameters (BMP280) ---
#define BMP_POLL_INTERVAL_MS    1000                // 1 Hz barometric poll
#define BMP_NOISE_DEADBAND_M    0.50f               // 0.5m deadband filter to eliminate wind drafts
#define SEA_LEVEL_PRESSURE_HPA  1013.25f

// --- Perspiration / Microclimate Parameters (DHT) ---
#define DHT_POLL_INTERVAL_MS    2000                // DHT sensor sampling rate (2s)
#define SWEAT_DERIVATIVE_WINDOW_SEC 60              // 60-second rolling derivative calculation window

// --- NVS Storage Namespace & Keys ---
#define NVS_NAMESPACE           "biotracker"
#define NVS_KEY_WEIGHT          "weight"
#define NVS_KEY_HEIGHT          "height"
#define NVS_KEY_STEPS_TOTAL     "steps_tot"
#define NVS_KEY_BASE_HUMIDITY   "base_rh"

// --- Default Biometric Profile ---
#define DEFAULT_USER_WEIGHT_KG  70.0f               // 70.0 kg standard baseline
#define DEFAULT_USER_HEIGHT_CM  175.0f              // 175.0 cm standard baseline

// --- Dynamic Calorie Expenditure Multipliers ---
// Empirical lower-limb kinetic energy model (Nature Sci. Rep. & MDPI Sensors 2024/2025)
#define CAL_KCAL_PER_STEP_BASE  0.040f              // Caloric baseline per flat ground footfall (for 70kg)
#define CAL_KCAL_PER_METER_CLIMB 0.150f             // Caloric expenditure per vertical meter gained
