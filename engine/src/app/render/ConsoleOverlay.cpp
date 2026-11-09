#include "render/ConsoleOverlay.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace engine::app {

namespace {

using core::console::LineKind;

constexpr float panel_share = 0.45F;
constexpr float padding = 10.F;
constexpr float text_size = 18.F;
constexpr float line_height = 22.F;
constexpr float cursor_width = 2.F;
constexpr std::string_view prompt = "> ";

constexpr graphics::Color panel_color{.r = 10, .g = 12, .b = 20, .a = 225};
constexpr graphics::Color separator_color{.r = 90, .g = 100, .b = 130, .a = 255};
constexpr graphics::Color input_color{.r = 140, .g = 200, .b = 255, .a = 255};
constexpr graphics::Color output_color{.r = 235, .g = 235, .b = 235, .a = 255};
constexpr graphics::Color error_color{.r = 255, .g = 110, .b = 110, .a = 255};
constexpr graphics::Color log_color{.r = 160, .g = 160, .b = 170, .a = 255};

graphics::Color color_of(LineKind kind) {
    switch (kind) {
    case LineKind::input:
        return input_color;
    case LineKind::output:
        return output_color;
    case LineKind::error:
        return error_color;
    case LineKind::log:
        return log_color;
    }
    return output_color;
}

void draw_scrollback(graphics::IRenderer& renderer, const core::console::Console& console,
                     float bottom) {
    const auto& lines = console.lines();
    const auto visible = static_cast<std::size_t>(std::max(0.F, (bottom - padding) / line_height));
    const std::size_t end = lines.size() - std::min(console.scroll_offset(), lines.size());
    const std::size_t begin = end - std::min(visible, end);
    float y = bottom - (static_cast<float>(end - begin) * line_height);
    for (std::size_t index = begin; index < end; ++index) {
        renderer.draw_text(lines[index].text, {padding, y}, text_size, color_of(lines[index].kind));
        y += line_height;
    }
}

void draw_input(graphics::IRenderer& renderer, const core::console::Console& console, float y) {
    const std::string line = std::string(prompt) + console.input();
    renderer.draw_text(line, {padding, y}, text_size, input_color);
    const std::string before_cursor =
        std::string(prompt) + console.input().substr(0, console.cursor());
    const float x = padding + renderer.measure_text(before_cursor, text_size).x;
    renderer.draw_rect({.x = x, .y = y, .width = cursor_width, .height = text_size}, input_color);
}

} // namespace

void draw_console(graphics::IRenderer& renderer, const core::console::Console& console) {
    const core::Vec2 screen = renderer.virtual_size();
    const float height = screen.y * panel_share;
    renderer.draw_rect({.x = 0.F, .y = 0.F, .width = screen.x, .height = height}, panel_color);
    const float input_y = height - padding - line_height;
    renderer.draw_line({0.F, input_y - (padding / 2.F)}, {screen.x, input_y - (padding / 2.F)}, 1.F,
                       separator_color);
    draw_scrollback(renderer, console, input_y - padding);
    draw_input(renderer, console, input_y);
}

} // namespace engine::app
