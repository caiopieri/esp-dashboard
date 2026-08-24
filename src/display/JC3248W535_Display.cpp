#if defined(BOARD_JC3248W535EN)
#include "JC3248W535_Display.h"

bool JC3248W535Display::begin() {
    pinMode(JC3248_LCD_BL, OUTPUT);
    setBrightness(0);

    _bus = new Arduino_ESP32QSPI(
        JC3248_LCD_CS,
        JC3248_LCD_SCLK,
        JC3248_LCD_SDIO0,
        JC3248_LCD_SDIO1,
        JC3248_LCD_SDIO2,
        JC3248_LCD_SDIO3);
    if (!_bus) return false;

    _display = new Arduino_AXS15231B(
        _bus,
        JC3248_LCD_RST,
        1,       // landscape: 480x320
        false,   // IPS
        320,
        480);
    if (!_display) return false;

    if (!_display->begin(32000000UL)) return false;
    _display->setRotation(1);
    setBrightness(220);
    return true;
}

void JC3248W535Display::drawPixels(
    int16_t x,
    int16_t y,
    const uint16_t* pixels,
    uint16_t width,
    uint16_t height) {
    if (_display) {
        _display->draw16bitRGBBitmap(x, y, pixels, width, height);
    }
}

void JC3248W535Display::setBrightness(uint8_t brightness) {
    analogWrite(JC3248_LCD_BL, brightness);
}
#endif
