#include "AppManager.h"
#include "NetworkManager.h"
#include "DeviceLog.h"

void AppManager::registerApp(App* app) {
    if (app) {
        _registeredApps.push_back(app);
        _apps.push_back(app);
    }
}

bool AppManager::isKnownApp(const char* id) const {
    if (!id) return false;
    for (App* app : _registeredApps) {
        if (strcmp(app->getId(), id) == 0) return true;
    }
    return false;
}

void AppManager::loadCardConfig() {
    String stored = _cardPreferences.getString("config", "");
    if (stored.length() == 0) return;

    JsonDocument doc;
    if (deserializeJson(doc, stored) != DeserializationError::Ok) {
        LOG_ERROR("Card config JSON invalid, using defaults");
        return;
    }

    JsonArray cards = doc["cards"].as<JsonArray>();
    if (cards.isNull()) return;

    struct CardOrder {
        App* app;
        int order;
    };
    std::vector<CardOrder> enabled;

    for (JsonObject card : cards) {
        const char* id = card["id"] | "";
        if (!isKnownApp(id) || !(card["enabled"] | true)) continue;

        for (App* app : _registeredApps) {
            if (strcmp(app->getId(), id) == 0) {
                enabled.push_back({app, card["order"] | 0});
                break;
            }
        }
    }

    std::sort(enabled.begin(), enabled.end(), [](const CardOrder& a, const CardOrder& b) {
        return a.order < b.order;
    });

    _apps.clear();
    for (const CardOrder& item : enabled) {
        bool duplicate = false;
        for (App* app : _apps) {
            if (app == item.app) duplicate = true;
        }
        if (!duplicate) _apps.push_back(item.app);
    }

    // Never leave the carousel empty because of a malformed or over-filtered
    // configuration.
    if (_apps.empty() && !_registeredApps.empty()) {
        _apps.push_back(_registeredApps.front());
    }
    LOG_INFO("Card config loaded, active=%u", _apps.size());
}

String AppManager::getCardConfigJson() {
    JsonDocument doc;
    JsonArray cards = doc["cards"].to<JsonArray>();
    JsonDocument storedDoc;
    deserializeJson(storedDoc, _cardPreferences.getString("config", ""));
    JsonArray storedCards = storedDoc["cards"].as<JsonArray>();

    for (size_t i = 0; i < _registeredApps.size(); ++i) {
        App* app = _registeredApps[i];
        JsonObject card = cards.add<JsonObject>();
        card["id"] = app->getId();
        card["title"] = app->getTitle();
        int activeIndex = -1;
        for (size_t j = 0; j < _apps.size(); ++j) {
            if (_apps[j] == app) activeIndex = static_cast<int>(j);
        }
        card["enabled"] = activeIndex >= 0;
        card["order"] = activeIndex >= 0 ? activeIndex : static_cast<int>(i);

        for (JsonObject stored : storedCards) {
            if (strcmp(stored["id"] | "", app->getId()) != 0) continue;
            if (stored["template"].is<const char*>()) card["template"] = stored["template"];
            JsonArray refs = stored["variables"].as<JsonArray>();
            if (!refs.isNull()) {
                JsonArray outputRefs = card["variables"].to<JsonArray>();
                for (const char* ref : refs) outputRefs.add(ref);
            }
            break;
        }
    }

    String output;
    serializeJson(doc, output);
    return output;
}

bool AppManager::saveCardConfigJson(const String& json) {
    if (json.length() == 0 || json.length() > 2048) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;
    JsonArray cards = doc["cards"].as<JsonArray>();
    if (cards.isNull() || cards.size() == 0 || cards.size() > _registeredApps.size()) return false;

    bool seen[8] = {};
    JsonDocument normalized;
    JsonArray normalizedCards = normalized["cards"].to<JsonArray>();
    for (JsonObject card : cards) {
        const char* id = card["id"] | "";
        int order = card["order"] | -1;
        bool enabled = card["enabled"] | false;
        if (!isKnownApp(id) || order < 0 || order >= 8) return false;

        for (JsonObject existing : normalizedCards) {
            if (strcmp(existing["id"] | "", id) == 0) return false;
        }
        if (enabled && seen[order]) return false;
        if (enabled) seen[order] = true;

        JsonObject clean = normalizedCards.add<JsonObject>();
        clean["id"] = id;
        clean["enabled"] = enabled;
        clean["order"] = order;

        const char* templateText = card["template"] | "";
        if (strlen(templateText) > 512) return false;
        if (templateText[0] != '\0') clean["template"] = templateText;

        JsonArray refs = card["variables"].as<JsonArray>();
        if (!refs.isNull()) {
            if (refs.size() > 8) return false;
            JsonArray cleanRefs = clean["variables"].to<JsonArray>();
            for (const char* ref : refs) {
                if (!ref || strlen(ref) == 0 || strlen(ref) > 32) return false;
                cleanRefs.add(ref);
            }
        }
    }

    String normalizedJson;
    serializeJson(normalized, normalizedJson);
    _cardPreferences.putString("config", normalizedJson);
    LOG_INFO("Card config saved, reboot required");
    return true;
}

void AppManager::onTileChangedEvent(lv_event_t* e) {
    AppManager* mgr = (AppManager*)lv_event_get_user_data(e);
    lv_obj_t* tv = lv_event_get_target(e);
    lv_obj_t* active_tile = lv_tileview_get_tile_act(tv);

    for (size_t i = 0; i < mgr->_tiles.size(); i++) {
        if (mgr->_tiles[i] == active_tile) {
            mgr->handleTileChange(i);
            break;
        }
    }
}

void AppManager::handleTileChange(int newIndex) {
    if (newIndex == _currentIndex || newIndex < 0 || newIndex >= (int)_apps.size()) {
        return;
    }

    Serial.printf("[AppManager] Switching to App #%d: %s\n", newIndex, _apps[newIndex]->getTitle());

    // Stop current app
    if (_currentIndex >= 0 && _currentIndex < (int)_apps.size()) {
        _apps[_currentIndex]->setActive(false);
        _apps[_currentIndex]->onStop();
    }

    _currentIndex = newIndex;

    // Start new app inside its tile container
    _apps[_currentIndex]->setActive(true);
    _apps[_currentIndex]->onStart(_tiles[_currentIndex]);

}

void AppManager::onWifiNetworkClicked(lv_event_t* e) {
    AppManager* mgr = static_cast<AppManager*>(lv_event_get_user_data(e));
    lv_obj_t* button = lv_event_get_target(e);
    lv_obj_t* label = lv_obj_get_child(button, 0);
    if (mgr && label) {
        mgr->showWifiPasswordView(lv_label_get_text(label));
    }
}

void AppManager::onWifiPasswordChanged(lv_event_t* e) {
    (void)e;
}

void AppManager::onWifiConnectClicked(lv_event_t* e) {
    AppManager* mgr = static_cast<AppManager*>(lv_event_get_user_data(e));
    if (!mgr || !mgr->_wifiPassword) return;

    const char* password = lv_textarea_get_text(mgr->_wifiPassword);
    if (NetworkManager::getInstance().connectToWifi(mgr->_selectedSSID.c_str(), password)) {
        lv_label_set_text(mgr->_wifiStatus, "Conectando...");
        lv_obj_add_state(mgr->_wifiConnectButton, LV_STATE_DISABLED);
    }
}

void AppManager::onWifiBackClicked(lv_event_t* e) {
    AppManager* mgr = static_cast<AppManager*>(lv_event_get_user_data(e));
    if (mgr) mgr->showWifiNetworkView();
}

void AppManager::onWifiCloseClicked(lv_event_t* e) {
    AppManager* mgr = static_cast<AppManager*>(lv_event_get_user_data(e));
    if (mgr) mgr->closeWifiPanel();
}

void AppManager::onWifiRefreshClicked(lv_event_t* e) {
    AppManager* mgr = static_cast<AppManager*>(lv_event_get_user_data(e));
    if (!mgr) return;

    mgr->showWifiNetworkView();
    NetworkManager::getInstance().startWifiScan();
    lv_label_set_text(mgr->_wifiStatus, "Procurando redes...");
}

void AppManager::openWifiPanel() {
    if (_wifiPanel) return;

    LOG_INFO("WiFi panel opened");

    _wifiPanel = lv_obj_create(lv_scr_act());
    lv_obj_set_size(_wifiPanel, 320, 240);
    lv_obj_center(_wifiPanel);
    lv_obj_set_style_bg_color(_wifiPanel, lv_color_hex(0x11111B), 0);
    lv_obj_set_style_border_color(_wifiPanel, lv_color_hex(0x45475A), 0);
    lv_obj_set_style_border_width(_wifiPanel, 1, 0);
    lv_obj_set_style_radius(_wifiPanel, 0, 0);
    lv_obj_set_style_pad_all(_wifiPanel, 4, 0);
    lv_obj_clear_flag(_wifiPanel, LV_OBJ_FLAG_SCROLLABLE);

    _wifiTitle = lv_label_create(_wifiPanel);
    lv_label_set_text(_wifiTitle, "Selecionar WiFi");
    lv_obj_set_style_text_color(_wifiTitle, lv_color_hex(0xCDD6F4), 0);
    lv_obj_align(_wifiTitle, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t* closeButton = lv_btn_create(_wifiPanel);
    lv_obj_set_size(closeButton, 52, 22);
    lv_obj_align(closeButton, LV_ALIGN_TOP_RIGHT, -2, 0);
    lv_obj_t* closeLabel = lv_label_create(closeButton);
    lv_label_set_text(closeLabel, "Fechar");
    lv_obj_center(closeLabel);
    lv_obj_add_event_cb(closeButton, onWifiCloseClicked, LV_EVENT_CLICKED, this);

    _wifiRefreshButton = lv_btn_create(_wifiPanel);
    lv_obj_set_size(_wifiRefreshButton, 60, 22);
    lv_obj_align(_wifiRefreshButton, LV_ALIGN_TOP_LEFT, 2, 0);
    lv_obj_t* refreshLabel = lv_label_create(_wifiRefreshButton);
    lv_label_set_text(refreshLabel, "Atualizar");
    lv_obj_center(refreshLabel);
    lv_obj_add_event_cb(_wifiRefreshButton, onWifiRefreshClicked, LV_EVENT_CLICKED, this);

    _wifiStatus = lv_label_create(_wifiPanel);
    lv_label_set_text(_wifiStatus, "Procurando redes...");
    lv_obj_set_style_text_color(_wifiStatus, lv_color_hex(0xA6ADC8), 0);
    lv_obj_align(_wifiStatus, LV_ALIGN_TOP_LEFT, 6, 28);

    _wifiList = lv_obj_create(_wifiPanel);
    lv_obj_set_size(_wifiList, 308, 180);
    lv_obj_align(_wifiList, LV_ALIGN_TOP_LEFT, 2, 50);
    lv_obj_set_style_bg_opa(_wifiList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_wifiList, 0, 0);
    lv_obj_set_style_pad_all(_wifiList, 2, 0);
    lv_obj_set_flex_flow(_wifiList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_wifiList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    _wifiPassword = lv_textarea_create(_wifiPanel);
    lv_obj_set_size(_wifiPassword, 308, 34);
    lv_obj_align(_wifiPassword, LV_ALIGN_TOP_LEFT, 2, 50);
    lv_textarea_set_one_line(_wifiPassword, true);
    lv_textarea_set_password_mode(_wifiPassword, true);
    lv_textarea_set_placeholder_text(_wifiPassword, "Senha da rede");
    lv_obj_add_event_cb(_wifiPassword, onWifiPasswordChanged, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_flag(_wifiPassword, LV_OBJ_FLAG_HIDDEN);

    _wifiBackButton = lv_btn_create(_wifiPanel);
    lv_obj_set_size(_wifiBackButton, 90, 30);
    lv_obj_align(_wifiBackButton, LV_ALIGN_TOP_LEFT, 2, 90);
    lv_obj_t* backLabel = lv_label_create(_wifiBackButton);
    lv_label_set_text(backLabel, "Voltar");
    lv_obj_center(backLabel);
    lv_obj_add_event_cb(_wifiBackButton, onWifiBackClicked, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(_wifiBackButton, LV_OBJ_FLAG_HIDDEN);

    _wifiConnectButton = lv_btn_create(_wifiPanel);
    lv_obj_set_size(_wifiConnectButton, 110, 30);
    lv_obj_align(_wifiConnectButton, LV_ALIGN_TOP_RIGHT, -2, 90);
    lv_obj_t* connectLabel = lv_label_create(_wifiConnectButton);
    lv_label_set_text(connectLabel, "Conectar");
    lv_obj_center(connectLabel);
    lv_obj_add_event_cb(_wifiConnectButton, onWifiConnectClicked, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(_wifiConnectButton, LV_OBJ_FLAG_HIDDEN);

    _wifiKeyboard = lv_keyboard_create(_wifiPanel);
    lv_obj_set_size(_wifiKeyboard, 320, 116);
    lv_obj_align(_wifiKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(_wifiKeyboard, _wifiPassword);
    lv_obj_add_flag(_wifiKeyboard, LV_OBJ_FLAG_HIDDEN);

    _wifiPasswordView = false;
    NetworkManager::getInstance().startWifiScan();
}

void AppManager::showWifiPasswordView(const char* ssid) {
    if (!_wifiPanel || !ssid) return;

    _selectedSSID = ssid;
    LOG_INFO("WiFi network selected=%s", _selectedSSID.c_str());
    _wifiPasswordView = true;
    lv_label_set_text_fmt(_wifiStatus, "Senha para: %s", _selectedSSID.c_str());
    lv_obj_add_flag(_wifiList, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_wifiRefreshButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_wifiPassword, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_wifiBackButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_wifiConnectButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(_wifiConnectButton, LV_STATE_DISABLED);
    lv_obj_clear_flag(_wifiKeyboard, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(_wifiPassword, "");
    lv_textarea_set_cursor_pos(_wifiPassword, LV_TEXTAREA_CURSOR_LAST);
    lv_keyboard_set_textarea(_wifiKeyboard, _wifiPassword);
}

void AppManager::showWifiNetworkView() {
    if (!_wifiPanel) return;

    _wifiPasswordView = false;
    lv_obj_clear_flag(_wifiList, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_wifiRefreshButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_wifiPassword, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_wifiBackButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_wifiConnectButton, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_wifiKeyboard, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(_wifiStatus, "Selecione uma rede");
}

void AppManager::closeWifiPanel() {
    if (!_wifiPanel) return;

    lv_obj_del(_wifiPanel);
    _wifiPanel = nullptr;
    _wifiTitle = nullptr;
    _wifiStatus = nullptr;
    _wifiList = nullptr;
    _wifiRefreshButton = nullptr;
    _wifiPassword = nullptr;
    _wifiKeyboard = nullptr;
    _wifiConnectButton = nullptr;
    _wifiBackButton = nullptr;
    _selectedSSID = "";
    _wifiPasswordView = false;
}

void AppManager::refreshWifiList() {
    if (!_wifiList || _wifiPasswordView) return;

    lv_obj_clean(_wifiList);
    if (NetworkManager::getInstance().wifiScanFailed()) {
        lv_obj_t* errorLabel = lv_label_create(_wifiList);
        lv_label_set_text(errorLabel, "Falha no scan. Toque em Atualizar.");
        lv_obj_set_style_text_color(errorLabel, lv_color_hex(0xF38BA8), 0);
        lv_label_set_text(_wifiStatus, "Falha ao procurar redes");
        return;
    }

    int count = NetworkManager::getInstance().getWifiScanCount();
    if (count <= 0) {
        lv_obj_t* emptyLabel = lv_label_create(_wifiList);
        lv_label_set_text(emptyLabel, "Nenhuma rede encontrada");
        lv_obj_set_style_text_color(emptyLabel, lv_color_hex(0xA6ADC8), 0);
        lv_label_set_text(_wifiStatus, "Nenhuma rede encontrada");
        return;
    }

    lv_label_set_text_fmt(_wifiStatus, "%d redes encontradas", count);
    for (int i = 0; i < count && i < 8; ++i) {
        String ssid = NetworkManager::getInstance().getWifiScanSSID(i);
        if (ssid.length() == 0) continue;

        lv_obj_t* button = lv_btn_create(_wifiList);
        lv_obj_set_width(button, 296);
        lv_obj_set_height(button, 28);
        // Keep the SSID as the first child: the click callback reads only
        // this label. RSSI is a separate visual label and must not become
        // part of the network name sent to WiFi.begin().
        lv_obj_t* label = lv_label_create(button);
        lv_label_set_text(label, ssid.c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(label, 220);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 4, 0);

        lv_obj_t* rssiLabel = lv_label_create(button);
        String rssiText = String(NetworkManager::getInstance().getWifiScanRSSI(i)) + " dBm";
        lv_label_set_text(rssiLabel, rssiText.c_str());
        lv_obj_align(rssiLabel, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_add_event_cb(button, onWifiNetworkClicked, LV_EVENT_CLICKED, this);
    }
}

void AppManager::init() {
    _cardPreferences.begin("cards", false);
    loadCardConfig();

    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x11111B), 0);

    // 1. Status Bar (Top 24px)
    _statusBar = lv_obj_create(scr);
    lv_obj_set_size(_statusBar, 320, 24);
    lv_obj_align(_statusBar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(_statusBar, lv_color_hex(0x181825), 0);
    lv_obj_set_style_border_side(_statusBar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(_statusBar, lv_color_hex(0x313244), 0);
    lv_obj_set_style_border_width(_statusBar, 1, 0);
    lv_obj_set_style_radius(_statusBar, 0, 0);
    lv_obj_set_style_pad_all(_statusBar, 2, 0);
    lv_obj_clear_flag(_statusBar, LV_OBJ_FLAG_SCROLLABLE);

    _wifiLabel = lv_label_create(_statusBar);
    lv_label_set_text(_wifiLabel, "WiFi Offline");
    lv_obj_set_style_text_font(_wifiLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_wifiLabel, lv_color_hex(0xA6ADC8), 0);
    lv_obj_align(_wifiLabel, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_add_flag(_wifiLabel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_wifiLabel, [](lv_event_t* e) {
        (void)e;
        AppManager::getInstance().openWifiPanel();
    }, LV_EVENT_CLICKED, nullptr);

    _timeLabel = lv_label_create(_statusBar);
    lv_label_set_text(_timeLabel, "--:--");
    lv_obj_set_style_text_font(_timeLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_timeLabel, lv_color_hex(0xCDD6F4), 0);
    lv_obj_align(_timeLabel, LV_ALIGN_RIGHT_MID, -24, 0);

    _notificationLabel = lv_label_create(_statusBar);
    lv_label_set_text(_notificationLabel, "!");
    lv_obj_set_style_text_font(_notificationLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_notificationLabel, lv_color_hex(0xF38BA8), 0);
    lv_obj_align(_notificationLabel, LV_ALIGN_RIGHT_MID, -6, 0);

    // 2. Carousel Tileview (all remaining screen space)
    _tileview = lv_tileview_create(scr);
    lv_obj_set_size(_tileview, 320, 216);
    lv_obj_align(_tileview, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_color(_tileview, lv_color_hex(0x11111B), 0);
    lv_obj_set_style_border_width(_tileview, 0, 0);
    lv_obj_add_event_cb(_tileview, onTileChangedEvent, LV_EVENT_VALUE_CHANGED, this);

    // Register all apps into the carousel
    size_t total = _apps.size();
    for (size_t i = 0; i < total; i++) {
        lv_dir_t dir = LV_DIR_NONE;
        if (i > 0) dir |= LV_DIR_LEFT;
        if (i < total - 1) dir |= LV_DIR_RIGHT;

        lv_obj_t* tile = lv_tileview_add_tile(_tileview, i, 0, dir);
        lv_obj_set_style_bg_color(tile, lv_color_hex(0x11111B), 0);
        lv_obj_set_style_pad_all(tile, 2, 0);
        _tiles.push_back(tile);

    }

    // Launch first app
    if (!_apps.empty()) {
        switchToApp(0);
    }
}

void AppManager::switchToApp(size_t index) {
    if (index < _tiles.size()) {
        lv_obj_set_tile_id(_tileview, index, 0, LV_ANIM_ON);
        handleTileChange(index);
    }
}

void AppManager::update() {
    if (_wifiPanel && !_wifiPasswordView && NetworkManager::getInstance().pollWifiScan()) {
        refreshWifiList();
    }

    if (_currentIndex >= 0 && _currentIndex < (int)_apps.size()) {
        _apps[_currentIndex]->onUpdate();
    }
}

void AppManager::setStatusBarText(const char* text) {
    if (_timeLabel) {
        lv_label_set_text(_timeLabel, text);
    }
}

void AppManager::setWifiStatus(bool connected, int rssi) {
    if (_wifiLabel) {
        if (connected) {
            char buf[32];
            snprintf(buf, sizeof(buf), "WiFi %d dBm", rssi);
            lv_label_set_text(_wifiLabel, buf);
            lv_obj_set_style_text_color(_wifiLabel, lv_color_hex(0xA6E3A1), 0);
            if (_wifiPanel) closeWifiPanel();
        } else {
            lv_label_set_text(_wifiLabel, "WiFi Offline");
            lv_obj_set_style_text_color(_wifiLabel, lv_color_hex(0xF38BA8), 0);
            if (_wifiPanel && _wifiPasswordView) {
                lv_label_set_text(_wifiStatus, "Falha ao conectar");
                lv_obj_clear_state(_wifiConnectButton, LV_STATE_DISABLED);
            }
        }
    }
}
