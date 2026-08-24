#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

class VariableStore {
public:
    static VariableStore& getInstance() {
        static VariableStore instance;
        return instance;
    }

    void begin();
    bool saveJson(const String& json);
    bool upsert(const String& name, const String& value, bool secret);
    String metadataJson() const;
    String resolve(const String& input) const;
    bool getValue(const char* name, String& value) const;

private:
    VariableStore() = default;

    bool validName(const char* name) const;
    Preferences _preferences;
    String _json;
};
