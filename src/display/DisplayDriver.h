#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>

#if defined(BOARD_JC3248W535EN)
#include "JC3248W535_Display.h"
#include "JC3248W535_Touch.h"
#else
#include <XPT2046_Touchscreen.h>
#include "LGFX_CYD.h"
#endif

// Confirmed CYD 2.8" Touch Pins
#if defined(BOARD_JC3248W535EN)
constexpr uint16_t DISPLAY_WIDTH = JC3248_LCD_WIDTH;
constexpr uint16_t DISPLAY_HEIGHT = JC3248_LCD_HEIGHT;
#else
constexpr uint16_t DISPLAY_WIDTH = 320;
constexpr uint16_t DISPLAY_HEIGHT = 240;
#define CYD_TP_CLK  25
#define CYD_TP_MISO 39
#define CYD_TP_MOSI 32
#define CYD_TP_CS   33
#endif

class DisplayDriver {
public:
    #if defined(BOARD_JC3248W535EN)
    static JC3248W535Display gfx;
    static JC3248W535Touch touch;
    #else
    static LGFX gfx;
    static SPIClass touchSPI;
    static XPT2046_Touchscreen ts;
    #endif

    static void init();
    static void handle();
    static void setBrightness(uint8_t brightness);

private:
    static const uint16_t SCREEN_WIDTH = DISPLAY_WIDTH;
    static const uint16_t SCREEN_HEIGHT = DISPLAY_HEIGHT;
    static const uint32_t BUFFER_SIZE = SCREEN_WIDTH * 25;

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t* buf_1;
    static lv_color_t* buf_2;

    static void flushCallback(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void touchCallback(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
};
