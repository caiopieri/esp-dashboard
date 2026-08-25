#pragma once

#include "App.h"
#include "DataStore.h"
#include "VariableStore.h"
#include <ArduinoJson.h>
#include <stdlib.h>
#include <time.h>

// Small, bounded renderer for cards created by the web panel. It deliberately
// supports data, not code: a card definition cannot execute on the ESP.
class DeclarativeCardApp : public App {
public:
    explicit DeclarativeCardApp(const String& definition) { load(definition); }

    const char* getId() override { return _id.c_str(); }
    const char* getTitle() override { return _title.c_str(); }
    const char* getIcon() override { return "D"; }

    void onStart(lv_obj_t* root) override {
        _layout = appCardLayout(root);
        _root = lv_obj_create(root);
        lv_obj_set_size(_root, _layout.width, _layout.height);
        lv_obj_center(_root);
        lv_obj_set_style_bg_color(_root, lv_color_hex(_accent), 0);
        lv_obj_set_style_border_color(_root, lv_color_hex(0x45475A), 0);
        lv_obj_set_style_border_width(_root, 1, 0);
        lv_obj_set_style_radius(_root, 12, 0);
        lv_obj_set_style_pad_all(_root, _layout.padding, 0);
        lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title = lv_label_create(_root);
        lv_label_set_text(title, _title.c_str());
        lv_obj_set_style_text_font(title, _layout.titleFont(), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xCDD6F4), 0);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 0);

        _value = lv_label_create(_root);
        lv_obj_set_style_text_font(_value, _type == "text" ? _layout.bodyFont() : _layout.valueFont(), 0);
        lv_obj_set_style_text_color(_value, lv_color_hex(0xA6E3A1), 0);
        lv_obj_align(_value, LV_ALIGN_CENTER, 0, 2);

        _detail = lv_label_create(_root);
        lv_obj_set_style_text_font(_detail, _layout.smallFont(), 0);
        lv_obj_set_style_text_color(_detail, lv_color_hex(0xA6ADC8), 0);
        lv_obj_align(_detail, LV_ALIGN_BOTTOM_LEFT, 2, -2);

        if (_type == "progress") {
            _bar = lv_bar_create(_root);
            lv_obj_set_size(_bar, _layout.contentWidth, _layout.scaled(14));
            lv_obj_align(_bar, LV_ALIGN_CENTER, 0, 14);
            lv_bar_set_range(_bar, 0, _maxValue);
            lv_obj_set_style_bg_color(_bar, lv_color_hex(0x313244), 0);
            lv_obj_set_style_bg_color(_bar, lv_color_hex(_accent), LV_PART_INDICATOR);
            lv_obj_set_style_radius(_bar, 7, 0);
            lv_obj_set_style_radius(_bar, 7, LV_PART_INDICATOR);
        } else if (_type == "chart") {
            _chart = lv_chart_create(_root);
            lv_obj_set_size(_chart, _layout.contentWidth, _layout.scaled(74));
            lv_obj_align(_chart, LV_ALIGN_CENTER, 0, 8);
            lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
            lv_chart_set_point_count(_chart, 24);
            lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, 0, _maxValue);
            _series = lv_chart_add_series(_chart, lv_color_hex(_accent), LV_CHART_AXIS_PRIMARY_Y);
        }
        refresh();
    }

    void onUpdate() override {
        if (millis() - _lastRefresh >= 2000) refresh();
    }

    void onStop() override {
        if (_root) lv_obj_del(_root);
        _root = nullptr;
        _value = nullptr;
        _detail = nullptr;
        _bar = nullptr;
        _chart = nullptr;
        _series = nullptr;
    }

private:
    void load(const String& definition) {
        JsonDocument doc;
        if (deserializeJson(doc, definition) != DeserializationError::Ok) return;
        _id = doc["id"] | "custom_card";
        _title = doc["title"] | _id.c_str();
        _type = doc["type"] | "metric";
        _accent = strtol((doc["theme"]["accent"] | "#74F0C1") + 1, nullptr, 16);
        JsonObject data = doc["data"].as<JsonObject>();
        _source = data["source"] | "static";
        _namespaceName = data["namespace"] | _id.c_str();
        _key = data["key"] | "value";
        _staticValue = data["value"] | "--";
        JsonObject body = doc["body"].as<JsonObject>();
        _label = body["label"] | "";
        _unit = body["unit"] | "";
        _maxValue = max(1, body["max"] | 100);
        for (const char* item : body["items"].as<JsonArray>()) {
            if (_items.length() > 0) _items += "\n";
            _items += item;
        }
    }

    String readValue() const {
        String value;
        if (_source == "runtime") {
            if (!DataStore::getInstance().get(_namespaceName, _key, value)) value = "--";
        } else if (_source == "variable") {
            if (!VariableStore::getInstance().getValue(_key.c_str(), value)) value = "--";
        } else {
            value = _staticValue;
        }
        return value;
    }

    void refresh() {
        _lastRefresh = millis();
        String value = _type == "list" && _items.length() > 0 ? _items : readValue();
        if (_type == "clock") {
            time_t now = time(nullptr);
            struct tm local;
            localtime_r(&now, &local);
            char buffer[24];
            strftime(buffer, sizeof(buffer), "%H:%M:%S", &local);
            value = buffer;
        }
        if (_type == "chart" && _chart && _series) {
            uint16_t point = 0;
            int start = 0;
            while (point < 24 && start <= value.length()) {
                int end = value.indexOf(',', start);
                if (end < 0) end = value.length();
                String sample = value.substring(start, end);
                lv_chart_set_value_by_id(_chart, _series, point++, constrain(sample.toInt(), 0, _maxValue));
                start = end + 1;
            }
            while (point < 24) lv_chart_set_value_by_id(_chart, _series, point++, 0);
            lv_chart_refresh(_chart);
        }
        if (_bar && _type == "progress") {
            lv_bar_set_value(_bar, constrain(value.toInt(), 0, _maxValue), LV_ANIM_OFF);
        }
        String shown = value;
        if (_type == "metric" && _unit.length() > 0) shown += " " + _unit;
        lv_label_set_text(_value, shown.c_str());
        String detail = _label;
        if (_type == "status") detail = value;
        lv_label_set_text(_detail, detail.c_str());
    }

    String _id, _title, _type, _source, _namespaceName, _key;
    String _staticValue, _label, _unit, _items;
    uint32_t _accent = 0x24243A;
    int _maxValue = 100;
    AppCardLayout _layout;
    lv_obj_t* _root = nullptr;
    lv_obj_t* _value = nullptr;
    lv_obj_t* _detail = nullptr;
    lv_obj_t* _bar = nullptr;
    lv_obj_t* _chart = nullptr;
    lv_chart_series_t* _series = nullptr;
    unsigned long _lastRefresh = 0;
};
