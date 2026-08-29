#include <ArduinoJson.h>
#include <SDL.h>
#include <lvgl.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

namespace {

constexpr int SCREEN_WIDTH = 320;
constexpr int SCREEN_HEIGHT = 240;
constexpr int STATUS_HEIGHT = 25;
constexpr int CARD_MARGIN = 9;

struct Card {
    std::string id;
    std::string title;
    std::string type = "metric";
    std::string value = "--";
    std::string label;
    std::string unit;
    std::vector<std::string> items;
    std::string accent = "#74f0c1";
    std::string actionId;
    std::string actionLabel;
    int maxValue = 100;
    int order = 0;
    bool enabled = true;
    bool deleted = false;
};

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
std::vector<uint32_t> frameBuffer(SCREEN_WIDTH * SCREEN_HEIGHT, 0xff11111b);
std::vector<Card> cards;
lv_obj_t* tileView = nullptr;
size_t currentIndex = 0;
int mouseX = 0;
int mouseY = 0;
bool mousePressed = false;

uint32_t parseColor(const std::string& value, uint32_t fallback = 0x74f0c1) {
    try {
        const size_t offset = value.rfind('#', 0) == 0 ? 1 : 0;
        if (value.size() - offset != 6) return fallback;
        return static_cast<uint32_t>(std::stoul(value.substr(offset), nullptr, 16));
    } catch (...) {
        return fallback;
    }
}

lv_color_t lvColor(const std::string& value, uint32_t fallback = 0x74f0c1) {
    return lv_color_hex(parseColor(value, fallback));
}

int parseInteger(const std::string& value, int fallback = 0) {
    try {
        size_t consumed = 0;
        const int parsed = std::stoi(value, &consumed);
        return consumed == 0 ? fallback : parsed;
    } catch (...) {
        return fallback;
    }
}

bool loadConfig(const std::string& path) {
    std::ifstream input(path);
    if (!input) return false;
    const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    JsonDocument document;
    if (deserializeJson(document, json) != DeserializationError::Ok) return false;

    cards.clear();
    auto appendCard = [](JsonObject jsonCard) {
        if (!(jsonCard["enabled"] | false) || (jsonCard["deleted"] | false)) return;
        if (!jsonCard["type"].is<const char*>()) return;

        Card card;
        card.id = jsonCard["id"] | "custom_card";
        card.title = jsonCard["title"] | card.id.c_str();
        card.type = jsonCard["type"] | "metric";
        card.order = jsonCard["order"] | 0;
        card.value = jsonCard["data"]["value"] | "--";
        card.label = jsonCard["body"]["label"] | "";
        card.unit = jsonCard["body"]["unit"] | "";
        card.maxValue = std::max(1, jsonCard["body"]["max"] | 100);
        card.accent = jsonCard["theme"]["accent"] | "#74f0c1";
        card.actionId = jsonCard["action"]["id"] | "";
        card.actionLabel = jsonCard["action"]["label"] | "Executar";
        JsonArray jsonItems = jsonCard["body"]["items"].as<JsonArray>();
        for (const char* item : jsonItems) {
            if (item) card.items.emplace_back(item);
        }
        cards.push_back(std::move(card));
    };

    JsonArray configCards = document["cards"].as<JsonArray>();
    if (!configCards.isNull()) {
        for (JsonObject jsonCard : configCards) appendCard(jsonCard);
    } else if (strcmp(document["kind"] | "", "desk-assistant-card") == 0 &&
               document["packageVersion"] == 1) {
        JsonObject packageCard = document["card"].as<JsonObject>();
        if (!packageCard.isNull()) appendCard(packageCard);
    }

    std::sort(cards.begin(), cards.end(), [](const Card& left, const Card& right) {
        return left.order < right.order;
    });
    return !cards.empty();
}

void useDemoCards() {
    cards = {
        {"claude_usage", "Claude", "metric", "6", "Tokens hoje", "%", {}, "#74f0c1", "", "", 100, 0},
        {"wifi_status", "Wi-Fi", "status", "Conectado", "-63 dBm", "", {}, "#89b4fa", "", "", 100, 1},
        {"temperature", "Temperatura", "progress", "24", "Sala", "°C", {}, "#f9e2af", "", "", 40, 2}
    };
}

void flushCallback(lv_disp_drv_t* display, const lv_area_t* area, lv_color_t* color) {
    const int width = area->x2 - area->x1 + 1;
    const int height = area->y2 - area->y1 + 1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const uint16_t raw = color[y * width + x].full;
            const uint8_t red = static_cast<uint8_t>(((raw >> 11) & 0x1f) * 255 / 31);
            const uint8_t green = static_cast<uint8_t>(((raw >> 5) & 0x3f) * 255 / 63);
            const uint8_t blue = static_cast<uint8_t>((raw & 0x1f) * 255 / 31);
            const int targetX = area->x1 + x;
            const int targetY = area->y1 + y;
            if (targetX >= 0 && targetX < SCREEN_WIDTH && targetY >= 0 && targetY < SCREEN_HEIGHT) {
                frameBuffer[targetY * SCREEN_WIDTH + targetX] =
                    0xff000000U | (static_cast<uint32_t>(red) << 16) |
                    (static_cast<uint32_t>(green) << 8) | blue;
            }
        }
    }
    lv_disp_flush_ready(display);
}

void pointerCallback(lv_indev_drv_t*, lv_indev_data_t* data) {
    data->point.x = mouseX;
    data->point.y = mouseY;
    data->state = mousePressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

void addValueLabel(lv_obj_t* root, const Card& card) {
    lv_obj_t* value = lv_label_create(root);
    std::string text = card.value;
    if (card.type == "clock") text = "12:34:56";
    if (card.type == "list" && !card.items.empty()) {
        text.clear();
        for (size_t i = 0; i < card.items.size(); ++i) {
            if (i > 0) text += "\n";
            text += card.items[i];
        }
    }
    if (!card.unit.empty() && card.type != "status") text += " " + card.unit;
    lv_label_set_text(value, text.c_str());
    lv_obj_set_style_text_font(value, card.type == "text" ? &lv_font_montserrat_14 : &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(value, lvColor(card.accent), 0);
    lv_obj_set_width(value, 280);
    lv_label_set_long_mode(value, LV_LABEL_LONG_WRAP);
    lv_obj_align(value, LV_ALIGN_CENTER, 0, 4);
}

void addCardContent(lv_obj_t* root, const Card& card) {
    if (card.type == "progress") {
        lv_obj_t* value = lv_label_create(root);
        std::string text = card.value + (card.unit.empty() ? "" : " " + card.unit);
        lv_label_set_text(value, text.c_str());
        lv_obj_set_style_text_font(value, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(value, lvColor(card.accent), 0);
        lv_obj_align(value, LV_ALIGN_CENTER, 0, -12);

        lv_obj_t* bar = lv_bar_create(root);
        lv_obj_set_size(bar, 280, 14);
        lv_obj_align(bar, LV_ALIGN_CENTER, 0, 20);
        lv_bar_set_range(bar, 0, card.maxValue);
        lv_bar_set_value(bar, std::clamp(parseInteger(card.value), 0, card.maxValue), LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x313244), 0);
        lv_obj_set_style_bg_color(bar, lvColor(card.accent), LV_PART_INDICATOR);
        return;
    }
    if (card.type == "chart") {
        lv_obj_t* chart = lv_chart_create(root);
        lv_obj_set_size(chart, 280, 86);
        lv_obj_align(chart, LV_ALIGN_CENTER, 0, 9);
        lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(chart, 12);
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, card.maxValue);
        lv_chart_series_t* series = lv_chart_add_series(chart, lvColor(card.accent), LV_CHART_AXIS_PRIMARY_Y);
        int point = 0;
        size_t start = 0;
        while (point < 12 && start <= card.value.size()) {
            const size_t end = card.value.find(',', start);
            const std::string sample = card.value.substr(start, end == std::string::npos ? end : end - start);
            lv_chart_set_value_by_id(chart, series, point++, std::clamp(parseInteger(sample), 0, card.maxValue));
            if (end == std::string::npos) break;
            start = end + 1;
        }
        while (point < 12) lv_chart_set_value_by_id(chart, series, point++, 0);
        lv_chart_refresh(chart);
        return;
    }
    addValueLabel(root, card);
}

void buildCard(lv_obj_t* tile, const Card& card) {
    lv_obj_t* root = lv_obj_create(tile);
    lv_obj_set_size(root, SCREEN_WIDTH - 2 * CARD_MARGIN, SCREEN_HEIGHT - STATUS_HEIGHT - 2 * CARD_MARGIN);
    lv_obj_center(root);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x1e1e2e), 0);
    lv_obj_set_style_border_color(root, lv_color_hex(0x45475a), 0);
    lv_obj_set_style_border_width(root, 1, 0);
    lv_obj_set_style_radius(root, 12, 0);
    lv_obj_set_style_pad_all(root, 10, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(root);
    lv_label_set_text(title, card.title.c_str());
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xcdd6f4), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 2, 0);

    lv_obj_t* type = lv_label_create(root);
    lv_label_set_text(type, card.type.c_str());
    lv_obj_set_style_text_font(type, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(type, lv_color_hex(0xa6adc8), 0);
    lv_obj_align(type, LV_ALIGN_TOP_RIGHT, -2, 1);

    addCardContent(root, card);

    if (!card.actionId.empty()) {
        lv_obj_t* action = lv_btn_create(root);
        lv_obj_set_size(action, 92, 24);
        lv_obj_align(action, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_add_event_cb(action, [](lv_event_t* event) {
            const char* actionId = static_cast<const char*>(lv_event_get_user_data(event));
            std::cout << "[simulator] action requested: " << (actionId ? actionId : "") << "\n";
        }, LV_EVENT_CLICKED, const_cast<char*>(card.actionId.c_str()));
        lv_obj_t* actionLabel = lv_label_create(action);
        lv_label_set_text(actionLabel, card.actionLabel.c_str());
        lv_obj_center(actionLabel);
    }

    lv_obj_t* detail = lv_label_create(root);
    lv_label_set_text(detail, card.label.c_str());
    lv_obj_set_style_text_font(detail, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(detail, lv_color_hex(0xa6adc8), 0);
    lv_obj_align(detail, LV_ALIGN_BOTTOM_LEFT, 2, -2);
}

void buildUi() {
    lv_obj_t* screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x11111b), 0);

    lv_obj_t* status = lv_obj_create(screen);
    lv_obj_set_size(status, SCREEN_WIDTH, STATUS_HEIGHT);
    lv_obj_align(status, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status, lv_color_hex(0x181825), 0);
    lv_obj_set_style_border_width(status, 0, 0);
    lv_obj_set_style_pad_all(status, 3, 0);
    lv_obj_clear_flag(status, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* wifi = lv_label_create(status);
    lv_label_set_text(wifi, "WiFi -63 dBm");
    lv_obj_set_style_text_font(wifi, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi, lv_color_hex(0xa6e3a1), 0);
    lv_obj_align(wifi, LV_ALIGN_LEFT_MID, 5, 0);

    lv_obj_t* clock = lv_label_create(status);
    lv_label_set_text(clock, "12:34");
    lv_obj_set_style_text_font(clock, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(clock, lv_color_hex(0xcdd6f4), 0);
    lv_obj_align(clock, LV_ALIGN_RIGHT_MID, -7, 0);

    tileView = lv_tileview_create(screen);
    lv_obj_set_size(tileView, SCREEN_WIDTH, SCREEN_HEIGHT - STATUS_HEIGHT);
    lv_obj_align(tileView, LV_ALIGN_TOP_MID, 0, STATUS_HEIGHT);
    lv_obj_set_style_bg_color(tileView, lv_color_hex(0x11111b), 0);
    lv_obj_set_style_border_width(tileView, 0, 0);

    for (size_t index = 0; index < cards.size(); ++index) {
        lv_dir_t direction = LV_DIR_NONE;
        if (index > 0) direction = static_cast<lv_dir_t>(direction | LV_DIR_LEFT);
        if (index + 1 < cards.size()) direction = static_cast<lv_dir_t>(direction | LV_DIR_RIGHT);
        lv_obj_t* tile = lv_tileview_add_tile(tileView, index, 0, direction);
        lv_obj_set_style_bg_color(tile, lv_color_hex(0x11111b), 0);
        lv_obj_set_style_pad_all(tile, 2, 0);
        buildCard(tile, cards[index]);
    }
}

void showFrame() {
    SDL_UpdateTexture(texture, nullptr, frameBuffer.data(), SCREEN_WIDTH * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

void selectCard(int delta) {
    if (cards.empty()) return;
    currentIndex = (currentIndex + cards.size() + delta) % cards.size();
    lv_obj_set_tile_id(tileView, currentIndex, 0, LV_ANIM_ON);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string configPath = argc > 1 ? argv[1] : "simulator/demo-config.json";
    if (!loadConfig(configPath)) {
        std::cerr << "Aviso: não foi possível carregar " << configPath << "; usando demo embutida.\n";
        useDemoCards();
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init falhou: " << SDL_GetError() << "\n";
        return 1;
    }
    window = SDL_CreateWindow("Desk Assistant — LVGL Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREEN_WIDTH * 3, SCREEN_HEIGHT * 3, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!window || !renderer || !texture) {
        std::cerr << "SDL não conseguiu criar a janela: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    lv_init();
    static lv_disp_draw_buf_t drawBuffer;
    static lv_color_t buffer[SCREEN_WIDTH * 40];
    lv_disp_draw_buf_init(&drawBuffer, buffer, nullptr, SCREEN_WIDTH * 40);
    static lv_disp_drv_t displayDriver;
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = SCREEN_WIDTH;
    displayDriver.ver_res = SCREEN_HEIGHT;
    displayDriver.flush_cb = flushCallback;
    displayDriver.draw_buf = &drawBuffer;
    lv_disp_drv_register(&displayDriver);

    static lv_indev_drv_t inputDriver;
    lv_indev_drv_init(&inputDriver);
    inputDriver.type = LV_INDEV_TYPE_POINTER;
    inputDriver.read_cb = pointerCallback;
    lv_indev_drv_register(&inputDriver);

    buildUi();
    showFrame();

    bool running = true;
    uint32_t lastTick = SDL_GetTicks();
    uint32_t lastAutoSlide = lastTick;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT) selectCard(1);
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFT) selectCard(-1);
            if (event.type == SDL_MOUSEMOTION) { int windowWidth = SCREEN_WIDTH; int windowHeight = SCREEN_HEIGHT; SDL_GetWindowSize(window, &windowWidth, &windowHeight); mouseX = event.motion.x * SCREEN_WIDTH / std::max(1, windowWidth); mouseY = event.motion.y * SCREEN_HEIGHT / std::max(1, windowHeight); }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) mousePressed = true;
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) mousePressed = false;
        }
        const uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - lastTick);
        lastTick = now;
        if (now - lastAutoSlide >= 5000) { selectCard(1); lastAutoSlide = now; }
        lv_timer_handler();
        showFrame();
        SDL_Delay(5);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
