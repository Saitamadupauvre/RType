#include "viewport/SceneItems.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ranges>

#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/math/Transform.hpp"

namespace editor {

namespace {

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Vec2;

Vec2 base_size(const engine::core::Sprite& sprite, const TextureSize& texture_size) {
    if (!sprite.path.empty() && texture_size) {
        if (const std::optional<Vec2> size = texture_size(sprite.path)) {
            return *size;
        }
    }
    return {square_size, square_size};
}

SceneItem item_of(const EntityManager& world, Entity entity, engine::editor::InstanceId id,
                  Vec2 position, const TextureSize& texture_size) {
    SceneItem item;
    item.id = id;
    item.top_left = position;
    if (const auto* scale = world.find<engine::core::Scale>(entity)) {
        item.scale = scale->factor;
    }
    if (const auto* rotation = world.find<engine::core::Rotation>(entity)) {
        item.rotation = rotation->degrees;
    }
    if (const auto* layer = world.find<engine::core::Layer>(entity)) {
        item.z_index = layer->z_index;
    }
    const auto* sprite = world.find<engine::core::Sprite>(entity);
    item.visible = sprite != nullptr;
    const engine::core::Sprite shown = sprite != nullptr ? *sprite : engine::core::Sprite{};
    item.tint = shown.color;
    item.texture = shown.path;
    const Vec2 size = base_size(shown, texture_size);
    item.size = {size.x * std::abs(item.scale.x), size.y * std::abs(item.scale.y)};
    return item;
}

bool holds(const SceneItem& item, Vec2 point) {
    return point.x >= item.top_left.x && point.x < item.top_left.x + item.size.x &&
           point.y >= item.top_left.y && point.y < item.top_left.y + item.size.y;
}

std::vector<float> lines_between(float first, float last, float grid) {
    std::vector<float> lines;
    const auto begin = static_cast<std::int64_t>(std::floor(first / grid));
    const auto end = static_cast<std::int64_t>(std::floor(last / grid));
    for (std::int64_t index = begin; index <= end; ++index) {
        lines.push_back(static_cast<float>(index) * grid);
    }
    return lines;
}

} // namespace

std::vector<SceneItem> scene_items(const engine::editor::EditorSession& session,
                                   const TextureSize& texture_size) {
    const EntityManager& world = session.world();
    std::vector<SceneItem> items;
    for (const auto& entry : session.document().instances) {
        const std::optional<Entity> entity = session.entity_of(entry.id);
        if (!entity) {
            continue;
        }
        const auto* position = world.find<engine::core::Position>(*entity);
        const Vec2 top_left = position != nullptr ? position->pos : entry.instance.position;
        items.push_back(item_of(world, *entity, entry.id, top_left, texture_size));
    }
    std::ranges::stable_sort(items, {}, &SceneItem::z_index);
    return items;
}

std::optional<engine::editor::InstanceId> item_at(const std::vector<SceneItem>& items,
                                                  Vec2 world) {
    for (const SceneItem& item : std::views::reverse(items)) {
        if (holds(item, world)) {
            return item.id;
        }
    }
    return std::nullopt;
}

GridLines grid_lines(const ViewCamera& camera, Clay_BoundingBox viewport, float grid) {
    if (grid <= 0.F || grid * camera.zoom < min_grid_spacing) {
        return {};
    }
    const Vec2 first = to_world(camera, viewport, {viewport.x, viewport.y});
    const Vec2 last =
        to_world(camera, viewport, {viewport.x + viewport.width, viewport.y + viewport.height});
    return {.xs = lines_between(first.x, last.x, grid), .ys = lines_between(first.y, last.y, grid)};
}

} // namespace editor
