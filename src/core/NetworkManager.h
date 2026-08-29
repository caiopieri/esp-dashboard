#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WebServer.h>

class NetworkManager {
public:
    static NetworkManager& getInstance() {
        static NetworkManager instance;
        return instance;
    }

    void begin(const char* ssid, const char* password);
    void handle();
    bool isConnected();
    String getFormattedTime();

    void startWifiScan();
    bool pollWifiScan();
    bool isWifiScanPending() const;
    int getWifiScanCount() const;
    bool wifiScanFailed() const;
    String getWifiScanSSID(int index) const;
    int getWifiScanRSSI(int index) const;
    bool connectToWifi(const char* ssid, const char* password);

private:
    NetworkManager() : _webServer(80) {}
    unsigned long _lastWifiCheck = 0;
    unsigned long _lastClockUpdate = 0;
    bool _connected = false;
    bool _scanPending = false;
    bool _scanFailed = false;
    int _scanCount = 0;
    uint8_t _scanRetries = 0;
    unsigned long _scanRetryAt = 0;
    bool _connecting = false;
    unsigned long _connectStarted = 0;
    String _pendingSSID;
    String _pendingPassword;
    Preferences _preferences;
    WebServer _webServer;
    bool _restartPending = false;
    unsigned long _restartAt = 0;
    bool _scanStartRequested = false;
    bool _wifiConnectRequested = false;
    String _requestedSSID;
    String _requestedPassword;
#if defined(BOARD_JC3248W535EN)
    String _mdnsHostname;
    bool _mdnsStarted = false;
#endif
};
