#include "ProvisioningManager.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_system.h>
#if defined(BOARD_JC3248W535EN)
#include <WiFiProv.h>
#endif

#include "DeviceLog.h"

static char provisioningPop[9] = "00000000";

void ProvisioningManager::begin(bool applicationCredentialsAvailable) {
    if (_started) return;
    _started = true;

    if (applicationCredentialsAvailable) {
        _transport = 0;
        LOG_INFO("WiFi provisioning skipped: application credentials are configured");
        return;
    }

    String mac = WiFi.macAddress();
    mac.replace(":", "");
    mac.toUpperCase();
    if (mac.length() < 8) mac = "00000000";
    const String suffix = mac.substring(mac.length() - 8);
    _serviceName = "PROV_" + suffix;
    // Security 1 requires a proof of possession. Keep a random per-unit value
    // in NVS so it is not guessable from the public MAC address and survives
    // reboot. It is shown only during onboarding/diagnostics.
    Preferences preferences;
    preferences.begin("provision", false);
    String storedPop = preferences.getString("pop", "");
    if (storedPop.length() == 8) {
        storedPop.toCharArray(provisioningPop, sizeof(provisioningPop));
    } else {
        snprintf(provisioningPop, sizeof(provisioningPop), "%08lX",
                 static_cast<unsigned long>(esp_random()));
        preferences.putString("pop", provisioningPop);
    }
    preferences.end();

#if defined(BOARD_JC3248W535EN)
#if defined(CONFIG_BLUEDROID_ENABLED) && CONFIG_BLUEDROID_ENABLED
    _transport = 1;
    WiFiProv.beginProvision(
        WIFI_PROV_SCHEME_BLE,
        WIFI_PROV_SCHEME_HANDLER_FREE_BLE,
        WIFI_PROV_SECURITY_1,
        provisioningPop,
        _serviceName.c_str(),
        nullptr,
        nullptr,
        false);
    WiFiProv.printQR(_serviceName.c_str(), provisioningPop, "ble");
    LOG_INFO("WiFi provisioning started over BLE: service=%s", _serviceName.c_str());
#else
    // Some ESP32 Arduino builds disable Bluedroid. Keep first-boot setup
    // usable through the ESP-IDF provisioning SoftAP in that configuration.
    _transport = 2;
    WiFiProv.beginProvision(
        WIFI_PROV_SCHEME_SOFTAP,
        WIFI_PROV_SCHEME_HANDLER_NONE,
        WIFI_PROV_SECURITY_1,
        provisioningPop,
        _serviceName.c_str(),
        nullptr,
        nullptr,
        false);
    WiFiProv.printQR(_serviceName.c_str(), provisioningPop, "softap");
    LOG_INFO("WiFi provisioning started over SoftAP: service=%s", _serviceName.c_str());
#endif
#if defined(BOARD_JC3248W535EN)
    _active = true;
#endif
#else
    // The classic CYD cannot afford the full WiFiProv/BLE provisioning
    // stack alongside its display driver. Its local setup AP exposes the
    // existing web panel and uses the per-device PoP as the WPA2 password.
    _transport = 2;
    WiFi.mode(WIFI_AP_STA);
    if (WiFi.softAP(_serviceName.c_str(), provisioningPop)) {
        _active = true;
        LOG_INFO("WiFi provisioning started over local SoftAP: ssid=%s ip=%s",
                 _serviceName.c_str(), WiFi.softAPIP().toString().c_str());
    } else {
        LOG_ERROR("Could not start local provisioning SoftAP");
    }
#endif

    return;
}

void ProvisioningManager::onWifiConnected() {
    if (!_active) return;
    if (_transport == 2) {
        WiFi.softAPdisconnect(true);
    }
    _active = false;
    LOG_INFO("WiFi provisioning session closed after successful connection");
}

String ProvisioningManager::getInfoJson() const {
    JsonDocument doc;
    doc["schemaVersion"] = 1;
    doc["active"] = _active;
    doc["transport"] = _transport == 1 ? "ble" : (_transport == 2 ? "softap" : "none");
    if (_active) {
        doc["serviceName"] = _serviceName;
        doc["proofOfPossession"] = provisioningPop;
        doc["expires"] = "until Wi-Fi is provisioned or device reboots";
    }
    String output;
    serializeJson(doc, output);
    return output;
}
