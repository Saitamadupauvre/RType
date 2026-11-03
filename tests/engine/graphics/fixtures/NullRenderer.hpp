#pragma once

#include <string>

#include "engine/core/Camera.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/graphics/Types.hpp"

namespace fixture {

class NullRenderer : public engine::graphics::IRenderer {
public:
    bool open(const engine::graphics::WindowConfig&) override { return true; }
    void close() override {}
    [[nodiscard]] bool should_close() const override { return false; }
    void set_fullscreen(bool) override {}
    [[nodiscard]] engine::core::Vec2 virtual_size() const override { return {1920, 1080}; }
    [[nodiscard]] engine::graphics::TextureId load_texture(const std::string&) override {
        return {};
    }
    void unload_texture(engine::graphics::TextureId) override {}
    [[nodiscard]] engine::core::Vec2 texture_size(engine::graphics::TextureId) const override {
        return {};
    }
    [[nodiscard]] engine::graphics::RenderTargetId
    create_render_target(engine::core::Vec2) override {
        return {};
    }
    void destroy_render_target(engine::graphics::RenderTargetId) override {}
    void begin_target(engine::graphics::RenderTargetId, engine::graphics::Color) override {}
    void end_target() override {}
    [[nodiscard]] engine::graphics::TextureId
    target_texture(engine::graphics::RenderTargetId) const override {
        return {};
    }
    [[nodiscard]] engine::graphics::ShaderId load_shader(const std::string&,
                                                         const std::string&) override {
        return {};
    }
    void unload_shader(engine::graphics::ShaderId) override {}
    void begin_shader(engine::graphics::ShaderId) override {}
    void end_shader() override {}
    void set_uniform(engine::graphics::ShaderId, const std::string&, float) override {}
    void set_uniform(engine::graphics::ShaderId, const std::string&, engine::core::Vec2) override {}
    void set_uniform(engine::graphics::ShaderId, const std::string&,
                     engine::graphics::Color) override {}
    void set_uniform(engine::graphics::ShaderId, const std::string&,
                     engine::graphics::TextureId) override {}
    void begin_frame(engine::graphics::Color) override {}
    void end_frame() override {}
    void begin_world(const engine::core::CameraView&) override {}
    void end_world() override {}
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

} // namespace fixture
