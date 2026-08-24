#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>

// Guition/Jingcai JC3248W535EN: 3.5-inch 320x480 panel, used in landscape.
// The display controller is AXS15231B over QSPI.
constexpr int8_t JC3248_LCD_CS = 45;
constexpr int8_t JC3248_LCD_SCLK = 47;
constexpr int8_t JC3248_LCD_SDIO0 = 21;
constexpr int8_t JC3248_LCD_SDIO1 = 48;
constexpr int8_t JC3248_LCD_SDIO2 = 40;
constexpr int8_t JC3248_LCD_SDIO3 = 39;
constexpr int8_t JC3248_LCD_RST = GFX_NOT_DEFINED;
constexpr int8_t JC3248_LCD_BL = 1;

// LVGL uses landscape coordinates, while the AXS15231B RAM is portrait.
constexpr uint16_t JC3248_LCD_WIDTH = 480;
constexpr uint16_t JC3248_LCD_HEIGHT = 320;
constexpr uint16_t JC3248_PANEL_WIDTH = 320;
constexpr uint16_t JC3248_PANEL_HEIGHT = 480;

class JC3248W535Display {
public:
    bool begin();
    void drawPixels(int16_t x, int16_t y, const uint16_t* pixels, uint16_t width, uint16_t height);
    void present();
    void setBrightness(uint8_t brightness);

private:
    Arduino_DataBus* _bus = nullptr;
    Arduino_AXS15231B* _panel = nullptr;
    uint16_t* _framebuffer = nullptr;
};
