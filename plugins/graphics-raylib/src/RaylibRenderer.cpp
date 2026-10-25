#include "RaylibRenderer.hpp"

#include "Letterbox.hpp"
#include "engine/core/log/Log.hpp"

namespace {

constexpr int placeholder_size = 32;
constexpr int placeholder_checks = 4;
constexpr float text_spacing_ratio = 0.1F;
constexpr float outline_thickness = 1.F;

::Color to_raylib(engine::graphics::Color color) {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

::Rectangle to_raylib(engine::graphics::Rect rect) {
    return {.x = rect.x, .y = rect.y, .width = rect.width, .height = rect.height};
}

::Vector2 to_raylib(engine::core::Vec2 point) { return {.x = point.x, .y = point.y}; }

Texture2D make_placeholder() {
    const Image image =
        GenImageChecked(placeholder_size, placeholder_size, placeholder_size / placeholder_checks,
                        placeholder_size / placeholder_checks, MAGENTA, BLACK);
    const Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

engine::core::Vec2 window_size() {
    return {static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())};
}

} // namespace

RaylibRenderer::~RaylibRenderer() { close(); }

bool RaylibRenderer::open(const engine::graphics::WindowConfig& config) {
    if (_open) {
        return true;
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(config.width, config.height, config.title.c_str());
    if (!IsWindowReady()) {
        return false;
    }
    _target = LoadRenderTexture(static_cast<int>(virtual_resolution.x),
                                static_cast<int>(virtual_resolution.y));
    if (!IsRenderTextureValid(_target)) {
        CloseWindow();
        return false;
    }
    SetTextureFilter(_target.texture, TEXTURE_FILTER_BILINEAR);
    _placeholder = make_placeholder();
    SetExitKey(KEY_NULL);
    _open = true;
    set_fullscreen(config.fullscreen);
    return true;
}

void RaylibRenderer::close() {
    if (!_open) {
        return;
    }
    for (const Texture2D& texture : _textures.remove_all()) {
        UnloadTexture(texture);
    }
    UnloadTexture(_placeholder);
    _placeholder = {};
    UnloadRenderTexture(_target);
    _target = {};
    CloseWindow();
    _open = false;
}

bool RaylibRenderer::should_close() const { return !_open || WindowShouldClose(); }

void RaylibRenderer::set_fullscreen(bool enabled) {
    if (_open && IsWindowFullscreen() != enabled) {
        ToggleFullscreen();
    }
}

engine::core::Vec2 RaylibRenderer::virtual_size() const { return virtual_resolution; }

engine::graphics::TextureId RaylibRenderer::load_texture(const std::string& path) {
    if (!_open) {
        engine::core::log::warn("graphics: cannot load {} before the window is open", path);
        return {};
    }
    const Texture2D texture = LoadTexture(path.c_str());
    if (!IsTextureValid(texture)) {
        engine::core::log::warn("graphics: cannot load texture {}, using the placeholder", path);
        return {};
    }
    return _textures.add(texture);
}

void RaylibRenderer::unload_texture(engine::graphics::TextureId texture) {
    if (const auto removed = _textures.remove(texture)) {
        UnloadTexture(*removed);
    }
}

engine::core::Vec2 RaylibRenderer::texture_size(engine::graphics::TextureId texture) const {
    const Texture2D& found = texture_or_placeholder(texture);
    return {static_cast<float>(found.width), static_cast<float>(found.height)};
}

const Texture2D& RaylibRenderer::texture_or_placeholder(engine::graphics::TextureId id) const {
    const Texture2D* found = _textures.find(id);
    return found == nullptr ? _placeholder : *found;
}

void RaylibRenderer::begin_frame(engine::graphics::Color clear) {
    _clear = clear;
    BeginTextureMode(_target);
    ClearBackground(to_raylib(clear));
}

void RaylibRenderer::end_frame() {
    EndTextureMode();
    BeginDrawing();
    ClearBackground(to_raylib(_clear));
    const engine::graphics::Rect view = letterbox(window_size(), virtual_resolution);
    const auto& texture = _target.texture;
    DrawTexturePro(texture,
                   {.x = 0.F,
                    .y = 0.F,
                    .width = static_cast<float>(texture.width),
                    .height = -static_cast<float>(texture.height)},
                   {.x = view.x, .y = view.y, .width = view.width, .height = view.height},
                   {.x = 0.F, .y = 0.F}, 0.F, WHITE);
    EndDrawing();
}

void RaylibRenderer::draw_sprite(const engine::graphics::SpriteDraw& sprite) {
    const Texture2D* found = _textures.find(sprite.texture);
    const Texture2D& texture = found == nullptr ? _placeholder : *found;
    const engine::graphics::Rect source =
        found == nullptr ? engine::graphics::Rect{.width = static_cast<float>(texture.width),
                                                  .height = static_cast<float>(texture.height)}
                         : sprite.source;
    DrawTexturePro(texture, to_raylib(source), to_raylib(sprite.dest), {.x = 0.F, .y = 0.F}, 0.F,
                   to_raylib(sprite.tint));
}

void RaylibRenderer::draw_rect(const engine::graphics::Rect& rect, engine::graphics::Color color,
                               bool filled) {
    if (filled) {
        DrawRectangleRec(to_raylib(rect), to_raylib(color));
        return;
    }
    DrawRectangleLinesEx(to_raylib(rect), outline_thickness, to_raylib(color));
}

void RaylibRenderer::draw_circle(engine::core::Vec2 center, float radius,
                                 engine::graphics::Color color) {
    DrawCircleV(to_raylib(center), radius, to_raylib(color));
}

void RaylibRenderer::draw_line(engine::core::Vec2 from, engine::core::Vec2 to, float thickness,
                               engine::graphics::Color color) {
    DrawLineEx(to_raylib(from), to_raylib(to), thickness, to_raylib(color));
}

void RaylibRenderer::draw_text(const std::string& text, engine::core::Vec2 position, float size,
                               engine::graphics::Color color) {
    DrawTextEx(GetFontDefault(), text.c_str(), to_raylib(position), size, size * text_spacing_ratio,
               to_raylib(color));
}

engine::core::Vec2 RaylibRenderer::measure_text(const std::string& text, float size) const {
    const Vector2 measured =
        MeasureTextEx(GetFontDefault(), text.c_str(), size, size * text_spacing_ratio);
    return {measured.x, measured.y};
}
