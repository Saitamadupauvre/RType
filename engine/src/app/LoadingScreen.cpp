#include "LoadingScreen.hpp"

#include <string>

namespace engine::app {

namespace {

constexpr float bar_width_share = 0.5F;
constexpr float bar_height = 24.F;
constexpr float text_size = 32.F;
constexpr float text_gap = 16.F;
constexpr graphics::Color foreground{.r = 255, .g = 255, .b = 255, .a = 255};

float progress_of(std::size_t loaded, std::size_t total) {
    return total == 0 ? 0.F : static_cast<float>(loaded) / static_cast<float>(total);
}

graphics::Rect centered_bar(core::Vec2 screen) {
    const float width = screen.x * bar_width_share;
    return {.x = (screen.x - width) / 2.F,
            .y = (screen.y - bar_height) / 2.F,
            .width = width,
            .height = bar_height};
}

void draw_bar(graphics::IRenderer& renderer, const graphics::Rect& bar, float progress) {
    renderer.draw_rect(bar, foreground, false);
    renderer.draw_rect(
        {.x = bar.x, .y = bar.y, .width = bar.width * progress, .height = bar.height}, foreground);
}

void draw_count(graphics::IRenderer& renderer, const graphics::Rect& bar, std::size_t loaded,
                std::size_t total) {
    const std::string text = std::to_string(loaded) + " / " + std::to_string(total);
    const core::Vec2 size = renderer.measure_text(text, text_size);
    renderer.draw_text(text, {bar.x + ((bar.width - size.x) / 2.F), bar.y - text_gap - size.y},
                       text_size, foreground);
}

} // namespace

void draw_loading_screen(graphics::IRenderer& renderer, std::size_t loaded, std::size_t total) {
    const graphics::Rect bar = centered_bar(renderer.virtual_size());
    draw_bar(renderer, bar, progress_of(loaded, total));
    draw_count(renderer, bar, loaded, total);
}

} // namespace engine::app
