#pragma once

#include <raylib.h>

#include <cstdint>
#include <string>
#include <unordered_set>

#include "HandleTable.hpp"
#include "UniformLocations.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/graphics/Types.hpp"

class RaylibRenderer final : public engine::graphics::IRenderer {
public:
    RaylibRenderer() = default;
    ~RaylibRenderer() override;

    bool open(const engine::graphics::WindowConfig& config) override;
    void close() override;
    [[nodiscard]] bool should_close() const override;
    void set_fullscreen(bool enabled) override;
    [[nodiscard]] engine::core::Vec2 virtual_size() const override;
    [[nodiscard]] engine::graphics::TextureId load_texture(const std::string& path) override;
    void unload_texture(engine::graphics::TextureId texture) override;
    [[nodiscard]] engine::core::Vec2
    texture_size(engine::graphics::TextureId texture) const override;
    [[nodiscard]] engine::graphics::RenderTargetId
    create_render_target(engine::core::Vec2 size) override;
    void destroy_render_target(engine::graphics::RenderTargetId target) override;
    void begin_target(engine::graphics::RenderTargetId target,
                      engine::graphics::Color clear) override;
    void end_target() override;
    [[nodiscard]] engine::graphics::TextureId
    target_texture(engine::graphics::RenderTargetId target) const override;
    [[nodiscard]] engine::graphics::ShaderId load_shader(const std::string& vertex,
                                                         const std::string& fragment) override;
    void unload_shader(engine::graphics::ShaderId shader) override;
    void begin_shader(engine::graphics::ShaderId shader) override;
    void end_shader() override;
    void set_uniform(engine::graphics::ShaderId shader, const std::string& name,
                     float value) override;
    void set_uniform(engine::graphics::ShaderId shader, const std::string& name,
                     engine::core::Vec2 value) override;
    void set_uniform(engine::graphics::ShaderId shader, const std::string& name,
                     engine::graphics::Color value) override;
    void set_uniform(engine::graphics::ShaderId shader, const std::string& name,
                     engine::graphics::TextureId value) override;
    void begin_frame(engine::graphics::Color clear) override;
    void end_frame() override;
    void begin_world(const engine::core::CameraView& view) override;
    void end_world() override;
    void draw_sprite(const engine::graphics::SpriteDraw& sprite) override;
    void draw_rect(const engine::graphics::Rect& rect, engine::graphics::Color color,
                   bool filled) override;
    void draw_circle(engine::core::Vec2 center, float radius,
                     engine::graphics::Color color) override;
    void draw_line(engine::core::Vec2 from, engine::core::Vec2 to, float thickness,
                   engine::graphics::Color color) override;
    void draw_text(const std::string& text, engine::core::Vec2 position, float size,
                   engine::graphics::Color color) override;
    [[nodiscard]] engine::core::Vec2 measure_text(const std::string& text,
                                                  float size) const override;

private:
    struct Target {
        RenderTexture2D surface;
        engine::graphics::TextureId texture;
    };

    struct LoadedShader {
        Shader shader;
        UniformLocations uniforms;
    };

    [[nodiscard]] const Texture2D& texture_or_placeholder(engine::graphics::TextureId id) const;
    [[nodiscard]] bool is_target_texture(engine::graphics::TextureId id) const;
    [[nodiscard]] engine::core::Vec2 surface_size() const;
    void release(const Target& target);
    void end_frame_state();
    void set_uniform_value(engine::graphics::ShaderId shader, const std::string& name,
                           const void* value, int type);
    [[nodiscard]] int uniform_location(LoadedShader& loaded, const std::string& name);

    bool _open{false};
    bool _in_frame{false};
    bool _in_world{false};
    RenderTexture2D _target{};
    Texture2D _placeholder{};
    HandleTable<engine::graphics::TextureId, Texture2D> _textures;
    HandleTable<engine::graphics::RenderTargetId, Target> _targets;
    HandleTable<engine::graphics::ShaderId, LoadedShader> _shaders;
    std::unordered_set<std::uint32_t> _target_textures;
    engine::graphics::RenderTargetId _current_target{};
    engine::graphics::ShaderId _current_shader{};
    engine::graphics::Color _clear{};
};
