#include "DisplayDriver.h"
#include <esp_heap_caps.h>

#if defined(BOARD_JC3248W535EN)
JC3248W535Display DisplayDriver::gfx;
JC3248W535Touch DisplayDriver::touch;
#else
LGFX DisplayDriver::gfx;
SPIClass DisplayDriver::touchSPI(HSPI);
XPT2046_Touchscreen DisplayDriver::ts(CYD_TP_CS);
#endif

lv_disp_draw_buf_t DisplayDriver::draw_buf;
lv_color_t* DisplayDriver::buf_1 = nullptr;
lv_color_t* DisplayDriver::buf_2 = nullptr;

void DisplayDriver::flushCallback(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    #if defined(BOARD_JC3248W535EN)
    gfx.drawPixels(area->x1, area->y1, &color_p->full, w, h);
    #else
    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
    gfx.endWrite();
    #endif

    lv_disp_flush_ready(disp);
}

void DisplayDriver::touchCallback(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    (void)indev_driver;

    #if defined(BOARD_JC3248W535EN)
    JC3248TouchPoint point;
    if (touch.read(point)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = point.x;
        data->point.y = point.y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
    #else
    // Read the controller once per LVGL input sample. Calling touched() and
    // getPoint() separately can result in two SPI updates for one sample.
    TS_Point p = ts.getPoint();
    if (p.z >= 300) {

        // Exact formula verified in calibration
        int x = map(p.x, 3800, 240, 0, SCREEN_WIDTH);
        int y = map(p.y, 3800, 240, 0, SCREEN_HEIGHT);

        x = constrain(x, 0, SCREEN_WIDTH - 1);
        y = constrain(y, 0, SCREEN_HEIGHT - 1);

        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;

        return;
    }
    data->state = LV_INDEV_STATE_REL;
    #endif
}

void DisplayDriver::init() {
    // 1. Display Hardware
    #if defined(BOARD_JC3248W535EN)
    if (!gfx.begin()) {
        Serial.println("Display init failed: JC3248W535EN AXS15231B");
    }
    touch.setRotation(1, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!touch.begin()) {
        Serial.println("Touch init failed: JC3248W535EN AXS15231B");
    }
    #else
    gfx.init();
    gfx.setRotation(1); // Landscape: 320x240
    gfx.setBrightness(220);

    // 2. Touch SPI Hardware (EXACTLY like calibration)
    touchSPI.begin(CYD_TP_CLK, CYD_TP_MISO, CYD_TP_MOSI, CYD_TP_CS);
    ts.begin(touchSPI);
    // Note: Do NOT call ts.setRotation(), we map raw coordinates directly!
    #endif

    // 3. LVGL Buffers
    lv_init();
    #if defined(BOARD_JC3248W535EN)
    buf_1 = (lv_color_t*)heap_caps_malloc(BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    buf_2 = (lv_color_t*)heap_caps_malloc(BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    #else
    buf_1 = (lv_color_t*)heap_caps_malloc(BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    buf_2 = (lv_color_t*)heap_caps_malloc(BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    #endif
    lv_disp_draw_buf_init(&draw_buf, buf_1, buf_2, BUFFER_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = flushCallback;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchCallback;
    lv_indev_drv_register(&indev_drv);
}

void DisplayDriver::handle() {
    // LVGL 8 does not use Arduino's millis() automatically unless
    // LV_TICK_CUSTOM is enabled. The input and animation timers otherwise
    // remain at tick 0, so the indev read callback is never scheduled.
    static uint32_t last_tick = millis();
    uint32_t now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    lv_timer_handler();
}

void DisplayDriver::setBrightness(uint8_t brightness) {
    gfx.setBrightness(brightness);
}
