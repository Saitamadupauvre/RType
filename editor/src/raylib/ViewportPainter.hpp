#pragma once

#include <clay.h>
#include <raylib.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "engine/core/math/Vec2.hpp"

namespace editor {

class ViewportPainter {
public:
    explicit ViewportPainter(std::filesystem::path assets);
    ~ViewportPainter();
    ViewportPainter(const ViewportPainter&) = delete;
    ViewportPainter& operator=(const ViewportPainter&) = delete;
    ViewportPainter(ViewportPainter&&) = delete;
    ViewportPainter& operator=(ViewportPainter&&) = delete;

    [[nodiscard]] std::optional<engine::core::Vec2> texture_size(std::string_view path);
    void paint(const Clay_RenderCommand& command);

private:
    [[nodiscard]] const Texture2D* texture(std::string_view path);

    std::filesystem::path _assets;
    std::unordered_map<std::string, std::optional<Texture2D>> _textures;
};

} // namespace editor
