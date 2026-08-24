#include "NetworkManager.h"
#include "AppManager.h"
#include "DeviceLog.h"
#include "VariableStore.h"
#include <ArduinoJson.h>
#include <time.h>

static const char CONFIG_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CYD Smart Display</title>
<style>
:root{color-scheme:dark;font-family:system-ui,sans-serif;background:#11111b;color:#cdd6f4}
body{max-width:760px;margin:0 auto;padding:18px}header{display:flex;justify-content:space-between;align-items:center}
h1{font-size:24px;margin:0 0 18px}h2{font-size:17px;margin:0 0 12px;color:#89b4fa}
section{background:#181825;border:1px solid #313244;border-radius:12px;padding:16px;margin:12px 0}
button,input,select{font:inherit;border-radius:7px;border:1px solid #45475a;padding:9px;background:#1e1e2e;color:#cdd6f4}
button{background:#89b4fa;color:#11111b;border:0;font-weight:700;cursor:pointer;margin:4px 3px 4px 0}
button.secondary{background:#45475a;color:#cdd6f4}button.danger{background:#f38ba8}
input,select{width:100%;box-sizing:border-box;margin:4px 0 8px}.row{display:flex;gap:10px;align-items:center;border-bottom:1px solid #313244;padding:9px 0}
.row:last-child{border-bottom:0}.row label{flex:1}.order{width:64px}.muted{color:#a6adc8;font-size:13px}.ok{color:#a6e3a1}.error{color:#f38ba8}
a{color:#89dceb}.pill{padding:4px 8px;border-radius:12px;background:#313244;font-size:12px}
</style></head><body><header><h1>CYD Smart Display</h1><span id="status" class="pill">carregando</span></header>
<section><h2>Wi‑Fi</h2><button id="scan" class="secondary">Procurar redes</button>
<select id="ssid"><option value="">Selecione uma rede</option></select>
<input id="password" type="password" placeholder="Senha da rede">
<button id="connect">Conectar</button><p id="wifi-msg" class="muted"></p></section>
<section><h2>Cards do carrossel</h2><p class="muted">Marque os cards ativos e defina a ordem. As alterações reiniciam o dispositivo.</p>
<div id="cards"></div><button id="save-cards">Salvar cards e reiniciar</button>
<button id="export" class="secondary">Exportar JSON</button>
<input id="import" type="file" accept="application/json"><button id="import-btn" class="secondary">Importar JSON</button>
<p id="card-msg" class="muted"></p></section>
<section><h2>Variáveis do dispositivo</h2><p class="muted">Use nomes como <code>GEMINI_API_KEY</code>. Cards e agentes podem referenciar <code>{{NOME}}</code>. Valores secretos nunca aparecem na leitura.</p>
<input id="var-name" placeholder="Nome (A-Z, 0-9 e _)"><input id="var-value" placeholder="Valor" type="password">
<label><input id="var-secret" type="checkbox" style="width:auto"> variável secreta</label><br><button id="save-var">Salvar variável</button>
<div id="variables" class="muted"></div><p id="var-msg" class="muted"></p></section>
<section><h2>Diagnóstico</h2><a href="/logs" target="_blank">Abrir logs</a> · <a href="/api/schema" target="_blank">Schema para agentes</a></section>
<script>
const $=id=>document.getElementById(id);let config=null;
async function api(url,opt={}){const r=await fetch(url,opt);const t=await r.text();let d;try{d=JSON.parse(t)}catch{d={raw:t}}if(!r.ok)throw Error(d.error||t||r.status);return d}
function message(id,text,error=false){$(id).textContent=text;$(id).className=error?'error':'muted'}
async function load(){try{const [s,c,v]=await Promise.all([api('/api/status'),api('/api/config'),api('/api/variables')]);$('status').textContent=s.connected?'Wi‑Fi '+s.ip:'Offline';config=c;renderCards();renderVars(v.variables)}catch(e){message('card-msg',e.message,true)}}
function renderCards(){const host=$('cards');host.textContent='';(config.cards||[]).forEach(c=>{const row=document.createElement('div');row.className='row';row.dataset.id=c.id;const check=document.createElement('input');check.type='checkbox';check.checked=!!c.enabled;check.style.width='auto';const label=document.createElement('label');label.textContent=(c.title||c.id)+' · '+c.id;const order=document.createElement('input');order.type='number';order.min=0;order.max=7;order.value=c.order;order.className='order';row.append(check,label,order);host.append(row)})}
function renderVars(vars){const host=$('variables');host.textContent='';vars.forEach(v=>{const p=document.createElement('p');p.textContent=v.name+' · '+(v.secret?'secreta':'texto')+' · '+(v.configured?'configurada':'vazia');host.append(p)})}
async function scan(){try{message('wifi-msg','procurando...');await api('/api/wifi/scan',{method:'POST'});let attempts=0;const timer=setInterval(async()=>{try{const d=await api('/api/wifi/scan');if(d.pending)return;clearInterval(timer);$('ssid').textContent='';(d.networks||[]).forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+' dBm)';$('ssid').append(o)});message('wifi-msg',d.failed?'Falha no scan':(d.networks.length+' redes encontradas'),d.failed)}catch(e){attempts++;if(attempts>20){clearInterval(timer);message('wifi-msg','tempo esgotado ao consultar o scan',true)}else message('wifi-msg','rádio procurando...')}} ,600)}catch(e){message('wifi-msg',e.message,true)}}
async function connect(){try{await api('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:$('ssid').value,password:$('password').value})});message('wifi-msg','conectando...')}catch(e){message('wifi-msg',e.message,true)}}
async function saveCards(){try{const rows=[...document.querySelectorAll('.row')];const cards=rows.map(r=>{const c=config.cards.find(x=>x.id===r.dataset.id);return {...c,enabled:r.querySelector('input[type=checkbox]').checked,order:Number(r.querySelector('.order').value)}});await api('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cards})});message('card-msg','salvo; reiniciando...')}catch(e){message('card-msg',e.message,true)}}
function exportConfig(){const blob=new Blob([JSON.stringify(config,null,2)],{type:'application/json'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='cyd-cards.json';a.click();URL.revokeObjectURL(a.href)}
async function importConfig(){const f=$('import').files[0];if(!f)return message('card-msg','selecione um JSON',true);try{const d=JSON.parse(await f.text());await api('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});message('card-msg','JSON salvo; reiniciando...')}catch(e){message('card-msg',e.message,true)}}
async function saveVar(){try{await api('/api/variables',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:$('var-name').value,value:$('var-value').value,secret:$('var-secret').checked})});message('var-msg','variável salva');$('var-value').value='';load()}catch(e){message('var-msg',e.message,true)}}
$('scan').onclick=scan;$('connect').onclick=connect;$('save-cards').onclick=saveCards;$('export').onclick=exportConfig;$('import-btn').onclick=importConfig;$('save-var').onclick=saveVar;load();
</script></body></html>
)HTML";

static const char API_SCHEMA[] PROGMEM = R"JSON({"name":"cyd-smart-display","version":1,"endpoints":{"config":{"get":"GET /api/config","save":"POST /api/config","body":{"cards":[{"id":"gemini_usage|clock_system","enabled":true,"order":0,"variables":["GEMINI_API_KEY"],"template":"{{GEMINI_API_KEY}}"}]}},"variables":{"list":"GET /api/variables","upsert":"POST /api/variables","body":{"name":"A_Z_NAME","value":"secret-or-text","secret":true}},"wifi":{"scan_start":"POST /api/wifi/scan","scan_poll":"GET /api/wifi/scan","connect":"POST /api/wifi"}},"rules":{"variable_name":"^[A-Za-z0-9_]{1,32}$","max_variables":16,"max_value_length":512,"secret_values_never_returned":true,"card_changes_require_restart":true},"templates":"Use {{VARIABLE_NAME}} in card template fields; runtime consumers should resolve them through VariableStore."})JSON";

void NetworkManager::begin(const char* ssid, const char* password) {
    VariableStore::getInstance().begin();
    _preferences.begin("wifi", false);
    WiFi.mode(WIFI_STA);

    String savedSSID = _preferences.getString("ssid", "");
    String savedPassword = _preferences.getString("password", "");
    String configuredSSID = savedSSID.length() > 0 ? savedSSID : String(ssid ? ssid : "");
    String configuredPassword = savedSSID.length() > 0 ? savedPassword : String(password ? password : "");

    // Ignore the template values from config.h until the user chooses a network.
    if (configuredSSID.length() > 0 && configuredSSID != "SEU_WIFI_AQUI") {
        WiFi.begin(configuredSSID.c_str(), configuredPassword.c_str());
        LOG_INFO("WiFi connecting to %s", configuredSSID.c_str());
        _connecting = true;
        _connectStarted = millis();
    } else {
        LOG_INFO("WiFi has no saved network configured");
    }

    // Setup ArduinoOTA
    ArduinoOTA.setHostname("cyd-smart-display");

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        Serial.println("[OTA] Start updating " + type);
    });

    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] End. Rebooting...");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();

    _webServer.on("/", HTTP_GET, [this]() {
        _webServer.send_P(200, "text/html; charset=utf-8", CONFIG_PAGE);
    });

    _webServer.on("/api/schema", HTTP_GET, [this]() {
        _webServer.send_P(200, "application/json; charset=utf-8", API_SCHEMA);
    });

    _webServer.on("/api/config", HTTP_GET, [this]() {
        _webServer.send(200, "application/json; charset=utf-8", AppManager::getInstance().getCardConfigJson());
    });

    _webServer.on("/api/config", HTTP_POST, [this]() {
        if (!_webServer.hasArg("plain") || _webServer.arg("plain").length() > 2048) {
            _webServer.send(413, "application/json", "{\"error\":\"config too large\"}");
            return;
        }
        if (!AppManager::getInstance().saveCardConfigJson(_webServer.arg("plain"))) {
            _webServer.send(400, "application/json", "{\"error\":\"invalid card config\"}");
            return;
        }
        _restartPending = true;
        _restartAt = millis() + 800;
        _webServer.send(200, "application/json", "{\"saved\":true,\"restarting\":true}");
    });

    _webServer.on("/api/variables", HTTP_GET, [this]() {
        _webServer.send(200, "application/json; charset=utf-8", VariableStore::getInstance().metadataJson());
    });

    _webServer.on("/api/variables", HTTP_POST, [this]() {
        if (!_webServer.hasArg("plain") || _webServer.arg("plain").length() > 768) {
            _webServer.send(413, "application/json", "{\"error\":\"variable request too large\"}");
            return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, _webServer.arg("plain")) != DeserializationError::Ok) {
            _webServer.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
            return;
        }
        String name = doc["name"] | "";
        String value = doc["value"] | "";
        bool secret = doc["secret"] | false;
        if (!VariableStore::getInstance().upsert(name, value, secret)) {
            _webServer.send(400, "application/json", "{\"error\":\"invalid variable\"}");
            return;
        }
        _webServer.send(200, "application/json", "{\"saved\":true}");
    });

    _webServer.on("/api/wifi", HTTP_POST, [this]() {
        if (!_webServer.hasArg("plain") || _webServer.arg("plain").length() > 768) {
            _webServer.send(413, "application/json", "{\"error\":\"request too large\"}");
            return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, _webServer.arg("plain")) != DeserializationError::Ok) {
            _webServer.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
            return;
        }
        String requestedSSID = doc["ssid"] | "";
        String requestedPassword = doc["password"] | "";
        if (requestedSSID.length() == 0 || requestedSSID.length() > 32 || requestedPassword.length() > 63) {
            _webServer.send(400, "application/json", "{\"error\":\"invalid WiFi credentials\"}");
            return;
        }
        _requestedSSID = requestedSSID;
        _requestedPassword = requestedPassword;
        _wifiConnectRequested = true;
        _webServer.send(202, "application/json", "{\"accepted\":true}");
    });

    _webServer.on("/api/wifi/scan", HTTP_POST, [this]() {
        _scanStartRequested = true;
        _webServer.send(202, "application/json", "{\"pending\":true}");
    });

    _webServer.on("/api/wifi/scan", HTTP_GET, [this]() {
        pollWifiScan();
        JsonDocument doc;
        doc["pending"] = isWifiScanPending();
        if (!isWifiScanPending()) {
            doc["failed"] = _scanFailed;
            JsonArray networks = doc["networks"].to<JsonArray>();
            for (int i = 0; i < _scanCount && i < 16; ++i) {
                JsonObject network = networks.add<JsonObject>();
                network["ssid"] = getWifiScanSSID(i);
                network["rssi"] = getWifiScanRSSI(i);
            }
        }
        String response;
        serializeJson(doc, response);
        _webServer.send(200, "application/json; charset=utf-8", response);
    });

    _webServer.on("/logs", HTTP_GET, [this]() {
        _webServer.send(200, "text/plain; charset=utf-8", DeviceLog::snapshot());
    });
    _webServer.on("/api/status", HTTP_GET, [this]() {
        String body = "{\"connected\":" + String(_connected ? "true" : "false") +
                      ",\"ip\":\"" + WiFi.localIP().toString() +
                      "\",\"scan_count\":" + String(_scanCount) + "}";
        _webServer.send(200, "application/json", body);
    });
    _webServer.begin();
    LOG_INFO("HTTP log server ready on port 80");

    // NTP Time configuration (UTC-3 for Brazil / America/Sao_Paulo: -3 * 3600 = -10800)
    configTime(-10800, 0, "pool.ntp.org", "time.nist.gov");
}

void NetworkManager::handle() {
    if (_restartPending && static_cast<long>(millis() - _restartAt) >= 0) {
        LOG_INFO("Restart requested by web panel");
        delay(50);
        ESP.restart();
    }

    ArduinoOTA.handle();
    _webServer.handleClient();

    // Start radio operations only after the HTTP response has been sent. A
    // scan or WiFi.begin() can temporarily interrupt the current TCP socket.
    if (_scanStartRequested) {
        _scanStartRequested = false;
        startWifiScan();
    }
    if (_wifiConnectRequested) {
        _wifiConnectRequested = false;
        connectToWifi(_requestedSSID.c_str(), _requestedPassword.c_str());
        _requestedSSID = "";
        _requestedPassword = "";
    }

    unsigned long now = millis();
    if (now - _lastWifiCheck > 500) {
        _lastWifiCheck = now;
        bool currentlyConnected = (WiFi.status() == WL_CONNECTED);

        if (currentlyConnected != _connected) {
            _connected = currentlyConnected;
            if (_connected) {
                if (_connecting && _pendingSSID.length() > 0) {
                    _preferences.putString("ssid", _pendingSSID);
                    _preferences.putString("password", _pendingPassword);
                    _connecting = false;
                }
                LOG_INFO("WiFi connected, IP=%s, RSSI=%d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
                AppManager::getInstance().setWifiStatus(true, WiFi.RSSI());
            } else {
                LOG_INFO("WiFi disconnected");
                AppManager::getInstance().setWifiStatus(false);
            }
        }

        if (_connecting && !currentlyConnected && now - _connectStarted > 15000) {
            _connecting = false;
            LOG_ERROR("WiFi connection timeout for %s", _pendingSSID.c_str());
            AppManager::getInstance().setWifiStatus(false);
        }

        if (_connected && now - _lastClockUpdate > 3000) {
            _lastClockUpdate = now;
            AppManager::getInstance().setStatusBarText(getFormattedTime().c_str());
        }
    }
}

void NetworkManager::startWifiScan() {
    if (_scanPending) {
        LOG_INFO("WiFi scan already pending");
        return;
    }

    // The radio can briefly reject a scan while the STA interface is
    // changing state. Keep the request pending so pollWifiScan() can retry.
    WiFi.mode(WIFI_STA);
    if (WiFi.status() != WL_CONNECTED) {
        // A previous WiFi.begin() may still be associating. Cancel only that
        // offline attempt; an established connection is left untouched.
        WiFi.disconnect(false, false);
        _connecting = false;
        delay(100);
    }
    WiFi.scanDelete();
    _scanPending = true;
    _scanFailed = false;
    _scanCount = 0;
    _scanRetries = 0;
    _scanRetryAt = millis();
    int16_t result = WiFi.scanNetworks(true, false, false, 500);
    LOG_INFO("WiFi scan start, result=%d, status=%d", result, WiFi.status());
}

bool NetworkManager::pollWifiScan() {
    if (!_scanPending) return false;

    int16_t result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) return false;

    if (result > 0) {
        _scanFailed = false;
        _scanCount = result;
        _scanPending = false;
        LOG_INFO("WiFi scan complete, networks=%d", _scanCount);
        return true;
    }

    // The ESP32 Arduino core can report zero when its async timeout races
    // with the scan-complete event. Re-run the proven synchronous API before
    // exposing an empty list to the user.
    if (result == 0) {
        WiFi.scanDelete();
        int16_t fallbackResult = WiFi.scanNetworks(false, false, false, 300);
        if (fallbackResult >= 0) {
            _scanFailed = false;
            _scanCount = fallbackResult;
            _scanPending = false;
            LOG_INFO("WiFi zero-result fallback complete, networks=%d", _scanCount);
            return true;
        }
        LOG_INFO("WiFi zero-result fallback failed, result=%d", fallbackResult);
    }

    // -2 means that the radio rejected or lost the scan request. Retry a few
    // times before exposing an error in the UI; this is common during STA
    // startup or immediately after a connection attempt.
    const unsigned long now = millis();
    if (_scanRetries < 3 && now - _scanRetryAt >= 250) {
        ++_scanRetries;
        WiFi.scanDelete();
        int16_t retryResult = WiFi.scanNetworks(true, false, false, 500);
        _scanRetryAt = now;
        LOG_INFO("WiFi scan retry=%u, result=%d, status=%d", _scanRetries, retryResult, WiFi.status());
        return false;
    }

    // Last resort for ESP32 core/driver states that reject the async API.
    // This is intentionally only used after the retries above, so the normal
    // UI remains responsive while scanning.
    WiFi.scanDelete();
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(false, false);
        delay(100);
    }
    int16_t fallbackResult = WiFi.scanNetworks(false, false, false, 300);
    if (fallbackResult >= 0) {
        _scanFailed = false;
        _scanCount = fallbackResult;
        _scanPending = false;
        LOG_INFO("WiFi synchronous fallback complete, networks=%d", _scanCount);
        return true;
    }

    _scanFailed = true;
    _scanCount = 0;
    _scanPending = false;
    LOG_ERROR("WiFi scan failed after fallback, retries=%u, result=%d, fallback=%d, status=%d", _scanRetries, result, fallbackResult, WiFi.status());
    return true;
}

bool NetworkManager::isWifiScanPending() const {
    return _scanPending;
}

int NetworkManager::getWifiScanCount() const {
    return _scanCount;
}

bool NetworkManager::wifiScanFailed() const {
    return _scanFailed;
}

String NetworkManager::getWifiScanSSID(int index) const {
    if (index < 0 || index >= _scanCount) return String();
    return WiFi.SSID(index);
}

int NetworkManager::getWifiScanRSSI(int index) const {
    if (index < 0 || index >= _scanCount) return 0;
    return WiFi.RSSI(index);
}

bool NetworkManager::connectToWifi(const char* ssid, const char* password) {
    if (ssid == nullptr || strlen(ssid) == 0) return false;

    _pendingSSID = ssid;
    _pendingPassword = password ? password : "";
    _connecting = true;
    _connectStarted = millis();
    _connected = false;

    WiFi.disconnect(false, false);
    WiFi.begin(_pendingSSID.c_str(), _pendingPassword.c_str());
    LOG_INFO("WiFi selected network=%s, password_length=%u", _pendingSSID.c_str(), _pendingPassword.length());
    return true;
}

bool NetworkManager::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

String NetworkManager::getFormattedTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 50)) {
        return "--:--";
    }
    char timeStr[10];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    return String(timeStr);
}
