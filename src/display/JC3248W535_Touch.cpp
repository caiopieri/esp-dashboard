#include "JC3248W535_Touch.h"

#if defined(BOARD_JC3248W535EN)

const uint8_t JC3248W535Touch::READ_COMMAND[READ_COMMAND_LENGTH] = {
    0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};

bool JC3248W535Touch::begin() {
    Wire.begin(JC3248_TOUCH_SDA, JC3248_TOUCH_SCL);
    Wire.setClock(400000);

    Wire.beginTransmission(JC3248_TOUCH_ADDRESS);
    return Wire.endTransmission() == 0;
}

bool JC3248W535Touch::read(JC3248TouchPoint& point) {
    uint8_t response[RESPONSE_LENGTH] = {0};

    Wire.beginTransmission(JC3248_TOUCH_ADDRESS);
    Wire.write(READ_COMMAND, READ_COMMAND_LENGTH);
    if (Wire.endTransmission() != 0) {
        point.touched = false;
        return false;
    }

    if (Wire.requestFrom(JC3248_TOUCH_ADDRESS, RESPONSE_LENGTH) != RESPONSE_LENGTH) {
        point.touched = false;
        return false;
    }
    for (uint8_t i = 0; i < RESPONSE_LENGTH; ++i) response[i] = Wire.read();

    if (response[1] == 0) {
        point.touched = false;
        return false;
    }

    const uint16_t raw_x = ((response[2] & 0x0F) << 8) | response[3];
    const uint16_t raw_y = ((response[4] & 0x0F) << 8) | response[5];
    int32_t x = raw_x;
    int32_t y = raw_y;

    switch (_rotation) {
        case 1:
            x = raw_y;
            y = static_cast<int32_t>(_height) - 1 - raw_x;
            break;
        case 2:
            x = static_cast<int32_t>(_width) - 1 - raw_x;
            y = static_cast<int32_t>(_height) - 1 - raw_y;
            break;
        case 3:
            x = static_cast<int32_t>(_width) - 1 - raw_y;
            y = raw_x;
            break;
        default:
            break;
    }

    point.x = constrain(x, 0, static_cast<int32_t>(_width - 1));
    point.y = constrain(y, 0, static_cast<int32_t>(_height - 1));
    point.touched = true;
    return true;
}

void JC3248W535Touch::setRotation(uint8_t rotation, uint16_t width, uint16_t height) {
    _rotation = rotation & 0x03;
    _width = width;
    _height = height;
}
#endif
