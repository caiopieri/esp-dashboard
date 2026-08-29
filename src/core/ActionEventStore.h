#pragma once

#include <Arduino.h>

// A tiny one-event mailbox between the touch UI and a trusted companion. The
// card only contributes an opaque allowlist ID; it never contributes a shell
// command, URL or arguments.
class ActionEventStore {
public:
    static ActionEventStore& getInstance() {
        static ActionEventStore instance;
        return instance;
    }

    void publish(const char* actionId);
    String getJson() const;
    bool acknowledge(uint32_t sequence);

private:
    ActionEventStore() = default;

#if defined(BOARD_JC3248W535EN)
    char _actionId[33] = {};
    uint32_t _sequence = 0;
    uint32_t _createdAt = 0;
    bool _pending = false;
#endif
};
