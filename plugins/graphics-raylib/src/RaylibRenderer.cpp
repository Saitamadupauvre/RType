#include "RaylibRenderer.hpp"

#include "Letterbox.hpp"

namespace {

constexpr engine::core::Vec2 virtual_resolution{1920, 1080};

::Color to_raylib(engine::graphics::Color color) {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
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
    SetExitKey(KEY_NULL);
    _open = true;
    set_fullscreen(config.fullscreen);
    return true;
}

void RaylibRenderer::close() {
    if (!_open) {
        return;
    }
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
