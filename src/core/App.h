#pragma once

#include <Arduino.h>
#include <lvgl.h>

struct AppCardLayout {
    lv_coord_t width = 310;
    lv_coord_t height = 192;
    lv_coord_t padding = 10;
    lv_coord_t contentWidth = 290;
    float scale = 1.0f;

    lv_coord_t scaled(lv_coord_t value) const {
        return static_cast<lv_coord_t>(value * scale + 0.5f);
    }

    const lv_font_t* smallFont() const {
        return scale >= 1.2f ? &lv_font_montserrat_16 : &lv_font_montserrat_12;
    }

    const lv_font_t* bodyFont() const {
        return scale >= 1.2f ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    }

    const lv_font_t* valueFont() const {
        return scale >= 1.2f ? &lv_font_montserrat_20 : &lv_font_montserrat_16;
    }

    const lv_font_t* titleFont() const {
        return scale >= 1.2f ? &lv_font_montserrat_20 : &lv_font_montserrat_16;
    }

    const lv_font_t* iconFont() const {
        return scale >= 1.2f ? &lv_font_montserrat_28 : &lv_font_montserrat_20;
    }
};

inline AppCardLayout appCardLayout(lv_obj_t* root) {
    AppCardLayout layout;
    const lv_coord_t availableWidth = lv_obj_get_width(root);
    const lv_coord_t availableHeight = lv_obj_get_height(root);
    const lv_coord_t usableWidth = availableWidth > 16 ? availableWidth - 16 : availableWidth;
    const lv_coord_t usableHeight = availableHeight > 16 ? availableHeight - 16 : availableHeight;
    const float widthScale = usableWidth / 310.0f;
    const float heightScale = usableHeight / 192.0f;

    layout.width = usableWidth;
    layout.height = usableHeight;
    layout.scale = widthScale < heightScale ? widthScale : heightScale;
    layout.padding = layout.scaled(10);
    layout.contentWidth = layout.width - (layout.padding * 2);
    return layout;
}

class App {
public:
    virtual ~App() = default;

    virtual const char* getId() = 0;
    virtual const char* getTitle() = 0;
    virtual const char* getIcon() = 0;

    /**
     * @brief Called when the user swipes into this app's screen.
     * Use this to create LVGL widgets inside the provided root container.
     */
    virtual void onStart(lv_obj_t* root) = 0;

    /**
     * @brief Called periodically ONLY while this app is visible on screen.
     */
    virtual void onUpdate() = 0;

    /**
     * @brief Called when the user swipes away from this app.
     * Clean up widgets, free RAM, stop animations/timers.
     */
    virtual void onStop() = 0;

    bool isActive() const { return _active; }
    void setActive(bool active) { _active = active; }

private:
    bool _active = false;
};
