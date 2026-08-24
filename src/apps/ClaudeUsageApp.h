#pragma once

#include "core/App.h"
#include "core/ProviderUsage.h"
#include "core/VariableStore.h"

class ClaudeUsageApp : public App {
public:
    const char* getId() override { return "claude_usage"; }
    const char* getTitle() override { return "Claude Usage"; }
    const char* getIcon() override { return "A"; }

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
        lv_label_set_text(title, "Claude");
        lv_obj_set_style_text_font(title, _layout.titleFont(), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xF2CDCD), 0);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 0);

        lv_obj_t* subtitle = lv_label_create(_rootCard);
        lv_label_set_text(subtitle, "Anthropic assistant");
        lv_obj_set_style_text_font(subtitle, _layout.smallFont(), 0);
        lv_obj_set_style_text_color(subtitle, lv_color_hex(0xA6ADC8), 0);
        lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 3, 22);

        _syncLabel = lv_label_create(_rootCard);
        lv_obj_set_style_text_font(_syncLabel, _layout.smallFont(), 0);
        lv_obj_set_style_text_color(_syncLabel, lv_color_hex(0xF2CDCD), 0);
        lv_obj_align(_syncLabel, LV_ALIGN_TOP_LEFT, 2, 46);

        _progressBar = lv_bar_create(_rootCard);
        lv_obj_set_size(_progressBar, _layout.contentWidth, _layout.scaled(14));
        lv_obj_align(_progressBar, LV_ALIGN_TOP_MID, 0, _layout.scaled(64));
        lv_obj_set_style_bg_color(_progressBar, lv_color_hex(0x313244), 0);
        lv_obj_set_style_bg_color(_progressBar, lv_color_hex(0xF2CDCD), LV_PART_INDICATOR);
        lv_obj_set_style_radius(_progressBar, 7, 0);
        lv_obj_set_style_radius(_progressBar, 7, LV_PART_INDICATOR);
        lv_bar_set_range(_progressBar, 0, 100);

        createMetricCard(0, "Tokens hoje", &_tokensLabel);
        createMetricCard(1, "Requisições", &_requestsLabel);
        refreshUsage();
    }

    void onUpdate() override {
        if (millis() - _lastRefresh >= 5000) refreshUsage();
    }

    void onStop() override {
        if (_rootCard) {
            lv_obj_del(_rootCard);
            _rootCard = nullptr;
        }
    }

private:
    static String readVariable(const char* name, const char* fallback) {
        String value;
        if (VariableStore::getInstance().getValue(name, value) && value.length() > 0) return value;
        return String(fallback);
    }

    void createMetricCard(int index, const char* title, lv_obj_t** valueLabel) {
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
        lv_obj_set_style_text_color(*valueLabel, lv_color_hex(0xF2CDCD), 0);
        lv_obj_align(*valueLabel, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    }

    void refreshUsage() {
        _lastRefresh = millis();
        ProviderUsage usage;
        const bool hasRuntime = ProviderUsageStore::getInstance().get("claude", usage);
        const int percent = hasRuntime ? usage.sessionPercent :
            constrain(readVariable("CLAUDE_USAGE_PERCENT", "0").toInt(), 0, 100);
        const int weekly = hasRuntime ? usage.weeklyPercent :
            constrain(readVariable("CLAUDE_WEEKLY_PERCENT", "0").toInt(), 0, 100);
        lv_bar_set_value(_progressBar, percent, LV_ANIM_OFF);
        lv_label_set_text_fmt(_syncLabel, "Sessao %d%% | 7d %d%%", percent, weekly);
        const String tokens = hasRuntime && usage.tokens.length() > 0 ? usage.tokens : readVariable("CLAUDE_TOKENS", "--");
        const String requests = hasRuntime && usage.requests.length() > 0 ? usage.requests : readVariable("CLAUDE_REQUESTS", "--");
        lv_label_set_text(_tokensLabel, tokens.c_str());
        lv_label_set_text(_requestsLabel, requests.c_str());
    }

    lv_obj_t* _rootCard = nullptr;
    lv_obj_t* _syncLabel = nullptr;
    lv_obj_t* _progressBar = nullptr;
    lv_obj_t* _tokensLabel = nullptr;
    lv_obj_t* _requestsLabel = nullptr;
    AppCardLayout _layout;
    unsigned long _lastRefresh = 0;
};
