#include "profile_storage.h"

ProfileStorage storage;

ProfileStorage::ProfileStorage() {
    currentProfile.weightKg = DEFAULT_USER_WEIGHT_KG;
    currentProfile.heightCm = DEFAULT_USER_HEIGHT_CM;
    currentProfile.baselineHumidity = 45.0f;
}

bool ProfileStorage::begin() {
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        Serial.println(F("[NVS] Error: Unable to open Preferences namespace!"));
        return false;
    }

    // Load persisted values or write defaults if fresh flash
    if (prefs.isKey(NVS_KEY_WEIGHT)) {
        currentProfile.weightKg = prefs.getFloat(NVS_KEY_WEIGHT, DEFAULT_USER_WEIGHT_KG);
    } else {
        prefs.putFloat(NVS_KEY_WEIGHT, DEFAULT_USER_WEIGHT_KG);
        currentProfile.weightKg = DEFAULT_USER_WEIGHT_KG;
    }

    if (prefs.isKey(NVS_KEY_HEIGHT)) {
        currentProfile.heightCm = prefs.getFloat(NVS_KEY_HEIGHT, DEFAULT_USER_HEIGHT_CM);
    } else {
        prefs.putFloat(NVS_KEY_HEIGHT, DEFAULT_USER_HEIGHT_CM);
        currentProfile.heightCm = DEFAULT_USER_HEIGHT_CM;
    }

    if (prefs.isKey(NVS_KEY_BASE_HUMIDITY)) {
        currentProfile.baselineHumidity = prefs.getFloat(NVS_KEY_BASE_HUMIDITY, 45.0f);
    } else {
        prefs.putFloat(NVS_KEY_BASE_HUMIDITY, 45.0f);
        currentProfile.baselineHumidity = 45.0f;
    }

    Serial.printf("[NVS] Profile loaded: Weight=%.1f kg, Height=%.1f cm, BaseRH=%.1f%%\n",
                  currentProfile.weightKg, currentProfile.heightCm, currentProfile.baselineHumidity);
    return true;
}

UserProfile ProfileStorage::getProfile() const {
    return currentProfile;
}

void ProfileStorage::saveWeight(float weightKg) {
    if (weightKg >= 25.0f && weightKg <= 250.0f) {
        currentProfile.weightKg = weightKg;
        prefs.putFloat(NVS_KEY_WEIGHT, weightKg);
        Serial.printf("[NVS] Saved new weight: %.1f kg\n", weightKg);
    }
}

void ProfileStorage::saveHeight(float heightCm) {
    if (heightCm >= 90.0f && heightCm <= 240.0f) {
        currentProfile.heightCm = heightCm;
        prefs.putFloat(NVS_KEY_HEIGHT, heightCm);
        Serial.printf("[NVS] Saved new height: %.1f cm\n", heightCm);
    }
}

void ProfileStorage::saveBaselineHumidity(float rh) {
    if (rh >= 5.0f && rh <= 100.0f) {
        currentProfile.baselineHumidity = rh;
        prefs.putFloat(NVS_KEY_BASE_HUMIDITY, rh);
        Serial.printf("[NVS] Saved new baseline humidity: %.1f%%\n", rh);
    }
}

void ProfileStorage::resetToDefaults() {
    saveWeight(DEFAULT_USER_WEIGHT_KG);
    saveHeight(DEFAULT_USER_HEIGHT_CM);
    saveBaselineHumidity(45.0f);
    Serial.println(F("[NVS] Profile reset to factory defaults."));
}
