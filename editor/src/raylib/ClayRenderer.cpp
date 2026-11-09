#include "raylib/ClayRenderer.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "render/Geometry.hpp"

namespace editor {

namespace {

constexpr int corner_segments = 8;

Color to_raylib(Clay_Color color) {
    return {.r = static_cast<unsigned char>(std::lround(color.r)),
            .g = static_cast<unsigned char>(std::lround(color.g)),
            .b = static_cast<unsigned char>(std::lround(color.b)),
            .a = static_cast<unsigned char>(std::lround(color.a))};
}

Rectangle to_raylib(Clay_BoundingBox box) {
    return {.x = box.x, .y = box.y, .width = box.width, .height = box.height};
}

bool uniform(Clay_BorderWidth width) {
    return width.left == width.right && width.left == width.top && width.left == width.bottom;
}

} // namespace

ClayRenderer::ClayRenderer(std::filesystem::path font_directory)
    : _fonts(std::move(font_directory)) {}

void ClayRenderer::set_custom_painter(CustomPainter painter) { _custom = std::move(painter); }

void ClayRenderer::set_image_painter(CustomPainter painter) { _image = std::move(painter); }

const Font& ClayRenderer::font(Face face, std::uint16_t size) { return _fonts.get(face, size); }

Clay_Dimensions ClayRenderer::measure(std::string_view text, const Clay_TextElementConfig& config) {
    const std::string terminated(text);
    const Vector2 size = MeasureTextEx(font_for(config.fontId, config.fontSize), terminated.c_str(),
                                       static_cast<float>(config.fontSize),
                                       static_cast<float>(config.letterSpacing));
    return {.width = size.x, .height = size.y};
}

const Font& ClayRenderer::font_for(std::uint16_t font_id, std::uint16_t size) {
    return _fonts.get(static_cast<Face>(font_id), size);
}

void ClayRenderer::render(Clay_RenderCommandArray commands) {
    for (std::int32_t index = 0; index < commands.length; ++index) {
        const Clay_RenderCommand& command = *Clay_RenderCommandArray_Get(&commands, index);
        switch (command.commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            draw_rectangle(command);
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            draw_border(command);
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT:
            draw_text(command);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            begin_scissor(command);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            end_scissor();
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            if (_custom) {
                _custom(command);
            }
            break;
        case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            if (_image) {
                _image(command);
            }
            break;
        case CLAY_RENDER_COMMAND_TYPE_NONE:
            break;
        }
    }
}

void ClayRenderer::draw_rectangle(const Clay_RenderCommand& command) const {
    const Clay_RectangleRenderData& data = command.renderData.rectangle;
    const Clay_BoundingBox box = command.boundingBox;
    const float radius = data.cornerRadius.topLeft;
    if (radius > 0.F) {
        DrawRectangleRounded(to_raylib(box), roundness(radius, box.width, box.height),
                             corner_segments, to_raylib(data.backgroundColor));
        return;
    }
    DrawRectangleRec(to_raylib(box), to_raylib(data.backgroundColor));
}

void ClayRenderer::draw_border(const Clay_RenderCommand& command) const {
    const Clay_BorderRenderData& data = command.renderData.border;
    const Clay_BoundingBox box = command.boundingBox;
    const Color color = to_raylib(data.color);
    const float radius = data.cornerRadius.topLeft;
    if (radius > 0.F && uniform(data.width)) {
        DrawRectangleRoundedLinesEx(to_raylib(box), roundness(radius, box.width, box.height),
                                    corner_segments, static_cast<float>(data.width.left), color);
        return;
    }
    const auto left = static_cast<float>(data.width.left);
    const auto right = static_cast<float>(data.width.right);
    const auto top = static_cast<float>(data.width.top);
    const auto bottom = static_cast<float>(data.width.bottom);
    DrawRectangleRec({.x = box.x, .y = box.y, .width = box.width, .height = top}, color);
    DrawRectangleRec(
        {.x = box.x, .y = box.y + box.height - bottom, .width = box.width, .height = bottom},
        color);
    DrawRectangleRec({.x = box.x, .y = box.y, .width = left, .height = box.height}, color);
    DrawRectangleRec(
        {.x = box.x + box.width - right, .y = box.y, .width = right, .height = box.height}, color);
}

void ClayRenderer::draw_text(const Clay_RenderCommand& command) {
    const Clay_TextRenderData& data = command.renderData.text;
    const std::string terminated(data.stringContents.chars,
                                 static_cast<std::size_t>(data.stringContents.length));
    const Vector2 position{.x = std::round(command.boundingBox.x),
                           .y = std::round(command.boundingBox.y)};
    DrawTextEx(font_for(data.fontId, data.fontSize), terminated.c_str(), position,
               static_cast<float>(data.fontSize), static_cast<float>(data.letterSpacing),
               to_raylib(data.textColor));
}

void ClayRenderer::begin_scissor(const Clay_RenderCommand& command) {
    const Clay_BoundingBox active = _scissors.push(command.boundingBox);
    EndScissorMode();
    BeginScissorMode(static_cast<int>(active.x), static_cast<int>(active.y),
                     static_cast<int>(active.width), static_cast<int>(active.height));
}

void ClayRenderer::end_scissor() {
    EndScissorMode();
    if (const auto restored = _scissors.pop()) {
        BeginScissorMode(static_cast<int>(restored->x), static_cast<int>(restored->y),
                         static_cast<int>(restored->width), static_cast<int>(restored->height));
    }
}

} // namespace editor
