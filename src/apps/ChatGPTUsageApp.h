#pragma once

#include "core/App.h"
#include "core/ProviderUsage.h"
#include "core/VariableStore.h"

class ChatGPTUsageApp : public App {
public:
    const char* getId() override { return "chatgpt_usage"; }
    const char* getTitle() override { return "ChatGPT · Codex"; }
    const char* getIcon() override { return "C"; }

    void onStart(lv_obj_t* root) override {
        _layout = appCardLayout(root);
        _rootCard = lv_obj_create(root);
        lv_obj_set_size(_rootCard, _layout.width, _layout.height);
        lv_obj_center(_rootCard);
        lv_obj_set_style_bg_color(_rootCard, lv_color_hex(0x181825), 0);
        lv_obj_set_style_border_color(_rootCard, lv_color_hex(0x313244), 0);
        lv_obj_set_style_border_width(_rootCard, 1, 0);
        lv_obj_set_style_radius(_rootCard, 12, 0);
        lv_obj_set_style_pad_all(_rootCard, _layout.padding, 0);
        lv_obj_clear_flag(_rootCard, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title = lv_label_create(_rootCard);
        lv_label_set_text(title, "ChatGPT");
        lv_obj_set_style_text_font(title, _layout.titleFont(), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0x74F0C1), 0);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 0);

        lv_obj_t* subtitle = lv_label_create(_rootCard);
        lv_label_set_text(subtitle, "Codex assistant");
        lv_obj_set_style_text_font(subtitle, _layout.smallFont(), 0);
        lv_obj_set_style_text_color(subtitle, lv_color_hex(0xA6ADC8), 0);
        lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 3, 22);

        createRobot();

        _syncLabel = lv_label_create(_rootCard);
        lv_obj_set_style_text_font(_syncLabel, _layout.smallFont(), 0);
        lv_obj_set_style_text_color(_syncLabel, lv_color_hex(0xA6E3A1), 0);
        lv_obj_align(_syncLabel, LV_ALIGN_TOP_LEFT, 2, 46);

        _progressBar = lv_bar_create(_rootCard);
        lv_obj_set_size(_progressBar, _layout.contentWidth, _layout.scaled(14));
        lv_obj_align(_progressBar, LV_ALIGN_TOP_MID, 0, _layout.scaled(64));
        lv_obj_set_style_bg_color(_progressBar, lv_color_hex(0x313244), 0);
        lv_obj_set_style_radius(_progressBar, 7, 0);
        lv_obj_set_style_bg_color(_progressBar, lv_color_hex(0x74F0C1), LV_PART_INDICATOR);
        lv_obj_set_style_radius(_progressBar, 7, LV_PART_INDICATOR);
        lv_bar_set_range(_progressBar, 0, 100);

        createMetricCard(0, "Tokens hoje", &_tokensLabel, lv_color_hex(0x74F0C1));
        createMetricCard(1, "Requisições", &_requestsLabel, lv_color_hex(0x89DCEB));
        refreshUsage();

        lv_anim_init(&_robotAnim);
        lv_anim_set_var(&_robotAnim, _robot);
        lv_anim_set_values(&_robotAnim, -2, 2);
        lv_anim_set_time(&_robotAnim, 900);
        lv_anim_set_playback_time(&_robotAnim, 900);
        lv_anim_set_repeat_count(&_robotAnim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&_robotAnim, [](void* var, int32_t value) {
            lv_obj_set_y(static_cast<lv_obj_t*>(var), value);
        });
#if !defined(BOARD_JC3248W535EN)
        lv_anim_start(&_robotAnim);
#endif
    }

    void onUpdate() override {
        if (millis() - _lastRefresh >= 5000) refreshUsage();
    }

    void onStop() override {
        if (_robot) lv_anim_del(_robot, nullptr);
        if (_rootCard) {
            lv_obj_del(_rootCard);
            _rootCard = nullptr;
        }
    }

private:
    static String readVariable(const char* name, const char* fallback) {
        String value;
        if (VariableStore::getInstance().getValue(name, value) && value.length() > 0) {
            return value;
        }
        return String(fallback);
    }

    void createRobot() {
        _robot = lv_obj_create(_rootCard);
        lv_obj_set_size(_robot, _layout.scaled(42), _layout.scaled(38));
        lv_obj_align(_robot, LV_ALIGN_TOP_RIGHT, -_layout.scaled(2), -_layout.scaled(1));
        lv_obj_set_style_bg_opa(_robot, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(_robot, 0, 0);
        lv_obj_set_style_pad_all(_robot, 0, 0);
        lv_obj_clear_flag(_robot, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* antenna = lv_obj_create(_robot);
        lv_obj_set_size(antenna, _layout.scaled(2), _layout.scaled(6));
        lv_obj_align(antenna, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(antenna, lv_color_hex(0x74F0C1), 0);
        lv_obj_set_style_border_width(antenna, 0, 0);

        lv_obj_t* head = lv_obj_create(_robot);
        lv_obj_set_size(head, _layout.scaled(32), _layout.scaled(24));
        lv_obj_align(head, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(head, lv_color_hex(0x24243A), 0);
        lv_obj_set_style_border_color(head, lv_color_hex(0x74F0C1), 0);
        lv_obj_set_style_border_width(head, 2, 0);
        lv_obj_set_style_radius(head, 8, 0);
        lv_obj_set_style_pad_all(head, 0, 0);

        for (int x : {-7, 5}) {
            lv_obj_t* eye = lv_obj_create(head);
            lv_obj_set_size(eye, _layout.scaled(5), _layout.scaled(5));
            lv_obj_align(eye, LV_ALIGN_CENTER, x, -1);
            lv_obj_set_style_bg_color(eye, lv_color_hex(0x74F0C1), 0);
            lv_obj_set_style_border_width(eye, 0, 0);
            lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        }
    }

    void createMetricCard(int index, const char* title, lv_obj_t** valueLabel, lv_color_t color) {
        lv_obj_t* card = lv_obj_create(_rootCard);
        lv_obj_set_size(card, _layout.scaled(138), _layout.scaled(70));
        lv_obj_align(card, index == 0 ? LV_ALIGN_BOTTOM_LEFT : LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x1E1E2E), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x45475A), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 6, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* titleLabel = lv_label_create(card);
        lv_label_set_text(titleLabel, title);
        lv_obj_set_style_text_font(titleLabel, _layout.smallFont(), 0);
        lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xA6ADC8), 0);
        lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 0, 0);

        *valueLabel = lv_label_create(card);
        lv_label_set_text(*valueLabel, "--");
        lv_obj_set_style_text_font(*valueLabel, _layout.valueFont(), 0);
        lv_obj_set_style_text_color(*valueLabel, color, 0);
        lv_obj_align(*valueLabel, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    }

    void refreshUsage() {
        _lastRefresh = millis();
        ProviderUsage usage;
        const bool hasRuntime = ProviderUsageStore::getInstance().get("chatgpt", usage);
        const int percent = hasRuntime ? usage.weeklyPercent :
            constrain(readVariable("CHATGPT_USAGE_PERCENT", "0").toInt(), 0, 100);
        const int weekly = hasRuntime ? usage.weeklyPercent :
            constrain(readVariable("CHATGPT_WEEKLY_PERCENT", "0").toInt(), 0, 100);
#if defined(BOARD_JC3248W535EN)
        lv_bar_set_value(_progressBar, max(0, percent), LV_ANIM_OFF);
#else
        lv_bar_set_value(_progressBar, percent, LV_ANIM_ON);
#endif
        const String quota = "7d " + providerUsagePercent(weekly);
        lv_label_set_text(_syncLabel, quota.c_str());
        const String tokens = hasRuntime && usage.tokens.length() > 0 ? usage.tokens : readVariable("CHATGPT_TOKENS", "--");
        const String requests = hasRuntime && usage.requests.length() > 0 ? usage.requests : readVariable("CHATGPT_REQUESTS", "--");
        lv_label_set_text(_tokensLabel, tokens.c_str());
        lv_label_set_text(_requestsLabel, requests.c_str());
    }

    lv_obj_t* _rootCard = nullptr;
    lv_obj_t* _robot = nullptr;
    lv_obj_t* _syncLabel = nullptr;
    lv_obj_t* _progressBar = nullptr;
    lv_obj_t* _tokensLabel = nullptr;
    lv_obj_t* _requestsLabel = nullptr;
    lv_anim_t _robotAnim;
    AppCardLayout _layout;
    unsigned long _lastRefresh = 0;
};
