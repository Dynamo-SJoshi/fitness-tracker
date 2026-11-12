#include "algorithms.h"
#include <math.h>

BiometricAlgorithms algorithms;

BiometricAlgorithms::BiometricAlgorithms() {
    lastStepTimeMs = 0;
    filteredMagnitude = 9.81f;
    stepHistIdx = 0;
    stepHistCount = 0;
    for (int i = 0; i < 10; i++) stepTimestamps[i] = 0;

    ppgDcEstimate = 2048.0f;
    ppgDynamicThreshold = 40.0f;
    lastPpgPeakMs = 0;
    bpmBufIdx = 0;
    bpmBufCount = 0;
    for (int i = 0; i < PPG_BUFFER_SIZE; i++) bpmBuffer[i] = 72;
    ppgRising = false;
    prevAcSignal = 0.0f;

    lastFilteredAltitude = 0.0f;
    previousSampleAltitude = 0.0f;
    lastAltTimeMs = 0;
    altitudeInitialized = false;

    dhtHistIdx = 0;
    dhtHistCount = 0;
}

void BiometricAlgorithms::resetSession(BiometricMetrics &metrics) {
    metrics.stepCount = 0;
    metrics.cadenceSpm = 0.0f;
    metrics.elevationGain = 0.0f;
    metrics.totalCalories = 0.0f;
    metrics.flatCalories = 0.0f;
    metrics.verticalCalories = 0.0f;
    altitudeInitialized = false;
    stepHistCount = 0;
    stepHistIdx = 0;
    dhtHistCount = 0;
    dhtHistIdx = 0;
}

// -----------------------------------------------------------------------------
// 1. MPU6050 Footfall & Pedometer Processing
// Algorithm: Calculates 3D vector magnitude, subtracts 1g Earth gravity baseline,
// and applies peak-threshold trigger with 350ms hysteresis debounce.
// -----------------------------------------------------------------------------
bool BiometricAlgorithms::processImuSample(float ax, float ay, float az, uint32_t nowMs, BiometricMetrics &metrics) {
    // 3D vector magnitude: ||a|| = sqrt(ax^2 + ay^2 + az^2)
    float rawMag = sqrtf(ax * ax + ay * ay + az * az);
    metrics.lastVectorMag = rawMag;

    // Exponential smoothing filter to reject high-frequency mechanical vibration
    filteredMagnitude = 0.7f * filteredMagnitude + 0.3f * rawMag;

    // Net acceleration above 1g baseline
    float netAcc = fabsf(filteredMagnitude - PEDO_GRAVITY_EARTH);

    bool stepDetected = false;

    // Peak threshold detection + 350ms hysteresis lockout
    if (netAcc > PEDO_PEAK_THRESHOLD) {
        if ((nowMs - lastStepTimeMs) >= PEDO_HYSTERESIS_DEBOUNCE_MS) {
            metrics.stepCount++;
            lastStepTimeMs = nowMs;
            stepDetected = true;

            // Update rolling cadence (steps per minute)
            stepTimestamps[stepHistIdx] = nowMs;
            stepHistIdx = (stepHistIdx + 1) % 10;
            if (stepHistCount < 10) stepHistCount++;

            if (stepHistCount >= 2) {
                uint8_t oldestIdx = (stepHistIdx - stepHistCount + 10) % 10;
                uint32_t dt = nowMs - stepTimestamps[oldestIdx];
                if (dt > 0) {
                    float spm = ((float)(stepHistCount - 1) / ((float)dt / 1000.0f)) * 60.0f;
                    // Cap cadence to physiologically realistic range (30 - 240 spm)
                    if (spm >= 30.0f && spm <= 240.0f) {
                        metrics.cadenceSpm = spm;
                    }
                }
            }
        }
    }

    // Decay cadence if no steps detected for > 3.0 seconds
    if (nowMs - lastStepTimeMs > 3000) {
        metrics.cadenceSpm = 0.0f;
    }

    return stepDetected;
}

// -----------------------------------------------------------------------------
// 2. Y1708 Optical PPG Pulse Sensor Processing
// Algorithm: Reads raw optical voltage fluctuations from ADC1 (GPIO 32).
// Uses dynamic baseline tracking, rising-edge peak detection, and IBI timing.
// -----------------------------------------------------------------------------
bool BiometricAlgorithms::processPpgSample(uint16_t rawVal, uint32_t nowMs, BiometricMetrics &metrics) {
    metrics.rawPpg = rawVal;
    metrics.beatDetected = false;

    // High-pass DC baseline removal (EWMA)
    ppgDcEstimate = (PPG_ALPHA_DC_FILTER * ppgDcEstimate) + ((1.0f - PPG_ALPHA_DC_FILTER) * (float)rawVal);
    float acSignal = (float)rawVal - ppgDcEstimate;

    // Adaptively decay the dynamic peak threshold
    ppgDynamicThreshold = ppgDynamicThreshold * 0.992f;
    if (ppgDynamicThreshold < 25.0f) {
        ppgDynamicThreshold = 25.0f; // Noise floor floor
    }

    // Rising-edge zero-cross / peak trigger
    bool beatHappened = false;
    if (acSignal > ppgDynamicThreshold && prevAcSignal <= ppgDynamicThreshold) {
        uint32_t ibiMs = nowMs - lastPpgPeakMs;

        // Check refractory lockout cooldown
        if (ibiMs >= PPG_PEAK_COOLDOWN_MS) {
            float calculatedBpm = 60000.0f / (float)ibiMs;

            if (calculatedBpm >= PPG_MIN_BPM && calculatedBpm <= PPG_MAX_BPM) {
                lastPpgPeakMs = nowMs;
                beatHappened = true;
                metrics.beatDetected = true;
                metrics.lastBeatTimeMs = nowMs;

                // Adjust threshold dynamically based on detected peak amplitude
                ppgDynamicThreshold = acSignal * 0.65f;

                // Buffer rolling average
                bpmBuffer[bpmBufIdx] = (uint16_t)calculatedBpm;
                bpmBufIdx = (bpmBufIdx + 1) % PPG_BUFFER_SIZE;
                if (bpmBufCount < PPG_BUFFER_SIZE) bpmBufCount++;

                uint32_t sum = 0;
                for (uint8_t i = 0; i < bpmBufCount; i++) {
                    sum += bpmBuffer[i];
                }
                metrics.bpm = (uint16_t)(sum / bpmBufCount);
            }
        }
    }

    // If no beat detected for 3.5 seconds, reset/decay BPM
    if (nowMs - lastPpgPeakMs > 3500) {
        metrics.bpm = 0;
    }

    prevAcSignal = acSignal;
    return beatHappened;
}

// -----------------------------------------------------------------------------
// 3. BMP280 Barometric Altimeter Processing
// Algorithm: Polled every 1 sec. Uses 0.5m noise filter to ignore wind drafts
// and a downhill filter so only upward climbing contributes to calorie gain.
// -----------------------------------------------------------------------------
void BiometricAlgorithms::processAltitudeSample(float rawAlt, uint32_t nowMs, BiometricMetrics &metrics) {
    if (!altitudeInitialized) {
        lastFilteredAltitude = rawAlt;
        previousSampleAltitude = rawAlt;
        metrics.baseAltitude = rawAlt;
        metrics.currentAltitude = rawAlt;
        metrics.elevationGain = 0.0f;
        metrics.verticalSpeedMps = 0.0f;
        lastAltTimeMs = nowMs;
        altitudeInitialized = true;
        return;
    }

    // Low-pass filter to dampen ambient air turbulences
    float filteredAlt = 0.8f * lastFilteredAltitude + 0.2f * rawAlt;
    metrics.currentAltitude = filteredAlt;

    float dtSec = (float)(nowMs - lastAltTimeMs) / 1000.0f;
    if (dtSec >= 0.8f) {
        float altDelta = filteredAlt - previousSampleAltitude;

        // Apply 0.5-meter deadband noise filter
        if (fabsf(altDelta) >= BMP_NOISE_DEADBAND_M) {
            // Downhill filter: Only active upward elevation gain accumulates
            if (altDelta > 0.0f) {
                metrics.elevationGain += altDelta;
            }
            metrics.verticalSpeedMps = altDelta / dtSec;
            previousSampleAltitude = filteredAlt;
        } else {
            metrics.verticalSpeedMps = 0.0f;
        }

        lastFilteredAltitude = filteredAlt;
        lastAltTimeMs = nowMs;
    }
}

// -----------------------------------------------------------------------------
// 4. DHT11/22 Microclimate Perspiration Index Processing
// Algorithm: Facing skin to measure trapped microclimate. Calculates Delta RH
// (current RH - baseline RH) and Sweat Rate (rolling 60s derivative).
// -----------------------------------------------------------------------------
void BiometricAlgorithms::processDhtSample(float tempC, float humPct, uint32_t nowMs, float baselineRh, BiometricMetrics &metrics) {
    metrics.temperatureC = tempC;
    metrics.humidityPercent = humPct;

    // Delta RH relative to ambient baseline
    metrics.deltaRh = humPct - baselineRh;
    if (metrics.deltaRh < 0.0f) {
        metrics.deltaRh = 0.0f;
    }

    // Append to circular buffer for rolling 60s derivative
    dhtHistory[dhtHistIdx].timestampMs = nowMs;
    dhtHistory[dhtHistIdx].humidity = humPct;
    dhtHistIdx = (dhtHistIdx + 1) % DHT_HISTORY_SIZE;
    if (dhtHistCount < DHT_HISTORY_SIZE) dhtHistCount++;

    // Calculate rolling derivative over ~60s window
    if (dhtHistCount >= 5) {
        uint8_t oldestIdx = (dhtHistIdx - dhtHistCount + DHT_HISTORY_SIZE) % DHT_HISTORY_SIZE;
        float dtMin = (float)(nowMs - dhtHistory[oldestIdx].timestampMs) / 60000.0f;
        if (dtMin > 0.25f) {
            float dHum = humPct - dhtHistory[oldestIdx].humidity;
            metrics.sweatRate = dHum / dtMin; // %RH per minute
        }
    } else {
        metrics.sweatRate = 0.0f;
    }

    // Categorical Perspiration Status classification
    if (metrics.deltaRh < 8.0f && metrics.sweatRate < 1.0f) {
        metrics.sweatStatus = "Basal / Dry";
    } else if (metrics.deltaRh < 18.0f || metrics.sweatRate < 3.5f) {
        metrics.sweatStatus = "Mild Perspiration";
    } else if (metrics.deltaRh < 32.0f || metrics.sweatRate < 7.0f) {
        metrics.sweatStatus = "Active Sweat";
    } else {
        metrics.sweatStatus = "Heavy Sweat / Hydrate Now";
    }
}

// -----------------------------------------------------------------------------
// 5. Dynamic Calorie Expenditure Math
// Combines MPU6050 flat-ground steps (0.04 kcal/step) with BMP280 elevation gain
// vertical work against gravity (0.15 kcal/m climbed), scaled by user body mass.
// -----------------------------------------------------------------------------
void BiometricAlgorithms::computeCalories(float weightKg, float heightCm, BiometricMetrics &metrics) {
    // Mass scaling factor relative to 70.0 kg reference athlete
    float massScale = weightKg / DEFAULT_USER_WEIGHT_KG;

    // Flat ground footfall energy
    metrics.flatCalories = ((float)metrics.stepCount * CAL_KCAL_PER_STEP_BASE) * massScale;

    // Vertical work expenditure against gravity
    metrics.verticalCalories = (metrics.elevationGain * CAL_KCAL_PER_METER_CLIMB) * massScale;

    metrics.totalCalories = metrics.flatCalories + metrics.verticalCalories;
}
