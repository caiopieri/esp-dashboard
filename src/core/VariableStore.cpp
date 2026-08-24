#include "VariableStore.h"
#include "DeviceLog.h"

void VariableStore::begin() {
    _preferences.begin("vars", false);
    _json = _preferences.getString("data", "{\"variables\":[]}");

    JsonDocument doc;
    if (deserializeJson(doc, _json) != DeserializationError::Ok || !doc["variables"].is<JsonArray>()) {
        _json = "{\"variables\":[]}";
        _preferences.putString("data", _json);
        LOG_ERROR("Variable store invalid, reset to empty");
    }
}

bool VariableStore::validName(const char* name) const {
    if (!name || name[0] == '\0' || strlen(name) > 32) return false;
    for (const char* p = name; *p; ++p) {
        const bool allowed = (*p >= 'A' && *p <= 'Z') ||
                             (*p >= 'a' && *p <= 'z') ||
                             (*p >= '0' && *p <= '9') || *p == '_';
        if (!allowed) return false;
    }
    return true;
}

bool VariableStore::saveJson(const String& json) {
    if (json.length() == 0 || json.length() > 4096) return false;

    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;
    JsonArray variables = doc["variables"].as<JsonArray>();
    if (variables.isNull() || variables.size() > 16) return false;

    JsonDocument normalized;
    JsonArray cleanVariables = normalized["variables"].to<JsonArray>();
    for (JsonObject variable : variables) {
        const char* name = variable["name"] | "";
        const char* value = variable["value"] | "";
        bool secret = variable["secret"] | false;
        if (!validName(name) || strlen(value) > 512) return false;

        for (JsonObject existing : cleanVariables) {
            if (strcmp(existing["name"] | "", name) == 0) return false;
        }

        JsonObject clean = cleanVariables.add<JsonObject>();
        clean["name"] = name;
        clean["value"] = value;
        clean["secret"] = secret;
    }

    String normalizedJson;
    serializeJson(normalized, normalizedJson);
    _preferences.putString("data", normalizedJson);
    _json = normalizedJson;
    LOG_INFO("Variable store saved, count=%u", cleanVariables.size());
    return true;
}

bool VariableStore::upsert(const String& name, const String& value, bool secret) {
    if (!validName(name.c_str()) || value.length() > 512) return false;

    JsonDocument doc;
    if (deserializeJson(doc, _json) != DeserializationError::Ok) return false;
    JsonArray variables = doc["variables"].as<JsonArray>();
    if (variables.isNull()) return false;

    for (JsonObject variable : variables) {
        if (strcmp(variable["name"] | "", name.c_str()) == 0) {
            variable["value"] = value;
            variable["secret"] = secret;
            String updated;
            serializeJson(doc, updated);
            return saveJson(updated);
        }
    }

    if (variables.size() >= 16) return false;
    JsonObject variable = variables.add<JsonObject>();
    variable["name"] = name;
    variable["value"] = value;
    variable["secret"] = secret;
    String updated;
    serializeJson(doc, updated);
    return saveJson(updated);
}

String VariableStore::metadataJson() const {
    JsonDocument source;
    JsonDocument output;
    JsonArray result = output["variables"].to<JsonArray>();
    if (deserializeJson(source, _json) == DeserializationError::Ok) {
        for (JsonObject variable : source["variables"].as<JsonArray>()) {
            JsonObject item = result.add<JsonObject>();
            item["name"] = variable["name"] | "";
            item["secret"] = variable["secret"] | false;
            item["configured"] = strlen(variable["value"] | "") > 0;
        }
    }

    String outputJson;
    serializeJson(output, outputJson);
    return outputJson;
}

bool VariableStore::getValue(const char* name, String& value) const {
    if (!validName(name)) return false;

    JsonDocument doc;
    if (deserializeJson(doc, _json) != DeserializationError::Ok) return false;
    for (JsonObject variable : doc["variables"].as<JsonArray>()) {
        if (strcmp(variable["name"] | "", name) == 0) {
            value = variable["value"] | "";
            return true;
        }
    }
    return false;
}

String VariableStore::resolve(const String& input) const {
    String result = input;
    JsonDocument doc;
    if (deserializeJson(doc, _json) != DeserializationError::Ok) return result;

    for (JsonObject variable : doc["variables"].as<JsonArray>()) {
        const char* name = variable["name"] | "";
        String token = "{{" + String(name) + "}}";
        result.replace(token, String(variable["value"] | ""));
    }
    return result;
}
