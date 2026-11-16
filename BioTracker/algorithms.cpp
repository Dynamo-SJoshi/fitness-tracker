#include "algorithms.h"
#include <math.h>

BiometricAlgorithms algorithms;

BiometricAlgorithms::BiometricAlgorithms() {
    lastStepTimeMs = 0;
    filteredMagnitude = 9.81f;
    dynamicGravity = 9.81f;
    gravityInitialized = false;
    pedoState = PEDO_STATE_ARMED;
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

    currentSimBpm = PPG_RESTING_BASE_BPM;
    lastSimBeatTimeMs = 0;
    lastPpgUpdateMs = 0;
    motionStopTimestampMs = 0;
    motionStartTimestampMs = 0;
    wasMoving = false;
    hrvOscillation = 0.0f;
    skinContactActive = false;
    skinContactStartMs = 0;
    skinContactLostMs = 0;

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
    pedoState = PEDO_STATE_ARMED;
    dynamicGravity = 9.81f;
    gravityInitialized = false;
    currentSimBpm = PPG_RESTING_BASE_BPM;
    wasMoving = false;
    skinContactActive = false;
    skinContactStartMs = 0;
    skinContactLostMs = 0;
}

// -----------------------------------------------------------------------------
// 1. MPU6050 Footfall & Pedometer Processing (Pure Original 1g Logic)
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
// Algorithm: Reads raw optical voltage fluctuations from ADC1 (GPIO 32),
// or uses physiological cardiovascular simulation when SIMULATE_PPG_HEART_RATE is true.
// -----------------------------------------------------------------------------
bool BiometricAlgorithms::processPpgSample(uint16_t rawVal, uint32_t nowMs, BiometricMetrics &metrics) {
#if SIMULATE_PPG_HEART_RATE
    // -------------------------------------------------------------------------
    // Optical Skin Proximity Gate (Reads actual photodiode voltage from GPIO 32)
    // In open air without skin contact, reflected green light is minimal.
    // When pressed against tissue, backscatter brings rawVal into contact range.
    // -------------------------------------------------------------------------
    bool opticalReflect = (rawVal >= PPG_SKIN_ADC_MIN && rawVal <= PPG_SKIN_ADC_MAX);
    if (opticalReflect) {
        if (skinContactStartMs == 0) skinContactStartMs = nowMs;
        if ((nowMs - skinContactStartMs) >= PPG_CONTACT_DEBOUNCE_MS) {
            skinContactActive = true;
        }
        skinContactLostMs = 0;
    } else {
        if (skinContactLostMs == 0) skinContactLostMs = nowMs;
        if ((nowMs - skinContactLostMs) >= 400) {
            skinContactActive = false;
            skinContactStartMs = 0;
        }
    }

    // If NOT touching skin: shut down heart rate & display real ambient ADC
    if (!skinContactActive) {
        metrics.bpm = 0;             // Displays as '--' on dashboard
        metrics.beatDetected = false;
        metrics.rawPpg = rawVal;     // Genuine live photodiode readout
        currentSimBpm = PPG_RESTING_BASE_BPM;
        wasMoving = false;
        return false;
    }

    // -------------------------------------------------------------------------
    // Physiological Cardiovascular Simulation (Motion-Correlated with Lag & Hold)
    // -------------------------------------------------------------------------
    bool isMoving = (metrics.cadenceSpm >= 25.0f);

    // Track motion state transitions
    if (isMoving && !wasMoving) {
        motionStartTimestampMs = nowMs;
        wasMoving = true;
    } else if (!isMoving && wasMoving) {
        motionStopTimestampMs = nowMs;
        wasMoving = false;
    }

    // Determine target HR based on current cadence
    float targetBpm = PPG_RESTING_BASE_BPM;
    if (isMoving) {
        if (metrics.cadenceSpm < 85.0f) {
            // Light walking (50-85 SPM -> 82 to 98 BPM)
            targetBpm = 82.0f + (metrics.cadenceSpm - 50.0f) * 0.45f;
        } else if (metrics.cadenceSpm < 130.0f) {
            // Brisk walking / jogging (85-130 SPM -> 98 to 132 BPM)
            targetBpm = 98.0f + (metrics.cadenceSpm - 85.0f) * 0.75f;
        } else {
            // Running (> 130 SPM -> 132 to 158 BPM)
            targetBpm = 132.0f + (metrics.cadenceSpm - 130.0f) * 0.50f;
            if (targetBpm > 165.0f) targetBpm = 165.0f;
        }
    }

    // Time delta calculation
    float dtSec = (float)(nowMs - lastPpgUpdateMs) / 1000.0f;
    if (dtSec > 0.5f || dtSec <= 0.0f) dtSec = 0.01f;
    lastPpgUpdateMs = nowMs;

    // Apply physiological cardiovascular response lag:
    if (isMoving) {
        // Latency window: Wait ~2.5 seconds of sustained steps before HR starts accelerating
        uint32_t moveDuration = nowMs - motionStartTimestampMs;
        if (moveDuration > 2500) {
            if (currentSimBpm < targetBpm) {
                // Smooth cardiovascular acceleration (~2.0 BPM per second)
                currentSimBpm += 2.0f * dtSec;
                if (currentSimBpm > targetBpm) currentSimBpm = targetBpm;
            } else if (currentSimBpm > targetBpm) {
                // Smooth deceleration if cadence drops while still walking
                currentSimBpm -= 1.0f * dtSec;
            }
        }
    } else {
        // User stopped!
        // Plateau hold: Keep HR elevated for PPG_HOLD_AFTER_STOP_SEC (8s)
        uint32_t stopDuration = nowMs - motionStopTimestampMs;
        if (stopDuration >= (PPG_HOLD_AFTER_STOP_SEC * 1000)) {
            // Recovery decay phase: Gradually cool down back to resting baseline
            if (currentSimBpm > PPG_RESTING_BASE_BPM) {
                currentSimBpm -= PPG_RECOVERY_DECAY_RATE * dtSec;
                if (currentSimBpm < PPG_RESTING_BASE_BPM) currentSimBpm = PPG_RESTING_BASE_BPM;
            }
        }
    }

    // Natural respiratory sinus arrhythmia (HRV wave ~0.18 Hz, +/- 1.2 BPM)
    hrvOscillation += dtSec * 1.15f;
    float liveBpm = currentSimBpm + (sinf(hrvOscillation) * 1.2f);
    if (liveBpm < 45.0f) liveBpm = 45.0f;

    // Beat-to-beat interval timing
    uint32_t beatIntervalMs = (uint32_t)(60000.0f / liveBpm);
    bool beatHappened = false;

    if (nowMs - lastSimBeatTimeMs >= beatIntervalMs) {
        lastSimBeatTimeMs = nowMs;
        beatHappened = true;
        metrics.beatDetected = true;
        metrics.lastBeatTimeMs = nowMs;
        metrics.bpm = (uint16_t)roundf(liveBpm);
    }

    // Synthesize realistic optical PPG dicrotic waveform for rawPpg (0 - 4095)
    uint32_t timeSinceBeat = nowMs - lastSimBeatTimeMs;
    float phase = (float)timeSinceBeat / (float)beatIntervalMs;
    float ppgVal = 1980.0f;

    if (phase < 0.18f) {
        // Systolic expansion peak
        float p = phase / 0.18f;
        ppgVal += 520.0f * sinf(p * 3.14159f);
    } else if (phase >= 0.28f && phase < 0.46f) {
        // Dicrotic notch / diastolic reflex
        float p = (phase - 0.28f) / 0.18f;
        ppgVal += 160.0f * sinf(p * 3.14159f);
    }
    // Subtle analog ADC noise (+/- 4 counts)
    ppgVal += (float)(rand() % 9 - 4);
    metrics.rawPpg = (uint16_t)ppgVal;

    return beatHappened;

#else
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
#endif
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
