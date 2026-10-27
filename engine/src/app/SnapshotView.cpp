#include "SnapshotView.hpp"

#include <stdexcept>
#include <utility>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/network/Replicated.hpp"

namespace rtype::client {

namespace {

constexpr float marker_radius = 6.F;
constexpr float marker_gap = 12.F;
constexpr engine::graphics::Color marker_color{.r = 255, .g = 255, .b = 255, .a = 255};

bool is_local(const engine::network::Replicated& replicated, std::uint32_t local_player) {
    return replicated.owner != 0 && replicated.owner == local_player;
}

} // namespace

SnapshotView::SnapshotView(engine::graphics::IRenderer& renderer, std::filesystem::path assets,
                           const engine::core::FieldRegistry& fields)
    : _renderer(renderer), _fields(fields), _draw(renderer, std::move(assets)) {
    _world.register_component<engine::core::Position>();
    _world.register_component<engine::core::Sprite>();
    _world.register_component<engine::network::Replicated>();
}

std::string SnapshotView::sprite_of(const std::string& type_name) {
    return "sprites/" + type_name + ".png";
}

void SnapshotView::draw(const std::vector<engine::network::EntitySnapshot>& entities,
                        const engine::network::StringTable& strings, std::uint32_t local_player) {
    mirror(entities, strings);
    _draw.draw(_world);
    mark_local(local_player);
}

void SnapshotView::mirror(const std::vector<engine::network::EntitySnapshot>& entities,
                          const engine::network::StringTable& strings) {
    for (const auto entity : _mirrored) {
        _world.kill(entity);
    }
    _mirrored.clear();
    for (const auto& snapshot : entities) {
        const std::string* name =
            strings.name_of(engine::network::StringCategory::type, snapshot.type_id);
        const auto entity = _world.spawn();
        _world.add(entity, engine::core::Sprite{.path = name == nullptr ? "" : sprite_of(*name),
                                                .color = {}});
        apply(entity, snapshot);
        _mirrored.push_back(entity);
    }
}

void SnapshotView::apply(engine::core::Entity entity,
                         const engine::network::EntitySnapshot& snapshot) {
    for (const auto& replicated : snapshot.fields) {
        if (!replicated.field.component->is_registered_in(_world)) {
            continue;
        }
        try {
            _fields.set(_world, entity, replicated.field, replicated.value);
        } catch (const std::invalid_argument& error) {
            engine::core::log::warn("client: entity {}: {}", snapshot.entity_id, error.what());
        } catch (const std::out_of_range& error) {
            engine::core::log::warn("client: entity {}: {}", snapshot.entity_id, error.what());
        }
    }
}

void SnapshotView::mark_local(std::uint32_t local_player) {
    _world.query<engine::network::Replicated, engine::core::Position>().each(
        [&](engine::core::Entity, const engine::network::Replicated& replicated,
            const engine::core::Position& position) {
            if (is_local(replicated, local_player)) {
                _renderer.draw_circle({position.pos.x, position.pos.y - marker_gap}, marker_radius,
                                      marker_color);
            }
        });
}

} // namespace rtype::client
