#pragma once

#include <clay.h>
#include <raylib.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "engine/core/math/Vec2.hpp"

namespace editor {

class ViewportPainter {
public:
    /** @brief Gives the editor font at a pixel size, so labels match the rest of the editor. */
    using FontAt = std::function<const Font&(std::uint16_t size)>;

    ViewportPainter(std::filesystem::path assets, FontAt font);
    ~ViewportPainter();
    ViewportPainter(const ViewportPainter&) = delete;
    ViewportPainter& operator=(const ViewportPainter&) = delete;
    ViewportPainter(ViewportPainter&&) = delete;
    ViewportPainter& operator=(ViewportPainter&&) = delete;

    [[nodiscard]] std::optional<engine::core::Vec2> texture_size(std::string_view path);
    void paint(const Clay_RenderCommand& command);
    /** @brief Draws the texture of the AssetEntry an image element points at, fitted and centred.
     */
    void paint_thumbnail(const Clay_RenderCommand& command);

private:
    [[nodiscard]] const Texture2D* texture(std::string_view path);

    std::filesystem::path _assets;
    FontAt _font;
    std::unordered_map<std::string, std::optional<Texture2D>> _textures;
};

} // namespace editor
