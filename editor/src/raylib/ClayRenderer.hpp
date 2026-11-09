#pragma once

#include <clay.h>
#include <raylib.h>

#include <filesystem>
#include <functional>
#include <string_view>

#include "raylib/FontCache.hpp"
#include "render/ScissorStack.hpp"

namespace editor {

class ClayRenderer {
public:
    using CustomPainter = std::function<void(const Clay_RenderCommand&)>;

    explicit ClayRenderer(std::filesystem::path font_directory);

    void set_custom_painter(CustomPainter painter);
    /** @brief Draws Clay image elements; without one they are skipped. */
    void set_image_painter(CustomPainter painter);

    /** @brief The editor font of `face` at `size` pixels, loaded on first use. */
    [[nodiscard]] const Font& font(Face face, std::uint16_t size);

    [[nodiscard]] Clay_Dimensions measure(std::string_view text,
                                          const Clay_TextElementConfig& config);
    void render(Clay_RenderCommandArray commands);

private:
    const Font& font_for(std::uint16_t font_id, std::uint16_t size);
    void draw_rectangle(const Clay_RenderCommand& command) const;
    void draw_border(const Clay_RenderCommand& command) const;
    void draw_text(const Clay_RenderCommand& command);
    void begin_scissor(const Clay_RenderCommand& command);
    void end_scissor();

    FontCache _fonts;
    CustomPainter _custom;
    CustomPainter _image;
    ScissorStack _scissors;
};

} // namespace editor
