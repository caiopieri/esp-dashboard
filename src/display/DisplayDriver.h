#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include "LGFX_CYD.h"

// Confirmed CYD 2.8" Touch Pins
#define CYD_TP_CLK  25
#define CYD_TP_MISO 39
#define CYD_TP_MOSI 32
#define CYD_TP_CS   33

class DisplayDriver {
public:
    static LGFX gfx;
    static SPIClass touchSPI;
    static XPT2046_Touchscreen ts;

    static void init();
    static void handle();
    static void setBrightness(uint8_t brightness);

private:
    static const uint16_t SCREEN_WIDTH = 320;
    static const uint16_t SCREEN_HEIGHT = 240;
    static const uint32_t BUFFER_SIZE = SCREEN_WIDTH * 25;

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t* buf_1;
    static lv_color_t* buf_2;

    static void flushCallback(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
    static void touchCallback(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);
};
