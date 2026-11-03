#pragma once

#include <string_view>
#include <vector>

#include "engine/core/Components.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/HudCommand.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

class DeclaredAssets;

class HudCanvas {
public:
    explicit HudCanvas(const DeclaredAssets& textures);

    void bind(sol::state& lua);
    void open() noexcept;
    [[nodiscard]] std::vector<HudCommand> close() noexcept;

private:
    void text(sol::this_state state, const sol::object& text, const sol::object& x,
              const sol::object& y, const sol::object& size, const sol::object& color);
    void rect(sol::this_state state, const sol::object& x, const sol::object& y,
              const sol::object& width, const sol::object& height, const sol::object& color,
              const sol::object& filled);
    void sprite(sol::this_state state, const sol::object& texture, const sol::object& x,
                const sol::object& y, const sol::object& options);
    void require_open(sol::this_state state, std::string_view function) const;
    void push(HudCommand command);

    const DeclaredAssets& _textures;
    std::vector<HudCommand> _commands;
    bool _open{false};
    bool _overflow_logged{false};
};

} // namespace engine::script
