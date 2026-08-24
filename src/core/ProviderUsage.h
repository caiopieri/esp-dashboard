#pragma once

#include <Arduino.h>

struct ProviderUsage {
    bool valid = false;
    bool ok = false;
    int sessionPercent = 0;
    int weeklyPercent = 0;
    int sessionResetMinutes = 0;
    int weeklyResetMinutes = 0;
    String tokens;
    String requests;
    String status;
    uint32_t updatedAt = 0;
};

class ProviderUsageStore {
public:
    static ProviderUsageStore& getInstance() {
        static ProviderUsageStore instance;
        return instance;
    }

    bool update(const char* provider, const ProviderUsage& usage) {
        const int index = providerIndex(provider);
        if (index < 0) return false;
        _usage[index] = usage;
        _usage[index].valid = true;
        _usage[index].updatedAt = millis();
        return true;
    }

    bool get(const char* provider, ProviderUsage& usage) const {
        const int index = providerIndex(provider);
        if (index < 0 || !_usage[index].valid) return false;
        usage = _usage[index];
        return true;
    }

private:
    static int providerIndex(const char* provider) {
        if (!provider) return -1;
        if (strcmp(provider, "gemini") == 0) return 0;
        if (strcmp(provider, "chatgpt") == 0 || strcmp(provider, "openai") == 0) return 1;
        if (strcmp(provider, "claude") == 0) return 2;
        return -1;
    }

    ProviderUsage _usage[3];
};
