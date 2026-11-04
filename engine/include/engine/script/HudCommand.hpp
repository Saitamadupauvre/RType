#pragma once

#include <cstddef>
#include <string>
#include <variant>

#include "engine/core/ecs/Components.hpp"
#include "engine/core/math/Vec2.hpp"

namespace engine::script {

/** @brief Most commands a client script can queue in one call of its `draw()`. */
inline constexpr std::size_t max_hud_commands = 1024;

/** @brief Longest text, in bytes, `draw.text` accepts. */
inline constexpr std::size_t max_hud_text_length = 256;

/** @brief `draw.text(text, x, y, size [, color])`: text whose top-left corner is at position. */
struct HudText {
    std::string text;
    /** @brief Screen position in virtual units. */
    core::Vec2 position;
    /** @brief Height of the text in virtual units, strictly positive. */
    float size{0.F};
    core::Rgba color;

    bool operator==(const HudText&) const = default;
};

/** @brief `draw.rect(x, y, w, h [, color [, filled]])`: rectangle from its top-left corner. */
struct HudRect {
    /** @brief Screen position of the top-left corner in virtual units. */
    core::Vec2 position;
    /** @brief Width and height in virtual units, never negative. */
    core::Vec2 size;
    core::Rgba color;
    /** @brief false draws only the outline. */
    bool filled{true};

    bool operator==(const HudRect&) const = default;
};

/**
 * @brief `draw.sprite(texture, x, y [, { scale, rotation, color }])`: texture whose top-left
 * corner is at position, at its own size times scale.
 */
struct HudSprite {
    /** @brief Texture declared with `textures { ... }`, relative to the assets folder. */
    std::string texture;
    /** @brief Screen position of the top-left corner in virtual units. */
    core::Vec2 position;
    /** @brief Factor on each axis; a negative factor mirrors the texture. */
    core::Vec2 scale{1.F, 1.F};
    /** @brief Degrees clockwise around the center of the drawn texture. */
    float rotation{0.F};
    /** @brief Tint multiplied with the texture; white keeps it unchanged. */
    core::Rgba color;

    bool operator==(const HudSprite&) const = default;
};

/**
 * @brief One screen-space drawing queued by a client script during its `draw()`.
 *
 * Positions are virtual screen units: the camera never moves or zooms them.
 */
using HudCommand = std::variant<HudText, HudRect, HudSprite>;

} // namespace engine::script
