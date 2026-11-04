#include "client/HudCanvas.hpp"

#include <format>
#include <stdexcept>
#include <string>
#include <utility>

#include "assets/DeclaredAssets.hpp"
#include "engine/core/log/Log.hpp"
#include "lua/LuaArgs.hpp"
#include "lua/LuaErrors.hpp"
#include "lua/LuaFieldValue.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

[[noreturn]] void draw_error(sol::this_state state, std::string_view function,
                             std::string_view message) {
    throw std::runtime_error(
        std::format("{}draw.{}: {}", caller_location(state), function, message));
}

float number(sol::this_state state, const sol::object& value, std::string_view function,
             std::string_view what) {
    const auto found = finite_number(value);
    if (!found) {
        draw_error(state, function, std::format("{} must be a finite number", what));
    }
    return *found;
}

float length(sol::this_state state, const sol::object& value, std::string_view function,
             std::string_view what) {
    const float found = number(state, value, function, what);
    if (found < 0.F) {
        draw_error(state, function, std::format("{} must not be negative", what));
    }
    return found;
}

core::Rgba color_or_white(sol::this_state state, const sol::object& value,
                          std::string_view function) {
    if (is_nil(value)) {
        return core::Rgba{};
    }
    return std::get<core::Rgba>(
        from_lua(state, value, core::FieldKind::Color, std::format("draw.{}", function), "color"));
}

core::Vec2 scale_of(sol::this_state state, const sol::object& value) {
    if (is_nil(value)) {
        return {1.F, 1.F};
    }
    if (value.get_type() == sol::type::number) {
        const float uniform = number(state, value, "sprite", "scale");
        return {uniform, uniform};
    }
    if (value.get_type() != sol::type::table) {
        draw_error(state, "sprite", "scale must be a number or a { x, y } table");
    }
    const auto table = value.as<sol::table>();
    return {number(state, table.raw_get<sol::object>("x"), "sprite", "scale.x"),
            number(state, table.raw_get<sol::object>("y"), "sprite", "scale.y")};
}

} // namespace

HudCanvas::HudCanvas(const DeclaredAssets& textures) : _textures(textures) {}

void HudCanvas::bind(sol::state& lua) {
    sol::table table = lua.create_named_table("draw");
    table.set_function(
        "text", [this](sol::this_state state, const sol::object& text, const sol::object& x,
                       const sol::object& y, const sol::object& size,
                       const sol::object& color) { this->text(state, text, x, y, size, color); });
    table.set_function(
        "rect",
        [this](sol::this_state state, const sol::object& x, const sol::object& y,
               const sol::object& width, const sol::object& height, const sol::object& color,
               const sol::object& filled) { rect(state, x, y, width, height, color, filled); });
    table.set_function(
        "sprite", [this](sol::this_state state, const sol::object& texture, const sol::object& x,
                         const sol::object& y,
                         const sol::object& options) { sprite(state, texture, x, y, options); });
}

void HudCanvas::open() noexcept {
    _commands.clear();
    _open = true;
    _overflow_logged = false;
}

std::vector<HudCommand> HudCanvas::close() noexcept {
    _open = false;
    return std::exchange(_commands, {});
}

void HudCanvas::require_open(sol::this_state state, std::string_view function) const {
    if (!_open) {
        draw_error(state, function, "only callable from draw_hud()");
    }
}

void HudCanvas::push(HudCommand command) {
    if (_commands.size() < max_hud_commands) {
        _commands.push_back(std::move(command));
        return;
    }
    if (!_overflow_logged) {
        _overflow_logged = true;
        log::warn("lua: draw: more than {} commands in one frame, the rest are dropped",
                  max_hud_commands);
    }
}

void HudCanvas::text(sol::this_state state, const sol::object& text, const sol::object& x,
                     const sol::object& y, const sol::object& size, const sol::object& color) {
    require_open(state, "text");
    if (text.get_type() != sol::type::string) {
        draw_error(state, "text", "text must be a string");
    }
    auto content = text.as<std::string>();
    if (content.size() > max_hud_text_length) {
        draw_error(state, "text",
                   std::format("text must be at most {} bytes", max_hud_text_length));
    }
    const auto height = positive_number(size);
    if (!height) {
        draw_error(state, "text", "size must be a positive number");
    }
    HudText command{.text = std::move(content),
                    .position = {number(state, x, "text", "x"), number(state, y, "text", "y")},
                    .size = *height,
                    .color = color_or_white(state, color, "text")};
    push(std::move(command));
}

void HudCanvas::rect(sol::this_state state, const sol::object& x, const sol::object& y,
                     const sol::object& width, const sol::object& height, const sol::object& color,
                     const sol::object& filled) {
    require_open(state, "rect");
    HudRect command{.position = {number(state, x, "rect", "x"), number(state, y, "rect", "y")},
                    .size = {length(state, width, "rect", "w"), length(state, height, "rect", "h")},
                    .color = color_or_white(state, color, "rect"),
                    .filled = true};
    if (!is_nil(filled)) {
        if (filled.get_type() != sol::type::boolean) {
            draw_error(state, "rect", "filled must be a boolean");
        }
        command.filled = filled.as<bool>();
    }
    push(command);
}

void HudCanvas::sprite(sol::this_state state, const sol::object& texture, const sol::object& x,
                       const sol::object& y, const sol::object& options) {
    require_open(state, "sprite");
    if (texture.get_type() != sol::type::string) {
        draw_error(state, "sprite", "texture must be a string");
    }
    auto name = texture.as<std::string>();
    if (!_textures.contains(name)) {
        draw_error(state, "sprite",
                   std::format("unknown texture '{}', declare it with textures", name));
    }
    HudSprite command{
        .texture = std::move(name),
        .position = {number(state, x, "sprite", "x"), number(state, y, "sprite", "y")},
        .scale = {1.F, 1.F},
        .rotation = 0.F,
        .color = {}};
    if (!is_nil(options)) {
        if (options.get_type() != sol::type::table) {
            draw_error(state, "sprite", "options must be a table");
        }
        const auto table = options.as<sol::table>();
        command.scale = scale_of(state, table.raw_get<sol::object>("scale"));
        const auto rotation = table.raw_get<sol::object>("rotation");
        command.rotation = is_nil(rotation) ? 0.F : number(state, rotation, "sprite", "rotation");
        command.color = color_or_white(state, table.raw_get<sol::object>("color"), "sprite");
    }
    push(std::move(command));
}

} // namespace engine::script
