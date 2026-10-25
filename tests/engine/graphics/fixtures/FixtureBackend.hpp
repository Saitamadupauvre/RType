#pragma once

#include <string>

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
    [[nodiscard]] engine::graphics::TextureId load_texture(const std::string&) override {
        return {};
    }
    void unload_texture(engine::graphics::TextureId) override {}
    [[nodiscard]] engine::core::Vec2 texture_size(engine::graphics::TextureId) const override {
        return {};
    }
    void begin_frame(engine::graphics::Color) override {}
    void end_frame() override {}
    void draw_sprite(const engine::graphics::SpriteDraw&) override {}
    void draw_rect(const engine::graphics::Rect&, engine::graphics::Color, bool) override {}
    void draw_circle(engine::core::Vec2, float, engine::graphics::Color) override {}
    void draw_line(engine::core::Vec2, engine::core::Vec2, float,
                   engine::graphics::Color) override {}
    void draw_text(const std::string&, engine::core::Vec2, float,
                   engine::graphics::Color) override {}
    [[nodiscard]] engine::core::Vec2 measure_text(const std::string&, float) const override {
        return {};
    }
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
