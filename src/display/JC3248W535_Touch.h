#pragma once

#include <Arduino.h>
#include <Wire.h>

constexpr int8_t JC3248_TOUCH_SDA = 4;
constexpr int8_t JC3248_TOUCH_SCL = 8;
constexpr int8_t JC3248_TOUCH_RST = 12;
constexpr int8_t JC3248_TOUCH_INT = 3;
constexpr uint8_t JC3248_TOUCH_ADDRESS = 0x3B;

struct JC3248TouchPoint {
    uint16_t x = 0;
    uint16_t y = 0;
    bool touched = false;
};

class JC3248W535Touch {
public:
    bool begin();
    bool read(JC3248TouchPoint& point);
    void setRotation(uint8_t rotation, uint16_t width, uint16_t height);

private:
    static constexpr uint8_t READ_COMMAND_LENGTH = 11;
    static constexpr uint8_t RESPONSE_LENGTH = 8;
    static const uint8_t READ_COMMAND[READ_COMMAND_LENGTH];

    uint8_t _rotation = 1;
    uint16_t _width = 480;
    uint16_t _height = 320;
    uint8_t _stableSamples = 0;
    int16_t _lastX = 0;
    int16_t _lastY = 0;
    bool _contactActive = false;
    uint32_t _lastContactMs = 0;
};
