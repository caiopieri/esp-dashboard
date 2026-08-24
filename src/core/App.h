#pragma once

#include <Arduino.h>
#include <lvgl.h>

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
