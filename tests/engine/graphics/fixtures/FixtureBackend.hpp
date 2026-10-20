#pragma once

#include "engine/core/Vec2.hpp"
#include "engine/graphics/IGraphicsBackend.hpp"

namespace fixture {

class Renderer final : public engine::graphics::IRenderer {
public:
    bool open(const engine::graphics::WindowConfig&) override { return true; }
    void close() override {}
    [[nodiscard]] bool should_close() const override { return true; }
    void set_fullscreen(bool) override {}
    [[nodiscard]] engine::core::Vec2 virtual_size() const override { return {1920, 1080}; }
    void begin_frame(engine::graphics::Color) override {}
    void end_frame() override {}
};

class Input final : public engine::graphics::IInput {
public:
    [[nodiscard]] bool is_key_down(engine::graphics::Key) const override { return false; }
    [[nodiscard]] bool is_key_pressed(engine::graphics::Key) const override { return false; }
    [[nodiscard]] bool is_key_released(engine::graphics::Key) const override { return false; }
    [[nodiscard]] engine::graphics::Key last_key_pressed() override {
        return engine::graphics::Key::Unknown;
    }
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
