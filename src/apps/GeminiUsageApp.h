#pragma once

#include "core/App.h"
#include "core/ProviderUsage.h"
#include "core/VariableStore.h"

class GeminiUsageApp : public App {
public:
    const char* getId() override { return "gemini_usage"; }
    const char* getTitle() override { return "Gemini AI Usage"; }
    const char* getIcon() override { return "🤖"; }

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

        // 1. Header: Icon + Title
        lv_obj_t* header = lv_obj_create(_rootCard);
        lv_obj_set_size(header, layout.contentWidth, layout.scaled(36));
        lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        _robotLabel = lv_label_create(header);
        lv_label_set_text(_robotLabel, "🤖");
        lv_obj_set_style_text_font(_robotLabel, layout.iconFont(), 0);
        lv_obj_align(_robotLabel, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "Gemini Pro 2.5 API");
        lv_obj_set_style_text_font(title, layout.titleFont(), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0x89B4FA), 0); // Pastel Blue
        lv_obj_align(title, LV_ALIGN_LEFT_MID, 28, 0);

        // Status Badge (Active / Optimal)
        lv_obj_t* badge = lv_obj_create(header);
        lv_obj_set_size(badge, layout.scaled(62), layout.scaled(20));
        lv_obj_align(badge, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x2E3B33), 0);
        lv_obj_set_style_border_color(badge, lv_color_hex(0xA6E3A1), 0);
        lv_obj_set_style_border_width(badge, 1, 0);
        lv_obj_set_style_radius(badge, 10, 0);
        lv_obj_set_style_pad_all(badge, 0, 0);
        lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* badgeText = lv_label_create(badge);
        lv_label_set_text(badgeText, "• Online");
        lv_obj_set_style_text_font(badgeText, layout.smallFont(), 0);
        lv_obj_set_style_text_color(badgeText, lv_color_hex(0xA6E3A1), 0);
        lv_obj_center(badgeText);

        // 2. Robot Mascot Floating Animation
        lv_anim_init(&_robotAnim);
        lv_anim_set_var(&_robotAnim, _robotLabel);
        lv_anim_set_values(&_robotAnim, -2, 2);
        lv_anim_set_time(&_robotAnim, 800);
        lv_anim_set_playback_time(&_robotAnim, 800);
        lv_anim_set_repeat_count(&_robotAnim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&_robotAnim, [](void* var, int32_t val) {
            lv_obj_set_y((lv_obj_t*)var, val);
        });
#if !defined(BOARD_JC3248W535EN)
        lv_anim_start(&_robotAnim);
#endif

        // 3. Weekly Quota Progress Bar
        lv_obj_t* barLabel = lv_label_create(_rootCard);
        _quotaLabel = barLabel;
        lv_obj_set_style_text_font(barLabel, layout.smallFont(), 0);
        lv_obj_set_style_text_color(barLabel, lv_color_hex(0xCDD6F4), 0);
        lv_obj_align(barLabel, LV_ALIGN_TOP_LEFT, layout.scaled(2), layout.scaled(44));

        _progressBar = lv_bar_create(_rootCard);
        lv_obj_set_size(_progressBar, layout.contentWidth, layout.scaled(14));
        lv_obj_align(_progressBar, LV_ALIGN_TOP_MID, 0, layout.scaled(62));
        lv_obj_set_style_bg_color(_progressBar, lv_color_hex(0x313244), 0);
        lv_obj_set_style_radius(_progressBar, 7, 0);
        lv_obj_set_style_bg_color(_progressBar, lv_color_hex(0xF9E2AF), LV_PART_INDICATOR); // Soft yellow/orange
        lv_obj_set_style_radius(_progressBar, 7, LV_PART_INDICATOR);
        lv_bar_set_range(_progressBar, 0, 100);
        lv_bar_set_value(_progressBar, 0, LV_ANIM_OFF);

        // 4. Metrics Grid (2 Cards: Tokens & Requests)
        lv_obj_t* cardTokens = lv_obj_create(_rootCard);
        lv_obj_set_size(cardTokens, layout.scaled(138), layout.scaled(70));
        lv_obj_align(cardTokens, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_bg_color(cardTokens, lv_color_hex(0x1E1E2E), 0);
        lv_obj_set_style_border_color(cardTokens, lv_color_hex(0x45475A), 0);
        lv_obj_set_style_border_width(cardTokens, 1, 0);
        lv_obj_set_style_radius(cardTokens, 8, 0);
        lv_obj_set_style_pad_all(cardTokens, 6, 0);
        lv_obj_clear_flag(cardTokens, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lblTokensTitle = lv_label_create(cardTokens);
        lv_label_set_text(lblTokensTitle, "Tokens Used");
        lv_obj_set_style_text_font(lblTokensTitle, layout.smallFont(), 0);
        lv_obj_set_style_text_color(lblTokensTitle, lv_color_hex(0xA6ADC8), 0);
        lv_obj_align(lblTokensTitle, LV_ALIGN_TOP_LEFT, 0, 0);

        _tokenValLabel = lv_label_create(cardTokens);
        lv_label_set_text(_tokenValLabel, "--");
        lv_obj_set_style_text_font(_tokenValLabel, layout.valueFont(), 0);
        lv_obj_set_style_text_color(_tokenValLabel, lv_color_hex(0xCBA6F7), 0); // Mauve / Purple
        lv_obj_align(_tokenValLabel, LV_ALIGN_BOTTOM_LEFT, 0, -2);

        lv_obj_t* cardReqs = lv_obj_create(_rootCard);
        lv_obj_set_size(cardReqs, layout.scaled(138), layout.scaled(70));
        lv_obj_align(cardReqs, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(cardReqs, lv_color_hex(0x1E1E2E), 0);
        lv_obj_set_style_border_color(cardReqs, lv_color_hex(0x45475A), 0);
        lv_obj_set_style_border_width(cardReqs, 1, 0);
        lv_obj_set_style_radius(cardReqs, 8, 0);
        lv_obj_set_style_pad_all(cardReqs, 6, 0);
        lv_obj_clear_flag(cardReqs, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lblReqsTitle = lv_label_create(cardReqs);
        lv_label_set_text(lblReqsTitle, "API Requests");
        lv_obj_set_style_text_font(lblReqsTitle, layout.smallFont(), 0);
        lv_obj_set_style_text_color(lblReqsTitle, lv_color_hex(0xA6ADC8), 0);
        lv_obj_align(lblReqsTitle, LV_ALIGN_TOP_LEFT, 0, 0);

        _reqsValLabel = lv_label_create(cardReqs);
        lv_label_set_text(_reqsValLabel, "--");
        lv_obj_set_style_text_font(_reqsValLabel, layout.valueFont(), 0);
        lv_obj_set_style_text_color(_reqsValLabel, lv_color_hex(0x89DCEB), 0); // Sky cyan
        lv_obj_align(_reqsValLabel, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    }

    void onUpdate() override {
        if (millis() - _lastRefresh >= 5000) refreshUsage();
    }

    void onStop() override {
        // Stop animation and clean up objects to free memory
        lv_anim_del(_robotLabel, nullptr);
        if (_rootCard != nullptr) {
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

    void refreshUsage() {
        _lastRefresh = millis();
        ProviderUsage usage;
        const bool hasRuntime = ProviderUsageStore::getInstance().get("gemini", usage);
        const int percent = hasRuntime ? usage.sessionPercent :
            constrain(readVariable("GEMINI_USAGE_PERCENT", "0").toInt(), 0, 100);
        const int weekly = hasRuntime ? usage.weeklyPercent :
            constrain(readVariable("GEMINI_WEEKLY_PERCENT", "0").toInt(), 0, 100);
        lv_bar_set_value(_progressBar, percent, LV_ANIM_OFF);
        lv_label_set_text_fmt(_quotaLabel, "Sessao %d%% | 7d %d%%", percent, weekly);
        const String tokens = hasRuntime && usage.tokens.length() > 0 ? usage.tokens : readVariable("GEMINI_TOKENS", "--");
        const String requests = hasRuntime && usage.requests.length() > 0 ? usage.requests : readVariable("GEMINI_REQUESTS", "--");
        lv_label_set_text(_tokenValLabel, tokens.c_str());
        lv_label_set_text(_reqsValLabel, requests.c_str());
    }

    lv_obj_t* _rootCard = nullptr;
    lv_obj_t* _robotLabel = nullptr;
    lv_obj_t* _progressBar = nullptr;
    lv_obj_t* _quotaLabel = nullptr;
    lv_obj_t* _tokenValLabel = nullptr;
    lv_obj_t* _reqsValLabel = nullptr;
    lv_anim_t _robotAnim;
    unsigned long _lastRefresh = 0;
};
