#pragma once

#include "engine/graphics/IInput.hpp"
#include "engine/graphics/Types.hpp"

class RaylibInput final : public engine::graphics::IInput {
public:
    [[nodiscard]] bool is_key_down(engine::graphics::Key key) const override;
    [[nodiscard]] bool is_key_pressed(engine::graphics::Key key) const override;
    [[nodiscard]] bool is_key_released(engine::graphics::Key key) const override;
    [[nodiscard]] engine::graphics::Key last_key_pressed() override;
    [[nodiscard]] engine::graphics::Vec2 mouse_position() const override;
    [[nodiscard]] bool is_mouse_down(engine::graphics::MouseButton button) const override;
    [[nodiscard]] bool is_mouse_pressed(engine::graphics::MouseButton button) const override;
    [[nodiscard]] bool is_mouse_released(engine::graphics::MouseButton button) const override;
};
