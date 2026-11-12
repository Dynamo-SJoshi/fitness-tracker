#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

struct UserProfile {
    float weightKg;
    float heightCm;
    float baselineHumidity;
};

class ProfileStorage {
public:
    ProfileStorage();
    bool begin();
    UserProfile getProfile() const;
    void saveWeight(float weightKg);
    void saveHeight(float heightCm);
    void saveBaselineHumidity(float rh);
    void resetToDefaults();

private:
    Preferences prefs;
    UserProfile currentProfile;
};

extern ProfileStorage storage;
