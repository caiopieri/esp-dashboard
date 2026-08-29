#pragma once

#include <Arduino.h>

// Owns the one-time Wi-Fi onboarding flow. The ESP-IDF provisioning manager
// handles the encrypted credential exchange; this wrapper keeps the product
// contract independent from the transport (BLE today, SoftAP fallback).
class ProvisioningManager {
public:
    static ProvisioningManager& getInstance() {
        static ProvisioningManager instance;
        return instance;
    }

    void begin(bool applicationCredentialsAvailable);
    void onWifiConnected();
    bool isActive() const { return _active; }
    String getInfoJson() const;

private:
    ProvisioningManager() = default;

    bool _started = false;
    bool _active = false;
    String _serviceName;
    uint8_t _transport = 0; // 0=none, 1=BLE, 2=SoftAP
};
