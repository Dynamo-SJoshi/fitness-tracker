#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include "config.h"
#include "algorithms.h"
#include "profile_storage.h"

class SensorsHandler {
public:
    SensorsHandler();
    bool begin();
    void update(uint32_t nowMs);
    const BiometricMetrics& getMetrics() const;
    void resetSessionData();
    void calibrateHumidityBaseline();
    void calibrateAltitudeBaseline();

    bool isMpuConnected() const { return mpuFound; }
    bool isBmpConnected() const { return bmpFound; }
    bool isDhtConnected() const { return dhtFound; }

private:
    void pollPpg(uint32_t nowMs);
    void pollImu(uint32_t nowMs);
    void pollBmp(uint32_t nowMs);
    void pollDht(uint32_t nowMs);

    Adafruit_MPU6050 mpu;
    Adafruit_BMP280 bmp;
    DHT dht;

    bool mpuFound;
    bool bmpFound;
    bool dhtFound;

    uint32_t lastPpgSampleMs;
    uint32_t lastImuSampleMs;
    uint32_t lastBmpSampleMs;
    uint32_t lastDhtSampleMs;

    BiometricMetrics metrics;
};

extern SensorsHandler sensors;
