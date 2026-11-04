#pragma once

#include <vector>

#include "assets/TextureLibrary.hpp"
#include "engine/app/Export.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/script/HudCommand.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT HudDraw {
public:
    HudDraw(graphics::IRenderer& renderer, TextureLibrary& textures);

    void draw(const std::vector<script::HudCommand>& commands);

private:
    void draw_one(const script::HudText& text);
    void draw_one(const script::HudRect& rect);
    void draw_one(const script::HudSprite& sprite);

    graphics::IRenderer& _renderer;
    TextureLibrary& _textures;
};

} // namespace engine::app
