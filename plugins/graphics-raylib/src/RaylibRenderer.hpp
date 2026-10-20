#pragma once

#include <raylib.h>

#include <string>

#include "TextureTable.hpp"
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
    void begin_frame(engine::graphics::Color clear) override;
    void end_frame() override;
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
    [[nodiscard]] const Texture2D& texture_or_placeholder(engine::graphics::TextureId id) const;

    bool _open{false};
    RenderTexture2D _target{};
    Texture2D _placeholder{};
    TextureTable<Texture2D> _textures;
    engine::graphics::Color _clear{};
};
