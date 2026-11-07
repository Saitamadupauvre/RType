#pragma once

#include <clay.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/ecs/Components.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/editor/EditorSession.hpp"
#include "engine/editor/SceneDocument.hpp"
#include "viewport/ViewCamera.hpp"

namespace editor {

/** @brief Size of an entity drawn without a texture, in world units, as the game draws it. */
inline constexpr float square_size = 64.F;

/** @brief Size of the game's drawing space in virtual units, as the graphics plugin sets it. */
inline constexpr engine::core::Vec2 game_screen{1920.F, 1080.F};

/** @brief An axis-aligned box in world units. */
struct WorldBox {
    engine::core::Vec2 top_left;
    engine::core::Vec2 size;

    bool operator==(const WorldBox&) const = default;
};

/** @brief A circle in world units. */
struct WorldCircle {
    engine::core::Vec2 center;
    float radius = 0.F;

    bool operator==(const WorldCircle&) const = default;
};

/** @brief Size of a texture in pixels, or std::nullopt when it cannot be loaded. */
using TextureSize = std::function<std::optional<engine::core::Vec2>(std::string_view path)>;

/** @brief One instance as the viewport draws it, laid out like the game's world renderer. */
struct SceneItem {
    engine::editor::InstanceId id;
    engine::core::Vec2 top_left;
    /** @brief Drawn size, always positive: the texture or square size times |scale|. */
    engine::core::Vec2 size;
    /** @brief Scale factor; a negative one mirrors the texture. */
    engine::core::Vec2 scale{1.F, 1.F};
    float rotation = 0.F;
    engine::core::Rgba tint;
    /** @brief Texture path relative to the assets folder; empty draws a tinted square. */
    std::string texture;
    /** @brief false for an entity without a Sprite, which the game does not draw. */
    bool visible = true;
    std::int32_t z_index = 0;
    /** @brief The instance name, or its type when it has none. */
    std::string label;
    /** @brief Whether the entity has a Rotation or a Scale the handles can edit. */
    bool rotates = false;
    bool scales = false;
    /** @brief As the physics reads it: Position plus the offset is the center of the box. */
    std::optional<WorldBox> box_collider;
    std::optional<WorldCircle> circle_collider;

    bool operator==(const SceneItem&) const = default;
};

/**
 * @brief Lays out every instance of the open scene in drawing order: by z_index, then in scene
 * order.
 * @param texture_size Sizes sprites; an unknown texture is laid out as a square.
 */
[[nodiscard]] std::vector<SceneItem> scene_items(const engine::editor::EditorSession& session,
                                                 const TextureSize& texture_size);

/** @brief The topmost item whose box holds `world`, ignoring rotation. */
[[nodiscard]] std::optional<engine::editor::InstanceId>
item_at(const std::vector<SceneItem>& items, engine::core::Vec2 world);

/**
 * @brief The world area the game shows when the scene starts: the view of the first instance
 * with a Camera, or the default view of the game screen without one.
 */
[[nodiscard]] WorldBox game_frame(const engine::editor::EditorSession& session);

/** @brief World coordinates of the grid lines visible in the viewport. */
struct GridLines {
    std::vector<float> xs;
    std::vector<float> ys;
};

/** @brief Grid lines closer than this on screen are not drawn. */
inline constexpr float min_grid_spacing = 6.F;

/** @brief The visible grid lines, or none when the grid is too dense on screen. */
[[nodiscard]] GridLines grid_lines(const ViewCamera& camera, Clay_BoundingBox viewport, float grid);

} // namespace editor
