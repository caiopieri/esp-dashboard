#pragma once

#include "core/App.h"
#include <WiFi.h>

class ClockWeatherApp : public App {
public:
    const char* getId() override { return "clock_system"; }
    const char* getTitle() override { return "Clock & System"; }
    const char* getIcon() override { return "⏰"; }

    void onStart(lv_obj_t* root) override {
        const AppCardLayout layout = appCardLayout(root);
        _rootCard = lv_obj_create(root);
        lv_obj_set_size(_rootCard, layout.width, layout.height);
        lv_obj_center(_rootCard);
        lv_obj_set_style_bg_color(_rootCard, lv_color_hex(0x181825), 0);
        lv_obj_set_style_border_color(_rootCard, lv_color_hex(0x313244), 0);
        lv_obj_set_style_border_width(_rootCard, 1, 0);
        lv_obj_set_style_radius(_rootCard, 12, 0);
        lv_obj_set_style_pad_all(_rootCard, layout.padding, 0);
        lv_obj_clear_flag(_rootCard, LV_OBJ_FLAG_SCROLLABLE);

        // Big Time Display
        _timeDisplay = lv_label_create(_rootCard);
        lv_label_set_text(_timeDisplay, "09:41");
        lv_obj_set_style_text_font(_timeDisplay, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(_timeDisplay, lv_color_hex(0xCDD6F4), 0);
        lv_obj_align(_timeDisplay, LV_ALIGN_TOP_LEFT, 4, 0);

        // Date Display
        _dateDisplay = lv_label_create(_rootCard);
        lv_label_set_text(_dateDisplay, "Monday, 24 August");
        lv_obj_set_style_text_font(_dateDisplay, layout.smallFont(), 0);
        lv_obj_set_style_text_color(_dateDisplay, lv_color_hex(0xA6ADC8), 0);
        lv_obj_align(_dateDisplay, LV_ALIGN_TOP_LEFT, layout.scaled(6), layout.scaled(38));

        // System Specs Container
        lv_obj_t* statsBox = lv_obj_create(_rootCard);
        lv_obj_set_size(statsBox, layout.contentWidth, layout.scaled(100));
        lv_obj_align(statsBox, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(statsBox, lv_color_hex(0x1E1E2E), 0);
        lv_obj_set_style_border_color(statsBox, lv_color_hex(0x45475A), 0);
        lv_obj_set_style_border_width(statsBox, 1, 0);
        lv_obj_set_style_radius(statsBox, 8, 0);
        lv_obj_set_style_pad_all(statsBox, 8, 0);
        lv_obj_clear_flag(statsBox, LV_OBJ_FLAG_SCROLLABLE);

        _ipLabel = lv_label_create(statsBox);
        lv_label_set_text_fmt(_ipLabel, "IP: %s", WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "No Wi-Fi");
        lv_obj_set_style_text_font(_ipLabel, layout.smallFont(), 0);
        lv_obj_set_style_text_color(_ipLabel, lv_color_hex(0x89DCEB), 0);
        lv_obj_align(_ipLabel, LV_ALIGN_TOP_LEFT, 0, 0);

        _heapLabel = lv_label_create(statsBox);
        lv_label_set_text_fmt(_heapLabel, "Free RAM: %d KB", ESP.getFreeHeap() / 1024);
        lv_obj_set_style_text_font(_heapLabel, layout.smallFont(), 0);
        lv_obj_set_style_text_color(_heapLabel, lv_color_hex(0xA6E3A1), 0);
        lv_obj_align(_heapLabel, LV_ALIGN_TOP_LEFT, 0, 24);

        _uptimeLabel = lv_label_create(statsBox);
        lv_label_set_text_fmt(_uptimeLabel, "Uptime: %lu sec", millis() / 1000);
        lv_obj_set_style_text_font(_uptimeLabel, layout.smallFont(), 0);
        lv_obj_set_style_text_color(_uptimeLabel, lv_color_hex(0xF9E2AF), 0);
        lv_obj_align(_uptimeLabel, LV_ALIGN_TOP_LEFT, 0, 48);
    }

    void onUpdate() override {
        static unsigned long lastTick = 0;
        if (millis() - lastTick > 1000) {
            lastTick = millis();
            if (_uptimeLabel) {
                lv_label_set_text_fmt(_uptimeLabel, "Uptime: %lu sec", millis() / 1000);
            }
            if (_heapLabel) {
                lv_label_set_text_fmt(_heapLabel, "Free RAM: %d KB", ESP.getFreeHeap() / 1024);
            }
        }
    }

    void onStop() override {
        if (_rootCard != nullptr) {
            lv_obj_del(_rootCard);
            _rootCard = nullptr;
        }
    }

private:
    lv_obj_t* _rootCard = nullptr;
    lv_obj_t* _timeDisplay = nullptr;
    lv_obj_t* _dateDisplay = nullptr;
    lv_obj_t* _ipLabel = nullptr;
    lv_obj_t* _heapLabel = nullptr;
    lv_obj_t* _uptimeLabel = nullptr;
};
