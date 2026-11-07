#include "viewport/SceneItems.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ranges>
#include <string>

#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/math/Camera.hpp"
#include "engine/core/math/Transform.hpp"
#include "engine/physics/Components.hpp"
#include "ui/Labels.hpp"

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

void add_colliders(const EntityManager& world, Entity entity, SceneItem& item) {
    if (const auto* box = world.find<engine::physics::AABBCollider>(entity)) {
        item.box_collider = WorldBox{.top_left = item.top_left + box->offset, .size = box->size};
    }
    if (const auto* circle = world.find<engine::physics::CircleCollider>(entity)) {
        item.circle_collider =
            WorldCircle{.center = item.top_left + circle->offset, .radius = circle->radius};
    }
}

SceneItem item_of(const EntityManager& world, Entity entity,
                  const engine::editor::DocumentInstance& entry, Vec2 position,
                  const TextureSize& texture_size) {
    SceneItem item;
    item.id = entry.id;
    item.top_left = position;
    item.label = std::string(instance_label(entry.instance));
    if (const auto* scale = world.find<engine::core::Scale>(entity)) {
        item.scale = scale->factor;
        item.scales = true;
    }
    if (const auto* rotation = world.find<engine::core::Rotation>(entity)) {
        item.rotation = rotation->degrees;
        item.rotates = true;
    }
    add_colliders(world, entity, item);
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
        items.push_back(item_of(world, *entity, entry, top_left, texture_size));
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

WorldBox game_frame(const engine::editor::EditorSession& session) {
    const EntityManager& world = session.world();
    for (const auto& entry : session.document().instances) {
        const std::optional<Entity> entity = session.entity_of(entry.id);
        const auto* camera = entity ? world.find<engine::core::Camera>(*entity) : nullptr;
        const auto* position = entity ? world.find<engine::core::Position>(*entity) : nullptr;
        if (camera != nullptr && position != nullptr) {
            const engine::core::CameraView view =
                engine::core::camera_view(position->pos, *camera, game_screen);
            const Vec2 size = game_screen / view.zoom;
            return {.top_left = view.center - (size / 2.F), .size = size};
        }
    }
    return {.top_left = {0.F, 0.F}, .size = game_screen};
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
