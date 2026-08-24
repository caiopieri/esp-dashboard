#include <Arduino.h>
#include "config.h"
#include "display/DisplayDriver.h"
#include "core/AppManager.h"
#include "core/NetworkManager.h"
#include "core/DeviceLog.h"

// Apps
#include "apps/GeminiUsageApp.h"
#include "apps/ClockWeatherApp.h"

static GeminiUsageApp geminiApp;
static ClockWeatherApp clockApp;

void setup() {
    Serial.begin(115200);
    delay(500);
    DeviceLog::begin();
    Serial.println("\n========================================");
    Serial.println("   CYD Smart Display OS Starting...   ");
    Serial.println("========================================");

    // 1. Initialize Display & Calibrated Touch (LovyanGFX + XPT2046 + LVGL)
    DisplayDriver::init();
    DisplayDriver::setBrightness(SCREEN_BRIGHTNESS);

    // 2. Register Apps to the Carousel
    AppManager::getInstance().registerApp(&geminiApp);
    AppManager::getInstance().registerApp(&clockApp);

    // 3. Build UI Carousel & Lifecycle Manager
    AppManager::getInstance().init();

    // 4. Start Wi-Fi & OTA
    NetworkManager::getInstance().begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
    // Refresh LVGL timers and animations
    DisplayDriver::handle();

    // Run active app's loop hook
    AppManager::getInstance().update();

    // Handle background Wi-Fi & OTA updates
    NetworkManager::getInstance().handle();

    delay(5);
}
