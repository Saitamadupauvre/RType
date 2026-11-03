#include "RaylibRenderer.hpp"

#include <rlgl.h>

#include <array>
#include <string_view>

#include "Letterbox.hpp"
#include "TargetSource.hpp"
#include "engine/core/log/Log.hpp"

namespace {

constexpr int placeholder_size = 32;
constexpr int placeholder_checks = 4;
constexpr float text_spacing_ratio = 0.1F;
constexpr float outline_thickness = 1.F;
constexpr float max_target_side = 16384.F;

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

engine::core::Vec2 size_of(const Texture2D& texture) {
    return {static_cast<float>(texture.width), static_cast<float>(texture.height)};
}

const char* path_or_default(const std::string& path) {
    return path.empty() ? nullptr : path.c_str();
}

bool shader_file_exists(std::string_view kind, const std::string& path) {
    if (path.empty() || FileExists(path.c_str())) {
        return true;
    }
    engine::core::log::warn("graphics: cannot find {} shader {}", kind, path);
    return false;
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
    end_frame_state();
    for (const Target& target : _targets.remove_all()) {
        release(target);
    }
    for (const LoadedShader& loaded : _shaders.remove_all()) {
        UnloadShader(loaded.shader);
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
    if (is_target_texture(texture)) {
        return;
    }
    if (const auto removed = _textures.remove(texture)) {
        UnloadTexture(*removed);
    }
}

engine::core::Vec2 RaylibRenderer::texture_size(engine::graphics::TextureId texture) const {
    return size_of(texture_or_placeholder(texture));
}

const Texture2D& RaylibRenderer::texture_or_placeholder(engine::graphics::TextureId id) const {
    const Texture2D* found = _textures.find(id);
    return found == nullptr ? _placeholder : *found;
}

bool RaylibRenderer::is_target_texture(engine::graphics::TextureId id) const {
    return _target_textures.contains(id.value);
}

engine::core::Vec2 RaylibRenderer::surface_size() const {
    const Target* current = _targets.find(_current_target);
    return current == nullptr ? virtual_resolution : size_of(current->surface.texture);
}

engine::graphics::RenderTargetId RaylibRenderer::create_render_target(engine::core::Vec2 size) {
    if (!_open) {
        engine::core::log::warn("graphics: cannot create a render target before the window is "
                                "open");
        return {};
    }
    if (!(size.x >= 1.F && size.y >= 1.F && size.x <= max_target_side &&
          size.y <= max_target_side)) {
        engine::core::log::warn("graphics: cannot create a {} x {} render target", size.x, size.y);
        return {};
    }
    const RenderTexture2D surface =
        LoadRenderTexture(static_cast<int>(size.x), static_cast<int>(size.y));
    if (!IsRenderTextureValid(surface)) {
        engine::core::log::warn("graphics: cannot create a {} x {} render target", size.x, size.y);
        return {};
    }
    const engine::graphics::TextureId texture = _textures.add(surface.texture);
    _target_textures.insert(texture.value);
    return _targets.add({.surface = surface, .texture = texture});
}

void RaylibRenderer::release(const Target& target) {
    _textures.remove(target.texture);
    _target_textures.erase(target.texture.value);
    UnloadRenderTexture(target.surface);
}

void RaylibRenderer::destroy_render_target(engine::graphics::RenderTargetId target) {
    if (_targets.find(target) == nullptr) {
        return;
    }
    if (_current_target == target) {
        end_target();
    }
    rlDrawRenderBatchActive();
    if (const auto removed = _targets.remove(target)) {
        release(*removed);
    }
}

void RaylibRenderer::begin_target(engine::graphics::RenderTargetId target,
                                  engine::graphics::Color clear) {
    const Target* found = _targets.find(target);
    if (!_in_frame || found == nullptr) {
        return;
    }
    end_world();
    EndTextureMode();
    BeginTextureMode(found->surface);
    ClearBackground(to_raylib(clear));
    _current_target = target;
}

void RaylibRenderer::end_target() {
    if (_current_target == engine::graphics::RenderTargetId{}) {
        return;
    }
    end_world();
    EndTextureMode();
    BeginTextureMode(_target);
    _current_target = {};
}

engine::graphics::TextureId
RaylibRenderer::target_texture(engine::graphics::RenderTargetId target) const {
    const Target* found = _targets.find(target);
    return found == nullptr ? engine::graphics::TextureId{} : found->texture;
}

engine::graphics::ShaderId RaylibRenderer::load_shader(const std::string& vertex,
                                                       const std::string& fragment) {
    if (!_open) {
        engine::core::log::warn("graphics: cannot load shader {} {} before the window is open",
                                vertex, fragment);
        return {};
    }
    if (vertex.empty() && fragment.empty()) {
        engine::core::log::warn("graphics: a shader needs a vertex or a fragment file");
        return {};
    }
    if (!shader_file_exists("vertex", vertex) || !shader_file_exists("fragment", fragment)) {
        return {};
    }
    const Shader shader = LoadShader(path_or_default(vertex), path_or_default(fragment));
    if (!IsShaderValid(shader) || shader.id == rlGetShaderIdDefault()) {
        MemFree(shader.locs);
        engine::core::log::warn("graphics: cannot compile shader {} {}, drawing without it", vertex,
                                fragment);
        return {};
    }
    return _shaders.add({.shader = shader, .uniforms = {}});
}

void RaylibRenderer::unload_shader(engine::graphics::ShaderId shader) {
    if (_shaders.find(shader) == nullptr) {
        return;
    }
    if (_current_shader == shader) {
        end_shader();
    }
    rlDrawRenderBatchActive();
    if (const auto removed = _shaders.remove(shader)) {
        UnloadShader(removed->shader);
    }
}

void RaylibRenderer::begin_shader(engine::graphics::ShaderId shader) {
    if (!_in_frame) {
        return;
    }
    const LoadedShader* found = _shaders.find(shader);
    if (found == nullptr) {
        end_shader();
        return;
    }
    BeginShaderMode(found->shader);
    _current_shader = shader;
}

void RaylibRenderer::end_shader() {
    if (_current_shader == engine::graphics::ShaderId{}) {
        return;
    }
    EndShaderMode();
    _current_shader = {};
}

int RaylibRenderer::uniform_location(LoadedShader& loaded, const std::string& name) {
    return loaded.uniforms.find(name, [&loaded](const std::string& uniform) {
        return GetShaderLocation(loaded.shader, uniform.c_str());
    });
}

void RaylibRenderer::set_uniform_value(engine::graphics::ShaderId shader, const std::string& name,
                                       const void* value, int type) {
    LoadedShader* found = _shaders.find(shader);
    if (found == nullptr) {
        return;
    }
    const int location = uniform_location(*found, name);
    if (location == UniformLocations::missing) {
        return;
    }
    rlDrawRenderBatchActive();
    SetShaderValue(found->shader, location, value, type);
}

void RaylibRenderer::set_uniform(engine::graphics::ShaderId shader, const std::string& name,
                                 float value) {
    set_uniform_value(shader, name, &value, SHADER_UNIFORM_FLOAT);
}

void RaylibRenderer::set_uniform(engine::graphics::ShaderId shader, const std::string& name,
                                 engine::core::Vec2 value) {
    const std::array<float, 2> components{value.x, value.y};
    set_uniform_value(shader, name, components.data(), SHADER_UNIFORM_VEC2);
}

void RaylibRenderer::set_uniform(engine::graphics::ShaderId shader, const std::string& name,
                                 engine::graphics::Color value) {
    constexpr float channel_max = 255.F;
    const std::array<float, 4> channels{
        static_cast<float>(value.r) / channel_max, static_cast<float>(value.g) / channel_max,
        static_cast<float>(value.b) / channel_max, static_cast<float>(value.a) / channel_max};
    set_uniform_value(shader, name, channels.data(), SHADER_UNIFORM_VEC4);
}

void RaylibRenderer::set_uniform(engine::graphics::ShaderId shader, const std::string& name,
                                 engine::graphics::TextureId value) {
    LoadedShader* found = _shaders.find(shader);
    if (found == nullptr) {
        return;
    }
    const int location = uniform_location(*found, name);
    if (location == UniformLocations::missing) {
        return;
    }
    rlDrawRenderBatchActive();
    SetShaderValueTexture(found->shader, location, texture_or_placeholder(value));
}

void RaylibRenderer::begin_frame(engine::graphics::Color clear) {
    _clear = clear;
    _in_frame = true;
    BeginTextureMode(_target);
    ClearBackground(to_raylib(clear));
}

void RaylibRenderer::end_frame_state() {
    end_target();
    end_world();
    end_shader();
}

void RaylibRenderer::end_frame() {
    end_frame_state();
    _in_frame = false;
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

void RaylibRenderer::begin_world(const engine::core::CameraView& view) {
    if (!_in_frame) {
        return;
    }
    _in_world = true;
    BeginMode2D(Camera2D{.offset = to_raylib(surface_size() / 2.F),
                         .target = to_raylib(view.center),
                         .rotation = 0.F,
                         .zoom = view.zoom});
}

void RaylibRenderer::end_world() {
    if (!_in_world) {
        return;
    }
    _in_world = false;
    EndMode2D();
}

void RaylibRenderer::draw_sprite(const engine::graphics::SpriteDraw& sprite) {
    const Texture2D* found = _textures.find(sprite.texture);
    const Texture2D& texture = found == nullptr ? _placeholder : *found;
    engine::graphics::Rect source = sprite.source;
    if (found == nullptr) {
        source = {.width = static_cast<float>(texture.width),
                  .height = static_cast<float>(texture.height)};
    } else if (is_target_texture(sprite.texture)) {
        source = upright_target_source(source, size_of(texture));
    }
    DrawTexturePro(texture, to_raylib(source), to_raylib(sprite.dest),
                   {.x = sprite.origin.x, .y = sprite.origin.y}, sprite.rotation,
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
