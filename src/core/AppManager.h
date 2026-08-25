#pragma once

#include <vector>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <lvgl.h>
#include "App.h"
#include "DeclarativeCardApp.h"

class AppManager {
public:
    static AppManager& getInstance() {
        static AppManager instance;
        return instance;
    }

    void registerApp(App* app);
    void init();
    void update();
    void switchToApp(size_t index);
    void setStatusBarText(const char* text);
    void setWifiStatus(bool connected, int rssi = 0);
    void openWifiPanel();
    String getCardConfigJson();
    bool saveCardConfigJson(const String& json);

private:
    AppManager() = default;

    std::vector<App*> _apps;
    std::vector<App*> _registeredApps;
    std::vector<DeclarativeCardApp*> _declarativeApps;
    std::vector<lv_obj_t*> _tiles;
    Preferences _cardPreferences;

    lv_obj_t* _tileview = nullptr;
    lv_obj_t* _statusBar = nullptr;
    lv_obj_t* _wifiLabel = nullptr;
    lv_obj_t* _timeLabel = nullptr;
    lv_obj_t* _notificationLabel = nullptr;
    lv_obj_t* _wifiPanel = nullptr;
    lv_obj_t* _wifiTitle = nullptr;
    lv_obj_t* _wifiStatus = nullptr;
    lv_obj_t* _wifiList = nullptr;
    lv_obj_t* _wifiRefreshButton = nullptr;
    lv_obj_t* _wifiPassword = nullptr;
    lv_obj_t* _wifiKeyboard = nullptr;
    lv_obj_t* _wifiConnectButton = nullptr;
    lv_obj_t* _wifiBackButton = nullptr;
    bool _wifiPasswordView = false;
    String _selectedSSID;

    int _currentIndex = -1;
    unsigned long _lastUpdate = 0;

    static void onTileChangedEvent(lv_event_t* e);
    static void onWifiNetworkClicked(lv_event_t* e);
    static void onWifiPasswordChanged(lv_event_t* e);
    static void onWifiConnectClicked(lv_event_t* e);
    static void onWifiBackClicked(lv_event_t* e);
    static void onWifiCloseClicked(lv_event_t* e);
    static void onWifiRefreshClicked(lv_event_t* e);
    void handleTileChange(int newIndex);
    void showWifiPasswordView(const char* ssid);
    void showWifiNetworkView();
    void closeWifiPanel();
    void refreshWifiList();
    void loadCardConfig();
    void loadDeclarativeApps();
    bool isKnownApp(const char* id) const;
    static bool validDeclarativeType(const char* type);
    static bool validCardId(const char* id);
};
