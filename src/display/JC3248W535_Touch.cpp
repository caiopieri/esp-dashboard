#include "JC3248W535_Touch.h"

#if defined(BOARD_JC3248W535EN)

const uint8_t JC3248W535Touch::READ_COMMAND[READ_COMMAND_LENGTH] = {
    0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};

bool JC3248W535Touch::begin() {
    pinMode(JC3248_TOUCH_INT, INPUT_PULLUP);
    pinMode(JC3248_TOUCH_RST, OUTPUT);
    digitalWrite(JC3248_TOUCH_RST, LOW);
    delay(200);
    digitalWrite(JC3248_TOUCH_RST, HIGH);
    delay(200);

    Wire.begin(JC3248_TOUCH_SDA, JC3248_TOUCH_SCL);
    Wire.setClock(400000);

    Wire.beginTransmission(JC3248_TOUCH_ADDRESS);
    return Wire.endTransmission() == 0;
}

bool JC3248W535Touch::read(JC3248TouchPoint& point) {
    uint8_t response[RESPONSE_LENGTH] = {0};

    auto holdContact = [&]() -> bool {
        if (!_contactActive || millis() - _lastContactMs > 120) {
            point.touched = false;
            return false;
        }
        point.x = _lastX;
        point.y = _lastY;
        point.touched = true;
        return true;
    };

    Wire.beginTransmission(JC3248_TOUCH_ADDRESS);
    Wire.write(READ_COMMAND, READ_COMMAND_LENGTH);
    if (Wire.endTransmission(false) != 0) {
        return holdContact();
    }

    if (Wire.requestFrom(JC3248_TOUCH_ADDRESS, RESPONSE_LENGTH) != RESPONSE_LENGTH) {
        return holdContact();
    }
    for (uint8_t i = 0; i < RESPONSE_LENGTH; ++i) response[i] = Wire.read();

    // Byte 1 is the controller's point-count/status field and can remain 1
    // while idle. The actual contact state is encoded in the top two bits of
    // byte 2: 0=down, 1=up, 2=contact.
    const uint8_t event = response[2] >> 6;
    static uint32_t lastRawLog = 0;
    if (millis() - lastRawLog >= 1000) {
        Serial.printf("[TouchRaw] %02X %02X %02X %02X %02X %02X event=%u\n",
                      response[0], response[1], response[2], response[3],
                      response[4], response[5], event);
        lastRawLog = millis();
    }

    // Valid frames start with gesture byte 0x00. Repeated 0x9C frames are
    // electrical/I2C noise from this controller and must never become touch.
    if (event == 1) {
        _stableSamples = 0;
        _contactActive = false;
        point.touched = false;
        return false;
    }

    if (response[0] != 0x00 || response[1] == 0) {
        return holdContact();
    }

    if (event == 0) {
        _stableSamples = 1;
    } else if (event == 2) {
        // The controller may be polled after the DOWN frame was already
        // consumed. Start the debounce sequence from the first CONTACT
        // frame instead of dropping the whole gesture.
        if (_stableSamples < 3) ++_stableSamples;
    } else {
        _stableSamples = 0;
        return holdContact();
    }

    const uint16_t raw_x = ((response[2] & 0x0F) << 8) | response[3];
    const uint16_t raw_y = ((response[4] & 0x0F) << 8) | response[5];
    if (raw_x >= _height || raw_y >= _width) {
        _stableSamples = 0;
        return holdContact();
    }
    int32_t x = raw_x;
    int32_t y = raw_y;

    switch (_rotation) {
        case 1:
            // The touch controller is mirrored horizontally relative to the
            // landscape LVGL framebuffer.
            x = static_cast<int32_t>(_width) - 1 - raw_y;
            // The panel is rotated for landscape, but this controller's Y
            // coordinate is already top-to-bottom. Inverting it made a tap
            // on the top WiFi bar arrive at the bottom of LVGL.
            y = raw_x;
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

    const uint16_t mappedX = constrain(x, 0, static_cast<int32_t>(_width - 1));
    const uint16_t mappedY = constrain(y, 0, static_cast<int32_t>(_height - 1));

    // AXS15231B can return isolated stale frames while idle. Require three
    // nearby samples before exposing a press to LVGL.
    if (_stableSamples == 0 || abs(static_cast<int32_t>(mappedX) - _lastX) > 120 ||
        abs(static_cast<int32_t>(mappedY) - _lastY) > 40) {
        if (event == 0) {
            _stableSamples = 1;
        } else {
            _stableSamples = 0;
        }
    }
    _lastX = mappedX;
    _lastY = mappedY;

    if (_stableSamples < 2) {
        return holdContact();
    }

    point.x = mappedX;
    point.y = mappedY;
    point.touched = true;
    _contactActive = true;
    _lastContactMs = millis();
    return true;
}

void JC3248W535Touch::setRotation(uint8_t rotation, uint16_t width, uint16_t height) {
    _rotation = rotation & 0x03;
    _width = width;
    _height = height;
}
#endif
