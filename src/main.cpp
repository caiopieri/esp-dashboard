#include <Arduino.h>
#include "config.h"
#include "display/DisplayDriver.h"
#include "core/AppManager.h"
#include "core/NetworkManager.h"
#include "core/DeviceLog.h"
#include "core/VariableStore.h"

// Apps
#include "apps/GeminiUsageApp.h"
#include "apps/ChatGPTUsageApp.h"
#include "apps/ClaudeUsageApp.h"
#include "apps/ClockWeatherApp.h"

static GeminiUsageApp geminiApp;
static ChatGPTUsageApp chatgptApp;
static ClaudeUsageApp claudeApp;
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

    // Load card data before AppManager creates the first visible card.
    VariableStore::getInstance().begin();

    // 2. Register Apps to the Carousel
    AppManager::getInstance().registerApp(&geminiApp);
    AppManager::getInstance().registerApp(&chatgptApp);
    AppManager::getInstance().registerApp(&claudeApp);
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

    // Keep the LVGL/input loop responsive; display transfers are synchronous
    // and already provide the necessary pacing.
    delay(1);
}
