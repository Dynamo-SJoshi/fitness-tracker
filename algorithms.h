#pragma once

#include <Arduino.h>
#include "config.h"

// Biometric & Gait Metric Snapshot
struct BiometricMetrics {
    // Pedometer
    uint32_t stepCount;
    float cadenceSpm;           // Steps per minute
    float lastVectorMag;        // Instantaneous acceleration magnitude (m/s^2)
    
    // Heart Rate
    uint16_t bpm;
    uint16_t rawPpg;
    bool beatDetected;
    uint32_t lastBeatTimeMs;
    
    // Barometer & Elevation
    float currentAltitude;
    float baseAltitude;
    float elevationGain;
    float verticalSpeedMps;
    
    // Perspiration Microclimate
    float temperatureC;
    float humidityPercent;
    float deltaRh;              // Current RH - Baseline RH
    float sweatRate;            // %RH / min (rolling derivative)
    const char* sweatStatus;    // Categorical index
    
    // Caloric Expenditure
    float totalCalories;
    float flatCalories;
    float verticalCalories;
    
    // System Status
    uint32_t uptimeSeconds;
    float batteryVoltageEstimate;
};

enum PedometerState {
    PEDO_STATE_ARMED,        // Ready to detect rising footfall impact peak
    PEDO_STATE_WAIT_VALLEY   // Peak detected; waiting for valley drop & debounce window
};

class BiometricAlgorithms {
public:
    BiometricAlgorithms();

    // Pedometer processing
    bool processImuSample(float ax, float ay, float az, uint32_t nowMs, BiometricMetrics &metrics);
    
    // PPG processing
    bool processPpgSample(uint16_t rawVal, uint32_t nowMs, BiometricMetrics &metrics);
    
    // Barometer & Altitude processing
    void processAltitudeSample(float rawAlt, uint32_t nowMs, BiometricMetrics &metrics);
    
    // Perspiration microclimate processing
    void processDhtSample(float tempC, float humPct, uint32_t nowMs, float baselineRh, BiometricMetrics &metrics);
    
    // Calorie computation
    void computeCalories(float weightKg, float heightCm, BiometricMetrics &metrics);
    
    // Reset session metrics
    void resetSession(BiometricMetrics &metrics);

private:
    // Pedometer states
    uint32_t lastStepTimeMs;
    float filteredMagnitude;
    float dynamicGravity;
    PedometerState pedoState;
    bool gravityInitialized;
    uint32_t stepTimestamps[10];
    uint8_t stepHistIdx;
    uint8_t stepHistCount;

    // PPG states
    float ppgDcEstimate;
    float ppgDynamicThreshold;
    uint32_t lastPpgPeakMs;
    uint16_t bpmBuffer[PPG_BUFFER_SIZE];
    uint8_t bpmBufIdx;
    uint8_t bpmBufCount;
    bool ppgRising;
    float prevAcSignal;

    // Simulated Cardiovascular Engine states
    float currentSimBpm;
    uint32_t lastSimBeatTimeMs;
    uint32_t lastPpgUpdateMs;
    uint32_t motionStopTimestampMs;
    uint32_t motionStartTimestampMs;
    bool wasMoving;
    float hrvOscillation;
    bool skinContactActive;
    uint32_t skinContactStartMs;
    uint32_t skinContactLostMs;

    // Altimeter states
    float lastFilteredAltitude;
    float previousSampleAltitude;
    uint32_t lastAltTimeMs;
    bool altitudeInitialized;

    // Sweat rate circular buffer (60s history)
    struct DhtSamplePoint {
        uint32_t timestampMs;
        float humidity;
    };
    static const uint8_t DHT_HISTORY_SIZE = 30; // 30 samples spaced ~2s = 60s
    DhtSamplePoint dhtHistory[DHT_HISTORY_SIZE];
    uint8_t dhtHistIdx;
    uint8_t dhtHistCount;
};

extern BiometricAlgorithms algorithms;
