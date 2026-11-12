#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "sensors_handler.h"
#include "profile_storage.h"
#include "web_dashboard.h"

class WebServerHandler {
public:
    WebServerHandler();
    void begin();
    void handleClient();

private:
    void setupRoutes();
    void handleRoot();
    void handleApiData();
    void handleApiProfile();
    void handleApiReset();
    void handleApiCalibrateHumidity();
    void handleApiZeroAltitude();
    void handleNotFound();

    WebServer server;
};

extern WebServerHandler webServer;
