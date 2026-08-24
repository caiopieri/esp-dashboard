#if defined(BOARD_JC3248W535EN)
#include "JC3248W535_Display.h"
#include <esp_heap_caps.h>

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

    _panel = new Arduino_AXS15231B(
        _bus,
        JC3248_LCD_RST,
        0,       // native portrait: 320x480
        false,   // IPS
        320,
        480);
    if (!_panel) return false;

    // The JC3248W535 examples use 40 MHz QSPI. The previous 32 MHz setting
    // made every full-frame carousel refresh unnecessarily expensive.
    if (!_panel->begin(40000000UL)) return false;

    // Keep a complete native portrait frame. This is slower than a partial
    // transfer, but is stable with the AXS15231B controller used by this board.
    _framebuffer = static_cast<uint16_t*>(heap_caps_malloc(
        static_cast<size_t>(JC3248_PANEL_WIDTH) * JC3248_PANEL_HEIGHT * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!_framebuffer) return false;
    memset(_framebuffer, 0, static_cast<size_t>(JC3248_PANEL_WIDTH) * JC3248_PANEL_HEIGHT * sizeof(uint16_t));

    _panel->fillScreen(BLACK);
    setBrightness(220);
    return true;
}

void JC3248W535Display::drawPixels(
    int16_t x,
    int16_t y,
    const uint16_t* pixels,
    uint16_t width,
    uint16_t height) {
    if (!_panel || !_framebuffer || !pixels || width == 0 || height == 0) return;

    // LVGL uses landscape coordinates. Update the corresponding pixels in
    // the native portrait framebuffer: (x, y) -> (y, 479 - x).
    for (uint16_t row = 0; row < height; ++row) {
        for (uint16_t column = 0; column < width; ++column) {
            const int16_t landscapeX = x + column;
            const int16_t landscapeY = y + row;
            const size_t destination =
                static_cast<size_t>(JC3248_LCD_WIDTH - 1 - landscapeX) * JC3248_PANEL_WIDTH + landscapeY;
            _framebuffer[destination] = pixels[static_cast<size_t>(row) * width + column];
        }
    }
}

void JC3248W535Display::present() {
    if (!_panel || !_framebuffer) return;

    _panel->startWrite();
    _panel->setAddrWindow(0, 0, JC3248_PANEL_WIDTH, JC3248_PANEL_HEIGHT);
    _panel->writePixels(_framebuffer, static_cast<uint32_t>(JC3248_PANEL_WIDTH) * JC3248_PANEL_HEIGHT);
    _panel->endWrite();
}

void JC3248W535Display::setBrightness(uint8_t brightness) {
    analogWrite(JC3248_LCD_BL, brightness);
}
#endif
