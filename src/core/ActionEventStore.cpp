#include "ActionEventStore.h"

#include <ArduinoJson.h>
#include <cstring>

#include "DeviceLog.h"

void ActionEventStore::publish(const char* actionId) {
#if defined(BOARD_JC3248W535EN)
    if (!actionId || !actionId[0]) return;
    strncpy(_actionId, actionId, sizeof(_actionId) - 1);
    _actionId[sizeof(_actionId) - 1] = '\0';
    ++_sequence;
    if (_sequence == 0) ++_sequence;
    _createdAt = millis();
    _pending = true;
    LOG_INFO("Action event queued id=%s sequence=%lu", _actionId,
             static_cast<unsigned long>(_sequence));
#else
    (void)actionId;
#endif
}

String ActionEventStore::getJson() const {
    JsonDocument doc;
#if defined(BOARD_JC3248W535EN)
    doc["schemaVersion"] = 1;
    doc["pending"] = _pending;
    if (_pending) {
        doc["sequence"] = _sequence;
        doc["actionId"] = _actionId;
        doc["createdMs"] = _createdAt;
    }
#else
    doc["schemaVersion"] = 1;
    doc["pending"] = false;
#endif
    String output;
    serializeJson(doc, output);
    return output;
}

bool ActionEventStore::acknowledge(uint32_t sequence) {
#if defined(BOARD_JC3248W535EN)
    if (!_pending || sequence != _sequence) return false;
    _pending = false;
    _actionId[0] = '\0';
    return true;
#else
    (void)sequence;
    return false;
#endif
}
