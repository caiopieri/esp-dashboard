#pragma once

#include <Arduino.h>

// Volatile, bounded runtime data received from a host agent, MQTT bridge or
// another local integration. Definitions live in Preferences; values do not.
class DataStore {
public:
    static DataStore& getInstance() {
        static DataStore instance;
        return instance;
    }

    bool set(const String& namespaceName, const String& key, const String& value);
    bool get(const String& namespaceName, const String& key, String& value) const;
    void clear();
    static bool validToken(const String& token);

private:
    struct Entry {
        String namespaceName;
        String key;
        String value;
    };

    // The classic CYD has substantially less DRAM than the S3. Runtime data
    // is intentionally bounded; a host can batch updates and reuse keys.
    static constexpr size_t MAX_ENTRIES = 16;
    DataStore() = default;
    int find(const String& namespaceName, const String& key) const;
    Entry _entries[MAX_ENTRIES];
    size_t _count = 0;
};
