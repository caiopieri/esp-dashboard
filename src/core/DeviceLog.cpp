#include "DeviceLog.h"

#include <stdarg.h>

char DeviceLog::_lines[24][112] = {};
uint8_t DeviceLog::_nextLine = 0;
uint8_t DeviceLog::_lineCount = 0;

void DeviceLog::begin() {
    info("Device log started");
}

void DeviceLog::info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    write("INFO", format, args);
    va_end(args);
}

void DeviceLog::error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    write("ERROR", format, args);
    va_end(args);
}

void DeviceLog::write(const char* level, const char* format, va_list args) {
    char message[96];
    vsnprintf(message, sizeof(message), format, args);

    char line[128];
    snprintf(line, sizeof(line), "[%lu] [%s] %s", millis(), level, message);
    Serial.println(line);

    strncpy(_lines[_nextLine], line, sizeof(_lines[_nextLine]) - 1);
    _lines[_nextLine][sizeof(_lines[_nextLine]) - 1] = '\0';
    _nextLine = (_nextLine + 1) % 24;
    if (_lineCount < 24) _lineCount++;
}

String DeviceLog::snapshot() {
    String result;
    result.reserve(_lineCount * 80);

    uint8_t first = (_lineCount == 24) ? _nextLine : 0;
    for (uint8_t i = 0; i < _lineCount; i++) {
        uint8_t index = (first + i) % 24;
        result += _lines[index];
        result += '\n';
    }
    return result;
}
