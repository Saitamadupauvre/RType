#include "raylib/ViewportPainter.hpp"

#include <cmath>
#include <utility>

#include "ui/Theme.hpp"
#include "viewport/SceneItems.hpp"
#include "viewport/ViewportState.hpp"

namespace editor {

namespace {

constexpr float outline_pixels = 1.F;
constexpr float selection_pixels = 2.F;
constexpr unsigned char grid_alpha = 28;
constexpr unsigned char axis_alpha = 70;
constexpr unsigned char marker_alpha = 120;

Color to_raylib(Clay_Color color) {
    return {.r = static_cast<unsigned char>(std::lround(color.r)),
            .g = static_cast<unsigned char>(std::lround(color.g)),
            .b = static_cast<unsigned char>(std::lround(color.b)),
            .a = static_cast<unsigned char>(std::lround(color.a))};
}

Color to_raylib(engine::core::Rgba color) {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

Color faded(Color color, unsigned char alpha) {
    color.a = alpha;
    return color;
}

Rectangle box_of(const SceneItem& item) {
    return {.x = item.top_left.x, .y = item.top_left.y, .width = item.size.x, .height = item.size.y};
}

Rectangle centered(const SceneItem& item) {
    return {.x = item.top_left.x + (item.size.x / 2.F),
            .y = item.top_left.y + (item.size.y / 2.F),
            .width = item.size.x,
            .height = item.size.y};
}

void draw_grid(const ViewportFrame& frame, Rectangle world) {
    const float thickness = outline_pixels / frame.camera.zoom;
    const Color line = faded(WHITE, grid_alpha);
    const Color axis = faded(WHITE, axis_alpha);
    for (const float x : frame.grid.xs) {
        DrawLineEx({.x = x, .y = world.y}, {.x = x, .y = world.y + world.height}, thickness,
                   x == 0.F ? axis : line);
    }
    for (const float y : frame.grid.ys) {
        DrawLineEx({.x = world.x, .y = y}, {.x = world.x + world.width, .y = y}, thickness,
                   y == 0.F ? axis : line);
    }
}

Rectangle visible_world(const ViewportFrame& frame, Clay_BoundingBox box) {
    const float width = box.width / frame.camera.zoom;
    const float height = box.height / frame.camera.zoom;
    return {.x = frame.camera.center.x - (width / 2.F),
            .y = frame.camera.center.y - (height / 2.F),
            .width = width,
            .height = height};
}

} // namespace

ViewportPainter::ViewportPainter(std::filesystem::path assets) : _assets(std::move(assets)) {}

ViewportPainter::~ViewportPainter() {
    for (auto& [path, texture] : _textures) {
        if (texture) {
            UnloadTexture(*texture);
        }
    }
}

const Texture2D* ViewportPainter::texture(std::string_view path) {
    auto found = _textures.find(std::string(path));
    if (found == _textures.end()) {
        const std::filesystem::path file = _assets / path;
        std::optional<Texture2D> loaded;
        if (std::filesystem::is_regular_file(file)) {
            const Texture2D texture = LoadTexture(file.string().c_str());
            if (texture.id != 0) {
                loaded = texture;
            }
        }
        found = _textures.emplace(std::string(path), loaded).first;
    }
    return found->second ? &*found->second : nullptr;
}

std::optional<engine::core::Vec2> ViewportPainter::texture_size(std::string_view path) {
    const Texture2D* loaded = texture(path);
    if (loaded == nullptr) {
        return std::nullopt;
    }
    return engine::core::Vec2{static_cast<float>(loaded->width),
                              static_cast<float>(loaded->height)};
}

void ViewportPainter::paint(const Clay_RenderCommand& command) {
    const auto& frame = *static_cast<const ViewportFrame*>(command.renderData.custom.customData);
    const Clay_BoundingBox box = command.boundingBox;
    DrawRectangleRec({.x = box.x, .y = box.y, .width = box.width, .height = box.height},
                     to_raylib(command.renderData.custom.backgroundColor));
    BeginScissorMode(static_cast<int>(box.x), static_cast<int>(box.y),
                     static_cast<int>(box.width), static_cast<int>(box.height));
    const Camera2D camera{.offset = {.x = box.x + (box.width / 2.F), .y = box.y + (box.height / 2.F)},
                          .target = {.x = frame.camera.center.x, .y = frame.camera.center.y},
                          .rotation = 0.F,
                          .zoom = frame.camera.zoom};
    BeginMode2D(camera);
    draw_grid(frame, visible_world(frame, box));
    for (const SceneItem& item : frame.items) {
        const Rectangle dest = centered(item);
        const Vector2 origin{.x = item.size.x / 2.F, .y = item.size.y / 2.F};
        if (!item.visible) {
            DrawRectangleLinesEx(box_of(item), outline_pixels / frame.camera.zoom,
                                 faded(WHITE, marker_alpha));
            continue;
        }
        if (const Texture2D* loaded = item.texture.empty() ? nullptr : texture(item.texture)) {
            const Rectangle source{.x = 0.F,
                                   .y = 0.F,
                                   .width = static_cast<float>(loaded->width) *
                                            (item.scale.x < 0.F ? -1.F : 1.F),
                                   .height = static_cast<float>(loaded->height) *
                                             (item.scale.y < 0.F ? -1.F : 1.F)};
            DrawTexturePro(*loaded, source, dest, origin, item.rotation, to_raylib(item.tint));
        } else {
            DrawRectanglePro(dest, origin, item.rotation, to_raylib(item.tint));
        }
    }
    for (const SceneItem& item : frame.items) {
        if (item.id == frame.selection) {
            DrawRectangleLinesEx(box_of(item), selection_pixels / frame.camera.zoom,
                                 to_raylib(color::accent));
        }
    }
    EndMode2D();
    EndScissorMode();
}

} // namespace editor
