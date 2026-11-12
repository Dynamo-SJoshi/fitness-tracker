#include "web_server_handler.h"

WebServerHandler webServer;

WebServerHandler::WebServerHandler() : server(80) {}

void WebServerHandler::begin() {
    Serial.println(F("[WiFi] Starting Off-Grid SoftAP..."));

    // Configure SoftAP
    WiFi.mode(WIFI_AP);
    bool apStarted = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CONN);

    if (apStarted) {
        IPAddress ip = WiFi.softAPIP();
        Serial.printf("[WiFi] SoftAP '%s' active at IP: %s\n", WIFI_AP_SSID, ip.toString().c_str());
    } else {
        Serial.println(F("[WiFi] ERROR: Failed to launch SoftAP!"));
    }

    setupRoutes();
    server.begin();
    Serial.println(F("[HTTP] Embedded Web Server started on port 80."));
}

void WebServerHandler::handleClient() {
    server.handleClient();
}

void WebServerHandler::setupRoutes() {
    server.on("/", HTTP_GET, std::bind(&WebServerHandler::handleRoot, this));
    server.on("/api/data", HTTP_GET, std::bind(&WebServerHandler::handleApiData, this));
    server.on("/api/profile", HTTP_ANY, std::bind(&WebServerHandler::handleApiProfile, this));
    server.on("/api/reset", HTTP_POST, std::bind(&WebServerHandler::handleApiReset, this));
    server.on("/api/calibrate-humidity", HTTP_POST, std::bind(&WebServerHandler::handleApiCalibrateHumidity, this));
    server.on("/api/zero-altitude", HTTP_POST, std::bind(&WebServerHandler::handleApiZeroAltitude, this));
    server.onNotFound(std::bind(&WebServerHandler::handleNotFound, this));
}

void WebServerHandler::handleRoot() {
    server.send_P(200, "text/html", DASHBOARD_HTML);
}

void WebServerHandler::handleApiData() {
    const BiometricMetrics &m = sensors.getMetrics();
    UserProfile prof = storage.getProfile();

    // Construct lightweight JSON payload
    char json[512];
    snprintf(json, sizeof(json),
        "{"
        "\"bpm\":%u,"
        "\"raw_ppg\":%u,"
        "\"steps\":%lu,"
        "\"cadence\":%.1f,"
        "\"calories\":%.2f,"
        "\"flat_cal\":%.2f,"
        "\"vert_cal\":%.2f,"
        "\"elevation_gain\":%.1f,"
        "\"altitude\":%.1f,"
        "\"temp\":%.1f,"
        "\"humidity\":%.1f,"
        "\"delta_rh\":%.1f,"
        "\"sweat_rate\":%.2f,"
        "\"sweat_status\":\"%s\","
        "\"weight\":%.1f,"
        "\"height\":%.1f,"
        "\"uptime\":%lu"
        "}",
        m.bpm,
        m.rawPpg,
        (unsigned long)m.stepCount,
        m.cadenceSpm,
        m.totalCalories,
        m.flatCalories,
        m.verticalCalories,
        m.elevationGain,
        m.currentAltitude,
        m.temperatureC,
        m.humidityPercent,
        m.deltaRh,
        m.sweatRate,
        m.sweatStatus ? m.sweatStatus : "OK",
        prof.weightKg,
        prof.heightCm,
        (unsigned long)m.uptimeSeconds
    );

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

void WebServerHandler::handleApiProfile() {
    bool updated = false;

    if (server.hasArg("weight")) {
        float w = server.arg("weight").toFloat();
        if (w > 0.0f) {
            storage.saveWeight(w);
            updated = true;
        }
    }

    if (server.hasArg("height")) {
        float h = server.arg("height").toFloat();
        if (h > 0.0f) {
            storage.saveHeight(h);
            updated = true;
        }
    }

    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (updated) {
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Profile saved in NVS\"}");
    } else {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing or invalid weight/height params\"}");
    }
}

void WebServerHandler::handleApiReset() {
    sensors.resetSessionData();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Session counters reset\"}");
}

void WebServerHandler::handleApiCalibrateHumidity() {
    sensors.calibrateHumidityBaseline();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Humidity baseline recalibrated\"}");
}

void WebServerHandler::handleApiZeroAltitude() {
    sensors.calibrateAltitudeBaseline();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Altitude zeroed\"}");
}

void WebServerHandler::handleNotFound() {
    // Captive portal fallback redirect to root dashboard
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
}
