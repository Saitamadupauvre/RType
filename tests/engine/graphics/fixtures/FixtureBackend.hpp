#pragma once

#include "NullRenderer.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/graphics/IGraphicsBackend.hpp"

namespace fixture {

class Renderer final : public NullRenderer {
public:
    [[nodiscard]] bool should_close() const override { return true; }
};

class Input final : public engine::graphics::IInput {
public:
    [[nodiscard]] bool is_key_down(engine::graphics::Key) const override { return false; }
    [[nodiscard]] bool is_key_pressed(engine::graphics::Key) const override { return false; }
    [[nodiscard]] bool is_key_released(engine::graphics::Key) const override { return false; }
    [[nodiscard]] engine::graphics::Key last_key_pressed() override {
        return engine::graphics::Key::Unknown;
    }
    [[nodiscard]] char32_t next_character() override { return U'\0'; }
    [[nodiscard]] engine::core::Vec2 mouse_position() const override { return {}; }
    [[nodiscard]] bool is_mouse_down(engine::graphics::MouseButton) const override { return false; }
    [[nodiscard]] bool is_mouse_pressed(engine::graphics::MouseButton) const override {
        return false;
    }
    [[nodiscard]] bool is_mouse_released(engine::graphics::MouseButton) const override {
        return false;
    }
};

class Backend final : public engine::graphics::IGraphicsBackend {
public:
    engine::graphics::IRenderer& renderer() override { return _renderer; }
    engine::graphics::IInput& input() override { return _input; }

private:
    Renderer _renderer;
    Input _input;
};

} // namespace fixture
