#pragma once

#include <Arduino.h>
#include <stdarg.h>

class DeviceLog {
public:
    static void begin();
    static void info(const char* format, ...);
    static void error(const char* format, ...);
    static String snapshot();

private:
    static void write(const char* level, const char* format, va_list args);
    static char _lines[24][112];
    static uint8_t _nextLine;
    static uint8_t _lineCount;
};

#define LOG_INFO(...) DeviceLog::info(__VA_ARGS__)
#define LOG_ERROR(...) DeviceLog::error(__VA_ARGS__)
