#include "SnapshotView.hpp"

#include <unordered_map>
#include <utility>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"

namespace rtype::client {

namespace {

constexpr float marker_radius = 6.F;
constexpr float marker_gap = 12.F;
constexpr engine::graphics::Color marker_color{.r = 255, .g = 255, .b = 255, .a = 255};

std::unordered_map<std::uint16_t, std::string>
names_by_id(const std::vector<engine::network::ReplicatedType>& types) {
    std::unordered_map<std::uint16_t, std::string> names;
    for (const auto& type : types) {
        names.emplace(type.type_id, type.name);
    }
    return names;
}

bool is_local(const engine::network::EntitySnapshot& entity, std::uint32_t local_player) {
    return entity.owner != 0 && entity.owner == local_player;
}

} // namespace

SnapshotView::SnapshotView(engine::graphics::IRenderer& renderer, std::filesystem::path assets)
    : _renderer(renderer), _draw(renderer, std::move(assets)) {
    _world.register_component<engine::core::Position>();
    _world.register_component<engine::core::Sprite>();
}

std::string SnapshotView::sprite_of(const std::string& type_name) {
    return "sprites/" + type_name + ".png";
}

void SnapshotView::draw(const std::vector<engine::network::EntitySnapshot>& entities,
                        const std::vector<engine::network::ReplicatedType>& types,
                        std::uint32_t local_player) {
    mirror(entities, types);
    _draw.draw(_world);
    mark_local(entities, local_player);
}

void SnapshotView::mirror(const std::vector<engine::network::EntitySnapshot>& entities,
                          const std::vector<engine::network::ReplicatedType>& types) {
    for (const auto entity : _mirrored) {
        _world.kill(entity);
    }
    _mirrored.clear();
    const auto names = names_by_id(types);
    for (const auto& snapshot : entities) {
        const auto name = names.find(snapshot.type_id);
        const auto entity = _world.spawn();
        _world.add(entity, engine::core::Position{snapshot.x, snapshot.y});
        _world.add(entity,
                   engine::core::Sprite{.path = name == names.end() ? "" : sprite_of(name->second),
                                        .color = snapshot.color});
        _mirrored.push_back(entity);
    }
}

void SnapshotView::mark_local(const std::vector<engine::network::EntitySnapshot>& entities,
                              std::uint32_t local_player) {
    for (const auto& entity : entities) {
        if (is_local(entity, local_player)) {
            _renderer.draw_circle({entity.x, entity.y - marker_gap}, marker_radius, marker_color);
        }
    }
}

} // namespace rtype::client
