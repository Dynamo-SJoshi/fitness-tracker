#include "sensors_handler.h"

SensorsHandler sensors;

SensorsHandler::SensorsHandler() 
    : bmp(&Wire),
      dht(PIN_DHT_DATA, DHT_TYPE),
      mpuFound(false),
      bmpFound(false),
      dhtFound(false),
      lastPpgSampleMs(0),
      lastImuSampleMs(0),
      lastBmpSampleMs(0),
      lastDhtSampleMs(0) {
    memset(&metrics, 0, sizeof(BiometricMetrics));
    metrics.sweatStatus = "Initializing";
}

bool SensorsHandler::begin() {
    Serial.println(F("[Sensors] Initializing hardware buses..."));

    // Set up Status LED
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);

    // Set up ADC1 for Y1708 PPG optical sensor
    analogReadResolution(12); // 0 - 4095
    analogSetAttenuation(ADC_11db); // Full range ~0 - 3.3V
    pinMode(PIN_PPG_ANALOG, INPUT);

    // Hardware I2C bus clear: toggle SCL 9 times to un-stick any hung slave
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(PIN_I2C_SCL, OUTPUT);
    for (int i = 0; i < 9; i++) {
        digitalWrite(PIN_I2C_SCL, HIGH);
        delayMicroseconds(5);
        digitalWrite(PIN_I2C_SCL, LOW);
        delayMicroseconds(5);
    }
    digitalWrite(PIN_I2C_SCL, HIGH);
    delay(10);

    // Initialize I2C bus at robust 100kHz standard mode (much more reliable over jumper wires)
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
    Wire.setTimeOut(30);

    // Automated I2C Scanner to report every attached device
    Serial.println(F("[I2C Scan] Scanning SDA(21) / SCL(22)..."));
    uint8_t i2cDevices = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  -> Detected active device at address 0x%02X", addr);
            if (addr == 0x68 || addr == 0x69) Serial.print(F(" (MPU6050)"));
            if (addr == 0x76 || addr == 0x77) Serial.print(F(" (BMP280)"));
            Serial.println();
            i2cDevices++;
        }
    }
    if (i2cDevices == 0) {
        Serial.println(F("  -> WARNING: Zero I2C devices detected! Check VCC, GND, SDA, SCL wires."));
    }

    // 1. Initialize MPU6050 (Try 0x68, then 0x69 fallback)
    Serial.print(F("[Sensors] Connecting to MPU6050... "));
    if (mpu.begin(0x68, &Wire) || mpu.begin(0x69, &Wire)) {
        mpuFound = true;
        mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        Serial.println(F("SUCCESS (4G range, 21Hz LPF)"));
    } else {
        Serial.println(F("FAILED! Check 5-wire harness I2C connection."));
    }

    // 2. Initialize BMP280 (Try 0x76 then 0x77)
    Serial.print(F("[Sensors] Connecting to BMP280... "));
    if (bmp.begin(I2C_ADDR_BMP280_ALT) || bmp.begin(I2C_ADDR_BMP280_DEF)) {
        bmpFound = true;
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                        Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                        Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                        Adafruit_BMP280::FILTER_X16,      /* Filtering */
                        Adafruit_BMP280::STANDBY_MS_500); /* Standby time */
        Serial.println(F("SUCCESS (Ultra High Res Mode)"));
    } else {
        Serial.println(F("FAILED! Check BMP280 address/wiring."));
    }

    // 3. Initialize DHT sensor
    Serial.println(F("[Sensors] Initializing DHT Microclimate Sensor..."));
    dht.begin();
    dhtFound = true;

    // Initial warm-up read
    delay(100);
    pollDht(millis());

    return (mpuFound || bmpFound);
}

void SensorsHandler::update(uint32_t nowMs) {
    // 100 Hz PPG Analog Poll (every 10ms)
    if (nowMs - lastPpgSampleMs >= 10) {
        lastPpgSampleMs = nowMs;
        pollPpg(nowMs);
    }

    // 50 Hz IMU Acceleration Poll (every 20ms)
    if (nowMs - lastImuSampleMs >= 20) {
        lastImuSampleMs = nowMs;
        pollImu(nowMs);
    }

    // 1 Hz BMP280 Barometric Poll (every 1000ms)
    if (nowMs - lastBmpSampleMs >= BMP_POLL_INTERVAL_MS) {
        lastBmpSampleMs = nowMs;
        pollBmp(nowMs);
    }

    // 0.5 Hz DHT Microclimate Poll (every 2000ms)
    if (nowMs - lastDhtSampleMs >= DHT_POLL_INTERVAL_MS) {
        lastDhtSampleMs = nowMs;
        pollDht(nowMs);
    }

    // Periodic Dynamic Calorie Math & Status Update
    UserProfile profile = storage.getProfile();
    algorithms.computeCalories(profile.weightKg, profile.heightCm, metrics);
    metrics.uptimeSeconds = nowMs / 1000;

    // Diagnostic Serial Telemetry (1.5s interval to monitor live MPU status & dynamic acceleration)
    static uint32_t lastDiagMs = 0;
    if (nowMs - lastDiagMs > 1500) {
        lastDiagMs = nowMs;
        Serial.printf("[DIAG] MPU: %s | DynAcc: %.2f m/s2 | Steps: %lu | Cadence: %.0f | BPM: %u\n",
            mpuFound ? "OK" : "NOT FOUND!",
            metrics.lastVectorMag,
            (unsigned long)metrics.stepCount,
            metrics.cadenceSpm,
            metrics.bpm);
    }
}

void SensorsHandler::pollPpg(uint32_t nowMs) {
    uint16_t rawVal = analogRead(PIN_PPG_ANALOG);
    bool beat = algorithms.processPpgSample(rawVal, nowMs, metrics);
    if (beat) {
        digitalWrite(PIN_STATUS_LED, HIGH);
    } else if (nowMs - metrics.lastBeatTimeMs > 40) {
        digitalWrite(PIN_STATUS_LED, LOW);
    }
}

void SensorsHandler::pollImu(uint32_t nowMs) {
    if (!mpuFound) {
        // Automatic hot-plug recovery every 2.5s if wires were loose or wiggled
        static uint32_t lastMpuRetryMs = 0;
        if (nowMs - lastMpuRetryMs > 2500) {
            lastMpuRetryMs = nowMs;
            if (mpu.begin(0x68, &Wire) || mpu.begin(0x69, &Wire)) {
                mpuFound = true;
                mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
                mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
                Serial.println(F("[Sensors] MPU6050 Re-connected successfully!"));
            }
        }
        return;
    }

    sensors_event_t a, g, temp;
    if (mpu.getEvent(&a, &g, &temp)) {
        algorithms.processImuSample(a.acceleration.x, a.acceleration.y, a.acceleration.z, nowMs, metrics);
    } else {
        // Glitch guard: If repeated I2C transactions fail, trigger auto-reconnect
        static uint8_t failStreak = 0;
        if (++failStreak > 30) {
            mpuFound = false;
            failStreak = 0;
            Serial.println(F("[Sensors] MPU6050 I2C bus error, searching for sensor..."));
        }
    }
}

void SensorsHandler::pollBmp(uint32_t nowMs) {
    if (!bmpFound) return;

    float currentAlt = bmp.readAltitude(SEA_LEVEL_PRESSURE_HPA);
    algorithms.processAltitudeSample(currentAlt, nowMs, metrics);
}

void SensorsHandler::pollDht(uint32_t nowMs) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Check if any reads failed
    if (isnan(h) || isnan(t)) {
        return;
    }

    UserProfile profile = storage.getProfile();
    algorithms.processDhtSample(t, h, nowMs, profile.baselineHumidity, metrics);
}

const BiometricMetrics& SensorsHandler::getMetrics() const {
    return metrics;
}

void SensorsHandler::resetSessionData() {
    algorithms.resetSession(metrics);
}

void SensorsHandler::calibrateHumidityBaseline() {
    float h = dht.readHumidity();
    if (!isnan(h) && h >= 10.0f && h <= 95.0f) {
        storage.saveBaselineHumidity(h);
        metrics.deltaRh = 0.0f;
        metrics.sweatRate = 0.0f;
        metrics.sweatStatus = "Basal / Calibrated";
    }
}

void SensorsHandler::calibrateAltitudeBaseline() {
    if (bmpFound) {
        metrics.baseAltitude = bmp.readAltitude(SEA_LEVEL_PRESSURE_HPA);
        metrics.elevationGain = 0.0f;
    }
}
